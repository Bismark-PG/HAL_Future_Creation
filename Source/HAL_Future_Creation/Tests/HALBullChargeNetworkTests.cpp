#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../HALBullChargeComponent.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALGameplayTags.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../TestVehiclePawn.h"
#include "../BasicBallActor.h"
#include "../HALBullChargeAbility.h"
#include "../HALDebugSettings.h"
#include "Components/SphereComponent.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	class FHALBullChargeLive : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline = FPlatformTime::Seconds() + 80;
		double RequestedAt = -1, CompleteAt = -1;
		bool bBothActive = false, bStunSent = false, bSawStun = false, bResumed = false, bSawRecovery = false, bExpired = false;
		bool bSawAutoDrive = false, bSawRaisedLimit = false;
		FGuid Activation;
	public:
		explicit FHALBullChargeLive(FAutomationTestBase* InTest) : Test(InTest) {}
		bool Update() override
		{
			if (CompleteAt >= 0) { return FPlatformTime::Seconds() - CompleteAt > 3; }
			if (FPlatformTime::Seconds() > Deadline) { Test->AddError(TEXT("BullCharge live test timed out; use two processes and HALBullChargeFixture.")); return true; }
			UWorld* World = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
			{
				if (auto* W = Context.World(); W && W->IsGameWorld() && (W->GetNetMode() == NM_Client || W->GetNetMode() == NM_ListenServer)) { World = W; break; }
			}
			if (!World) { return false; }
			auto* Match = World->GetGameState<AHALMatchGameState>();
			AHALPlayerController* PC = nullptr;
			for (auto It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (auto* Candidate = Cast<AHALPlayerController>(It->Get()); Candidate && Candidate->IsLocalController()) { PC = Candidate; break; }
			}
			auto* Pawn = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			auto* ASC = Pawn ? Cast<UHALAbilitySystemComponent>(Pawn->GetAbilitySystemComponent()) : nullptr;
			auto* Charge = Pawn ? Pawn->FindComponentByClass<UHALBullChargeComponent>() : nullptr;
			ATestVehiclePawn* Other = nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World); It; ++It) { if (*It != Pawn && It->GetAbilitySystemComponent()) { Other = *It; break; } }
			auto* OtherASC = Other ? Cast<UHALAbilitySystemComponent>(Other->GetAbilitySystemComponent()) : nullptr;
			if (!Match || !PC || !ASC || !Charge || (!OtherASC && !bExpired) || !ASC->IsHealthInitialized()
				|| !ASC->HasAbilityForInput(HALGameplayTags::Input_Ability_Primary)) { return false; }
			const bool bHost = World->GetNetMode() == NM_ListenServer;
			if (bHost && Match->GetMatchPhase() == EHALMatchPhase::WaitingForPlayers) { PC->HALStartMatch(); }
			if (Match->GetMatchPhase() != EHALMatchPhase::Playing) { return false; }
			auto* Movement = Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>();
			const auto Status = Movement->GetMovementEffectStatus();
			const auto& Motion = Status.Definition.Overdrive;
			if (Motion.IsDriving(Status.ServerFrame) && !Status.Value.bControlsBlocked)
			{
				const auto Contact = Movement->GetContactObservation();
				bSawAutoDrive |= Contact.bValid && FVector::DotProduct(Contact.Velocity, Contact.Rotation.GetForwardVector().GetSafeNormal2D()) > 100;
				bSawRaisedLimit |= Status.Value.SpeedScale > 1.2f;
			}
			if (RequestedAt < 0)
			{
				if (!Status.bReady || (bHost && (!OtherASC->HasMatchingGameplayTag(HALGameplayTags::State_BullCharge) || !Charge->CanStart()))) { return false; }
				PC->HALUseSkill(); PC->HALUseSkill();
				RequestedAt = World->GetTimeSeconds();
				return false;
			}
			if (!bBothActive && ASC->HasMatchingGameplayTag(HALGameplayTags::State_BullCharge)
				&& OtherASC->HasMatchingGameplayTag(HALGameplayTags::State_BullCharge) && Charge->IsActive()
				&& Other->FindComponentByClass<UHALBullChargeComponent>()->IsActive() && Status.bReady && Motion.IsDriving(Status.ServerFrame))
			{
				bBothActive = true; Activation = Motion.Activation;
				Test->TestTrue(TEXT("Owner receives impact immunity"), ASC->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune));
				Test->TestEqual(TEXT("Duplicate requests create one state and one cooldown"), ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), 2);
			}
			if (bBothActive && bHost && !bStunSent && World->GetTimeSeconds() - RequestedAt > 1)
			{
				Test->TestTrue(TEXT("Authority can stun charging host"), ASC->ApplyStun(.6f, Other));
				Test->TestTrue(TEXT("Authority can stun charging client"), OtherASC->ApplyStun(.6f, Pawn));
				bStunSent = true;
			}
			if (bBothActive && Motion.bStunned && Status.Value.bControlsBlocked)
			{
				bSawStun = true;
				Test->TestTrue(TEXT("Stun retains activation identity"), Motion.Activation == Activation && Charge->IsActive());
			}
			if (bSawStun && !Motion.bStunned && Motion.IsDriving(Status.ServerFrame)) { bResumed = true; }
			if (Motion.IsRecovering(Status.ServerFrame)) { bSawRecovery = true; }
			if (bBothActive && !bExpired && !ASC->HasMatchingGameplayTag(HALGameplayTags::State_BullCharge)
				&& !OtherASC->HasMatchingGameplayTag(HALGameplayTags::State_BullCharge) && !Charge->IsActive()
				&& !Motion.IsDriving(Status.ServerFrame))
			{
				bExpired = true;
				Test->TestTrue(TEXT("Stun preserved skill and subsequently resumed"), bSawStun && bResumed);
				Test->TestTrue(TEXT("Normal entry point automatically accelerates the actual truck"), bSawAutoDrive);
				Test->TestTrue(TEXT("Physics consumes the increased speed limit"), bSawRaisedLimit);
				Test->TestTrue(TEXT("Gameplay ends before cooldown"), ASC->HasMatchingGameplayTag(HALGameplayTags::Cooldown_BullCharge));
				Test->TestFalse(TEXT("Expiry removes charge immunity"), ASC->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune));
			}
			if (!bExpired || ASC->HasMatchingGameplayTag(HALGameplayTags::Cooldown_BullCharge)) { return false; }
			if (CompleteAt < 0)
			{
				Test->TestTrue(TEXT("Recovery was replayable and observed"), bSawRecovery);
				Test->TestTrue(TEXT("Owner motion is ready after expiry"), Status.bReady);
				if (bHost) { Test->TestTrue(TEXT("Cooldown counts from activation"), World->GetTimeSeconds() - RequestedAt >= 19.5); }
				UE_LOG(LogTemp, Display, TEXT("HAL BullCharge live Role=%s Active=1 Stun=1 Resume=1 Recovery=%d Ready=%d Replays=%d Rewinds=%d"),
					bHost ? TEXT("Host") : TEXT("Client"), bSawRecovery, Status.bReady, Movement->GetResimulationRunCount(), Status.RewindRequests);
				CompleteAt = FPlatformTime::Seconds();
			}
			return !bHost || FPlatformTime::Seconds() - CompleteAt > 3;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBullChargeLiveTest, "HAL.NetworkSmoke.BullCharge.MotionStunAndLifetime",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALBullChargeLiveTest::RunTest(const FString&)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHALBullChargeLive>(this));
	return true;
}

namespace
{
	// Real world/Chaos contact: no injected collision samples or calls to HandleFreeBall.
	class FHALBullChargeBallContactLive : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline = FPlatformTime::Seconds() + 55, RequestedAt = -1, LaunchedAt = -1;
		TWeakObjectPtr<ABasicBallActor> Ball;
		float PeakBallSpeed = 0;
	public:
		explicit FHALBullChargeBallContactLive(FAutomationTestBase* InTest) : Test(InTest) {}
		bool Update() override
		{
			if (FPlatformTime::Seconds() > Deadline) { Test->AddError(TEXT("Real charge/ball contact timed out.")); return true; }
			UWorld* World = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
			{
				if (auto* W = Context.World(); W && W->IsGameWorld() && W->GetNetMode() == NM_ListenServer) { World = W; break; }
			}
			if (!World) { return false; }
			auto* PC = Cast<AHALPlayerController>(World->GetFirstPlayerController());
			auto* Pawn = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			auto* Charge = Pawn ? Pawn->FindComponentByClass<UHALBullChargeComponent>() : nullptr;
			auto* Match = World->GetGameState<AHALMatchGameState>();
			if (!Charge || !Match) { return false; }
			if (Match->GetMatchPhase() == EHALMatchPhase::WaitingForPlayers) { PC->HALStartMatch(); return false; }
			if (Match->GetMatchPhase() != EHALMatchPhase::Playing) { return false; }
			if (RequestedAt < 0)
			{
				if (!Charge->CanStart()) { return false; }
				for (TActorIterator<ABasicBallActor> It(World); It; ++It) { It->Destroy(); }
				PC->HALUseSkill();
				if (!Test->TestTrue(TEXT("Actual saved Car04 skill activates"), Charge->IsActive())) { return true; }
				const FVector Forward = Pawn->GetActorForwardVector().GetSafeNormal2D();
				auto* Root = CastChecked<UPrimitiveComponent>(Pawn->GetRootComponent());
				FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Params.Name = TEXT("HALChargeContactBall");
				auto* Class = LoadClass<ABasicBallActor>(nullptr, TEXT("/Game/Blueprints/Balls/BP_BasicBall.BP_BasicBall_C"));
				FVector Position = Pawn->GetActorLocation() + Forward * (Root->Bounds.BoxExtent.Size2D() + 700);
				Position.Z = Root->Bounds.Origin.Z - Root->Bounds.BoxExtent.Z + 55;
				Ball = World->SpawnActor<ABasicBallActor>(Class, Position, FRotator::ZeroRotator, Params);
				if (!Test->TestTrue(TEXT("Actual configured ball is valid"), Ball.IsValid() && Ball->IsConfigurationValid())) { return true; }
				RequestedAt = World->GetTimeSeconds();
				UE_LOG(LogTemp, Display, TEXT("HAL BullCharge contact fixture Pawn=%s Position=%s Ball=%s Position=%s Forward=%s"),
					*Pawn->GetName(), *Pawn->GetActorLocation().ToCompactString(), *Ball->GetName(), *Position.ToCompactString(), *Forward.ToCompactString());
				return false;
			}
			if (!Ball.IsValid()) { Test->AddError(TEXT("Contact test ball was destroyed.")); return true; }
			PeakBallSpeed = FMath::Max(PeakBallSpeed, float(Ball->GetPhysicsRoot()->GetPhysicsLinearVelocity().Size()));
			if (LaunchedAt < 0 && Ball->GetBallState() == EBasicBallState::Launched && Ball->GetLaunchedBy() == Pawn)
			{
				LaunchedAt = World->GetTimeSeconds();
				Test->TestTrue(TEXT("Real contact applies a fast launch rather than natural pushing"), PeakBallSpeed > 4500);
				UE_LOG(LogTemp, Display, TEXT("HAL BullCharge real contact Launched=1 PeakBallSpeed=%.2f VehicleSpeed=%.2f Delay=%.3f"),
					PeakBallSpeed, CastChecked<UPrimitiveComponent>(Pawn->GetRootComponent())->GetPhysicsLinearVelocity().Size(), LaunchedAt - RequestedAt);
			}
			if (LaunchedAt >= 0) { return World->GetTimeSeconds() - LaunchedAt > .5; }
			if (World->GetTimeSeconds() - RequestedAt > 7)
			{
				Test->AddError(FString::Printf(TEXT("Real frontal contact never launched: State=%d PeakBallSpeed=%.2f Active=%d"), int32(Ball->GetBallState()), PeakBallSpeed, Charge->IsActive()));
				return true;
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBullChargeBallContactLiveTest, "HAL.NetworkSmoke.BullCharge.RealBallContact",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALBullChargeBallContactLiveTest::RunTest(const FString&)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHALBullChargeBallContactLive>(this));
	return true;
}

namespace
{
	class FHALBullChargeBallBothOwners : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline = FPlatformTime::Seconds() + 75, CompleteAt = -1;
		bool bSpawned = false, bRequested = false;
		TSet<FName> SawLaunch;
	public:
		explicit FHALBullChargeBallBothOwners(FAutomationTestBase* InTest) : Test(InTest) {}
		bool Update() override
		{
			if (CompleteAt >= 0) { return FPlatformTime::Seconds() - CompleteAt > 3; }
			if (FPlatformTime::Seconds() > Deadline) { Test->AddError(TEXT("Both-owner real ball contact timed out; requires two processes and saved Car04 skill configuration.")); return true; }
			UWorld* World = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
			{
				if (auto* W = Context.World(); W && W->IsGameWorld() && (W->GetNetMode() == NM_Client || W->GetNetMode() == NM_ListenServer)) { World = W; break; }
			}
			if (!World) { return false; }
			const bool bHost = World->GetNetMode() == NM_ListenServer;
			AHALPlayerController* PC = nullptr;
			for (auto It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (auto* Candidate = Cast<AHALPlayerController>(It->Get()); Candidate && Candidate->IsLocalController()) { PC = Candidate; break; }
			}
			auto* Pawn = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			auto* Charge = Pawn ? Pawn->FindComponentByClass<UHALBullChargeComponent>() : nullptr;
			auto* Match = World->GetGameState<AHALMatchGameState>();
			TArray<ATestVehiclePawn*> Players;
			for (TActorIterator<ATestVehiclePawn> It(World); It; ++It)
			{
				auto* ASC = Cast<UHALAbilitySystemComponent>(It->GetAbilitySystemComponent());
				// Only the server can inspect every player's granted ability specs.
				if (ASC && (!bHost || (ASC->IsCombatInitialized() && ASC->HasAbilityForInput(HALGameplayTags::Input_Ability_Primary)))) { Players.Add(*It); }
			}
			if (!PC || !Charge || !Match || Players.Num() != 2) { return false; }
			if (bHost && Match->GetMatchPhase() == EHALMatchPhase::WaitingForPlayers) { PC->HALStartMatch(); return false; }
			if (Match->GetMatchPhase() != EHALMatchPhase::Playing) { return false; }
			if (bHost && !bSpawned)
			{
				for (auto* Player : Players) { if (!Player->FindComponentByClass<UHALBullChargeComponent>()->CanStart()) { return false; } }
				for (TActorIterator<ABasicBallActor> It(World); It; ++It) { It->Destroy(); }
				auto* Class = LoadClass<ABasicBallActor>(nullptr, TEXT("/Game/Blueprints/Balls/BP_BasicBall.BP_BasicBall_C"));
				for (auto* Player : Players)
				{
					auto* Root = CastChecked<UPrimitiveComponent>(Player->GetRootComponent());
					FVector Position = Player->GetActorLocation() + Player->GetActorForwardVector().GetSafeNormal2D() * (Root->Bounds.BoxExtent.Size2D() + 700);
					Position.Z = Root->Bounds.Origin.Z - Root->Bounds.BoxExtent.Z + 55;
					FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					Params.Owner = Player; Params.Name = Player == Pawn ? TEXT("HALBothContact_Host") : TEXT("HALBothContact_Client");
					auto* Ball = World->SpawnActor<ABasicBallActor>(Class, Position, FRotator::ZeroRotator, Params);
					if (!Test->TestTrue(TEXT("Both owners get a real configured Free ball"), Ball && Ball->IsConfigurationValid())) { return true; }
				}
				bSpawned = true;
			}
			for (TActorIterator<ABasicBallActor> It(World); It; ++It)
			{
				// Dynamic actor names are generated locally; use replicated ownership as the identity.
				auto* BallOwner = Cast<ATestVehiclePawn>(It->GetOwner());
				if (!BallOwner || !Players.Contains(BallOwner)) { continue; }
				if (!bRequested && BallOwner == Pawn && Charge->CanStart()) { PC->HALUseSkill(); bRequested = true; }
				if (It->GetBallState() != EBasicBallState::Launched || It->GetLaunchedBy() != BallOwner || SawLaunch.Contains(BallOwner->GetFName())) { continue; }
				SawLaunch.Add(BallOwner->GetFName());
				const float Speed = It->GetPhysicsRoot()->GetPhysicsLinearVelocity().Size();
				if (bHost) { Test->TestTrue(TEXT("Both server-owned and client-owned trucks launch with skill velocity"), Speed > 4500); }
				UE_LOG(LogTemp, Display, TEXT("HAL BullCharge both contact Role=%s Ball=%s Launcher=%s Speed=%.2f"),
					bHost ? TEXT("Host") : TEXT("Client"), *It->GetName(), *GetNameSafe(It->GetLaunchedBy()), Speed);
			}
			if (SawLaunch.Num() == 2)
			{
				Test->TestTrue(TEXT("Each local owner submitted the normal Primary request"), bRequested);
				CompleteAt = FPlatformTime::Seconds();
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBullChargeBallBothOwnersTest, "HAL.NetworkSmoke.BullCharge.RealBallContactsBothOwners",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALBullChargeBallBothOwnersTest::RunTest(const FString&)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHALBullChargeBallBothOwners>(this));
	return true;
}
#endif
