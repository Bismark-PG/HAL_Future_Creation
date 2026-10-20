#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../SpringPunchGimmick.h"
#include "../TestVehiclePawn.h"
#include "../HALPlayerController.h"
#include "../HALMatchGameState.h"
#include "../HALImpactReceiverComponent.h"
#include "../VehicleHealthComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

/** Opt-in two-process test. Runtime actors only; no saved map or Blueprint edits. */
class FHALSpringPunchLiveTest : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	double Deadline = FPlatformTime::Seconds() + 70;
	double SpawnedAt = -1, CompletedAt = -1;
	bool bSawExtend = false, bSawRetract = false;
	TSet<int32> MediumTargets;
	TSet<TWeakObjectPtr<ASpringPunchGimmick>> Extended, Retracted;
	FString Diagnostics;
public:
	explicit FHALSpringPunchLiveTest(FAutomationTestBase* InTest) : Test(InTest) {}
	bool Update() override
	{
		if (CompletedAt > 0) { return FPlatformTime::Seconds() - CompletedAt > 3; }
		if (FPlatformTime::Seconds() > Deadline) { Test->AddError(TEXT("SpringPunch network timeout: ") + Diagnostics); return true; }
		UWorld* World = nullptr;
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			auto* W = Context.World();
			if (W && W->IsGameWorld() && (W->GetNetMode() == NM_ListenServer || W->GetNetMode() == NM_Client)) { World = W; break; }
		}
		if (!World) { return false; }
		const bool bHost = World->GetNetMode() == NM_ListenServer;
		AHALPlayerController* PC = nullptr;
		for (auto It = World->GetPlayerControllerIterator(); It; ++It)
		{
			auto* Candidate = Cast<AHALPlayerController>(It->Get());
			if (Candidate && Candidate->IsLocalController()) { PC = Candidate; break; }
		}
		TArray<ATestVehiclePawn*> Vehicles;
		for (TActorIterator<ATestVehiclePawn> It(World); It; ++It)
		{
			if (It->GetPlayerState() && It->FindComponentByClass<UVehicleHealthComponent>()->IsHealthReady()) { Vehicles.Add(*It); }
		}
		if (!PC || Vehicles.Num() != 2) { return false; }
		for (auto* V : Vehicles)
		{
			if (V->FindComponentByClass<UHALImpactReceiverComponent>()->GetLastImpact().EffectiveTier == EHALImpactTier::Medium)
			{
				MediumTargets.Add(V->GetPlayerState()->GetPlayerId());
			}
		}
		auto* Match = World->GetGameState<AHALMatchGameState>();
		if (!Match) { return false; }
		if (!bHost && SpawnedAt < 0) { PC->SetName(TEXT("HAL_PunchReady")); SpawnedAt = World->GetTimeSeconds(); }
		if (bHost && SpawnedAt < 0)
		{
			bool bReady = false;
			for (auto* V : Vehicles) { bReady |= V->GetPlayerState()->GetPlayerName() == TEXT("HAL_PunchReady"); }
			if (!bReady) { return false; }
			PC->HALStartMatch();
			if (Match->GetMatchPhase() != EHALMatchPhase::Playing) { return false; }
			int32 Index = 0;
			for (auto* V : Vehicles)
			{
				const auto* Body = CastChecked<UPrimitiveComponent>(V->GetRootComponent());
				const FTransform Transform(FRotator::ZeroRotator, Body->Bounds.Origin - FVector(200, 0, 0));
				UClass* PunchClass = Index++ == 0 ? ASpringPunchGimmick::StaticClass()
					: LoadClass<ASpringPunchGimmick>(nullptr, TEXT("/Game/Blueprints/MapMechanic/BP_SpringPunch.BP_SpringPunch_C"));
				if (!PunchClass) { Test->AddError(TEXT("Saved spring punch Blueprint unavailable")); return true; }
				auto* Punch = World->SpawnActorDeferred<ASpringPunchGimmick>(PunchClass, Transform);
				// Exercise compatibility with the old saved Blueprint nonreplicated flag.
				Punch->SetReplicates(false);
				Punch->bAlwaysRelevant = true;
				Punch->BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Punch->BaseDamage = 25;
				Punch->ImpactTier = EHALImpactTier::Medium;
				Punch->PunchDistance = 400;
				Punch->InitialDelay = 1;
				Punch->WindupDuration = 1;
				Punch->ExtendDuration = 1;
				Punch->RetractDuration = 1;
				Punch->CooldownDuration = 60;
				Punch->FinishSpawning(Transform);
			}
			SpawnedAt = World->GetTimeSeconds();
		}
		int32 Count = 0;
		bool bAtRest = true;
		for (TActorIterator<ASpringPunchGimmick> It(World); It; ++It)
		{
			++Count;
			bSawExtend |= It->Phase.State == ESpringPunchState::Extending;
			bSawRetract |= It->Phase.State == ESpringPunchState::Retracting;
			if (It->Phase.State == ESpringPunchState::Extending) { Extended.Add(*It); }
			if (It->Phase.State == ESpringPunchState::Retracting) { Retracted.Add(*It); }
			bAtRest &= It->Phase.State == ESpringPunchState::Cooldown && It->Phase.PunchNumber == 1;
			if (!bHost && It->PunchAction.IsActive()) { Test->AddError(TEXT("Client must never create a combat action")); return true; }
		}
		Diagnostics = FString::Printf(TEXT("Punches=%d Extended=%d Retracted=%d MediumTargets=%d Rest=%d HP=%.1f/%.1f"),
			Count, Extended.Num(), Retracted.Num(), MediumTargets.Num(), bAtRest,
			Vehicles[0]->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(), Vehicles[1]->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP());
		if (Count != 2 || !bAtRest || !bSawExtend || !bSawRetract || Extended.Num() != 2 || Retracted.Num() != 2) { return false; }
		bool bHPReady = true;
		for (auto* V : Vehicles)
		{
			bHPReady &= FMath::IsNearlyEqual(V->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(), 75.f);
		}
		if (!bHPReady || MediumTargets.Num() != 2) { return false; }
		if (bHost)
		{
			bool bAcknowledged = false;
			for (auto* V : Vehicles) { bAcknowledged |= V->GetPlayerState()->GetPlayerName() == TEXT("HAL_PunchDone"); }
			if (!bAcknowledged) { return false; }
		}
		for (auto* V : Vehicles)
		{
			Test->TestEqual(TEXT("Both targets lose exactly 25 HP"), V->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(), 75.f);
			Test->TestTrue(TEXT("Both targets received replicated Medium tier"), MediumTargets.Contains(V->GetPlayerState()->GetPlayerId()));
		}
		if (!bHost) { PC->SetName(TEXT("HAL_PunchDone")); }
		UE_LOG(LogTemp, Display, TEXT("HAL SpringPunch network: Role=%s Complete=1 Punches=2 SawExtend=1 SawRetract=1 HP=75/75"), bHost ? TEXT("Host") : TEXT("Client"));
		CompletedAt = FPlatformTime::Seconds();
		return false;
	}
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSpringPunchNetworkTest, "HAL.NetworkSmoke.SpringPunch",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALSpringPunchNetworkTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHALSpringPunchLiveTest>(this));
	return true;
}
#endif
