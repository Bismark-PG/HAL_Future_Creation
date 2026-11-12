#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../HALBullChargeComponent.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALGameplayTags.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../TestVehiclePawn.h"
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
#endif
