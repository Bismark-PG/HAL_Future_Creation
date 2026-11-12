#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../HALBlackHole.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../TestVehiclePawn.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

namespace
{
	class FBlackHoleNetworkProbe : public IAutomationLatentCommand
	{
		FAutomationTestBase& Test;
		double Deadline = FPlatformTime::Seconds() + 90;
		double CompleteAt = -1;
		float SpawnAfter = -1;
		bool bPrepared = false, bAligned = false, bSpawned = false, bSawField = false, bSawExpansion = false, bSawHold = false;
		float PeakInwardSpeed = 0;
		bool bSawEntrySlowdown = false;
		TWeakObjectPtr<AHALBlackHole> Observed;
	public:
		explicit FBlackHoleNetworkProbe(FAutomationTestBase& InTest) : Test(InTest) {}
		bool Update() override
		{
			if (CompleteAt > 0) { return FPlatformTime::Seconds() >= CompleteAt; }
			if (FPlatformTime::Seconds() > Deadline)
			{
				Test.AddError(FString::Printf(TEXT("Black hole paired NetTest timeout. Prepared=%d Aligned=%d Spawned=%d Seen=%d Expand=%d Hold=%d Peak=%.1f"),
					bPrepared, bAligned, bSpawned, bSawField, bSawExpansion, bSawHold, PeakInwardSpeed));
				return true;
			}
			UWorld* World = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
			{
				auto* W = Context.World();
				if (W && W->IsGameWorld() && (W->IsNetMode(NM_Client) || W->IsNetMode(NM_ListenServer))) { World = W; break; }
			}
			if (!World) { return false; }
			const bool bHost = World->IsNetMode(NM_ListenServer);
			auto* PC = Cast<AHALPlayerController>(World->GetFirstPlayerController());
			auto* Own = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			ATestVehiclePawn* Other = nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World); It; ++It) { if (*It != Own && It->GetPlayerState()) { Other = *It; break; } }
			auto* Match = World->GetGameState<AHALMatchGameState>();
			if (!Own || !Other || !Match || !Other->GetPlayerState()) { return false; }
			if (!bPrepared)
			{
				for (auto* Pawn : {Own, Other})
				{
					auto* Body = CastChecked<UPrimitiveComponent>(Pawn->GetRootComponent());
					Body->SetEnableGravity(false); Body->SetLinearDamping(0);
				}
				PC->SetName(bHost ? TEXT("HAL_BlackHole_Host") : TEXT("HAL_BlackHole_Client")); bPrepared = true;
			}
			if (Match->GetMatchPhase() != EHALMatchPhase::Playing)
			{
				if (bHost && Other->GetPlayerState()->GetPlayerName() == TEXT("HAL_BlackHole_Client")) { PC->HALStartMatch(); }
				return false;
			}
			if (bHost && !bAligned)
			{
				FPacketSimulationSettings Lag; Lag.PktLag = 80; World->GetNetDriver()->SetPacketSimulationSettings(Lag);
				for (auto* Pawn : {Own, Other})
				{
					auto* Body = CastChecked<UPrimitiveComponent>(Pawn->GetRootComponent());
					Body->SetWorldLocationAndRotation(FVector(0, Pawn == Own ? 1000 : -1000, 10000), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
					Body->SetPhysicsLinearVelocity(FVector::ZeroVector); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
					Pawn->ForceNetUpdate();
				}
				SpawnAfter = World->GetTimeSeconds() + 2; bAligned = true;
			}
			if (bHost && !bSpawned && World->GetTimeSeconds() >= SpawnAfter)
			{
				for (auto* Pawn : {Own, Other})
				{
					auto* Body = CastChecked<UPrimitiveComponent>(Pawn->GetRootComponent());
					Body->SetPhysicsLinearVelocity(-Pawn->GetActorLocation().GetSafeNormal2D() * 500.);
				}
				const FTransform Transform(FVector(0,0,10000));
				auto* Hole = World->SpawnActorDeferred<AHALBlackHole>(AHALBlackHole::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				FHALBlackHoleConfig Config; Config.ExpansionSeconds = 1; Config.DurationSeconds = 4;
				Config.MaximumRadius = 2500; Config.ContinuousAcceleration = 40; Config.PulseDeltaSpeed = 400; Config.PulseInterval = 1;
				Test.TestTrue(TEXT("Server configures deferred field"), Hole->ConfigureBlackHole(Config));
				Hole->FinishSpawning(Transform); bSpawned = true;
			}
			for (TActorIterator<AHALBlackHole> It(World); It; ++It)
			{
				// Team maps may contain long-lived Blueprint fields. Observe only this native fixture's volume.
				if (It->GetClass() != AHALBlackHole::StaticClass() || !It->GetActorLocation().Equals(FVector(0,0,10000),.1)) { continue; }
				Observed = *It; bSawField = true;
				bSawExpansion |= It->GetStage() == EHALBlackHoleStage::Expanding;
				bSawHold |= It->GetStage() == EHALBlackHoleStage::PreparingPulse && FMath::IsNearlyEqual(It->GetCurrentRadius(),2500.f);
				if (It->GetStage() == EHALBlackHoleStage::Expanding && Own->GetActorLocation().Size2D() <= It->GetCurrentRadius())
				{
					const FVector Direction = -Own->GetActorLocation().GetSafeNormal2D();
					const double Inward = FVector::DotProduct(Direction, CastChecked<UPrimitiveComponent>(Own->GetRootComponent())->GetPhysicsLinearVelocity());
					bSawEntrySlowdown |= Inward > 50 && Inward < 350;
				}
			}
			if (bSawField)
			{
				const FVector Direction = -Own->GetActorLocation().GetSafeNormal2D();
				PeakInwardSpeed = FMath::Max(PeakInwardSpeed, float(FVector::DotProduct(Direction, CastChecked<UPrimitiveComponent>(Own->GetRootComponent())->GetPhysicsLinearVelocity())));
			}
			if (bSawField && !Observed.IsValid())
			{
				Test.TestTrue(TEXT("Synchronized expansion observed"), bSawExpansion);
				Test.TestTrue(TEXT("Authority entry slowdown observed on local owned moving vehicle"), bSawEntrySlowdown);
				Test.TestTrue(TEXT("Replicated maximum range and hold observed"), bSawHold);
				Test.TestTrue(TEXT("Local owned vehicle pulled by authority physics actions"), PeakInwardSpeed > 200);
				bool bFixtureRemains = false;
				for (TActorIterator<AHALBlackHole> It(World); It; ++It)
				{ bFixtureRemains |= It->GetClass() == AHALBlackHole::StaticClass() && It->GetActorLocation().Equals(FVector(0,0,10000),.1); }
				Test.TestFalse(TEXT("No local duplicate fixture black hole remains"), bFixtureRemains);
				UE_LOG(LogTemp, Display, TEXT("BlackHole network Host=%d PeakInwardSpeed=%.2f Resimulations=%d"), bHost,
					PeakInwardSpeed, Own->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetResimulationRunCount());
				CompleteAt = FPlatformTime::Seconds() + (bHost ? 6 : 2);
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBlackHoleNetworkTest, "HAL.NetworkSmoke.BlackHole.AttractionLifecycle",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALBlackHoleNetworkTest::RunTest(const FString&)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FBlackHoleNetworkProbe>(*this));
	return true;
}
#endif
