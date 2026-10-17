#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALMovementBoostEffect.h"
#include "../HALMovementAttributeSet.h"
#include "../HALGameplayTags.h"
#include "../HALPlayerController.h"
#include "../HALMatchGameState.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../TestVehiclePawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

namespace
{
	class FBoostLiveTest : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline=FPlatformTime::Seconds()+100, Complete=-1;
		double RequestAt=-1, RefreshAt=-1, BothExpiredAt=-1, SecondAt=-1;
		double WaitingAt=-1;
		bool bPreplaySent=false, bPreplayChecked=false;
		bool bLagSet=false, bRequested=false, bLocalActive=false, bRefreshSent=false, bRefreshed=false;
		bool bPastOriginalExpiry=false, bLocalExpired=false, bRemoteActive=false, bRemoteExpired=false;
		bool bSecondRequested=false, bRemoteSecond=false, bClearSent=false;
		uint32 ActiveRevision=0;
	public:
		explicit FBoostLiveTest(FAutomationTestBase* InTest):Test(InTest) {}
		virtual bool Update() override
		{
			if (Complete>0) { return FPlatformTime::Seconds()-Complete>4; }
			if (FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("F3 boost network timeout: requires matching host/client processes")); return true; }
			UWorld* World=nullptr;
			for (const auto& Context:GEngine->GetWorldContexts())
			{
				auto* Candidate=Context.World();
				if (Candidate && Candidate->IsGameWorld() && (Candidate->GetNetMode()==NM_Client || Candidate->GetNetMode()==NM_ListenServer)) { World=Candidate; break; }
			}
			if (!World) { return false; }
			const bool bHost=World->GetNetMode()==NM_ListenServer;
			AHALPlayerController* PC=nullptr;
			for (auto It=World->GetPlayerControllerIterator();It;++It)
			{
				if (auto* Candidate=Cast<AHALPlayerController>(It->Get()); Candidate && Candidate->IsLocalController()) { PC=Candidate; break; }
			}
			auto* Vehicle=PC ? Cast<ATestVehiclePawn>(PC->GetPawn()):nullptr;
			auto* ASC=Vehicle ? Cast<UHALAbilitySystemComponent>(Vehicle->GetAbilitySystemComponent()):nullptr;
			auto* Movement=Vehicle ? Vehicle->FindComponentByClass<UArcadeVehicleMovementComponent>():nullptr;
			auto* Match=World->GetGameState<AHALMatchGameState>();
			UHALAbilitySystemComponent* Remote=nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World);It;++It)
			{
				if (*It!=Vehicle) { Remote=Cast<UHALAbilitySystemComponent>(It->GetAbilitySystemComponent()); if (Remote) { break; } }
			}
			if (!ASC || !Remote || !Movement || !Match || !ASC->IsHealthInitialized() || !Remote->IsHealthInitialized()) { return false; }
			if (bHost && !bLagSet)
			{
				FPacketSimulationSettings Settings; Settings.PktLag=80; World->GetNetDriver()->SetPacketSimulationSettings(Settings); bLagSet=true;
			}
			if (Match->GetMatchPhase()==EHALMatchPhase::WaitingForPlayers)
			{
				const double WaitingNow=World->GetTimeSeconds();
				if (WaitingAt<0) { WaitingAt=WaitingNow; }
				// Use an existing reliable owner RPC as a fixture readiness marker.
				// Client sends it only after its pre-Playing rejection check; never guess startup delay.
				const auto* RemoteState=Cast<APlayerState>(Remote->GetOwnerActor());
				if (bHost && RemoteState && RemoteState->GetPlayerName()==TEXT("HAL_F3_Ready")) { PC->HALStartMatch(); }
				else if (!bHost && !bPreplaySent) { bPreplaySent=true; PC->HALMovementBoost(); }
				else if (!bHost && !bPreplayChecked && WaitingNow-WaitingAt>0.4)
				{
					bPreplayChecked=true;
					Test->TestFalse(TEXT("Real client RPC before Playing does not apply GE"),ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Boost));
					PC->SetName(TEXT("HAL_F3_Ready"));
				}
				return false;
			}
			if (Match->GetMatchPhase()!=EHALMatchPhase::Playing) { return false; }
			const double Now=World->GetTimeSeconds();
			const auto Physics=Movement->GetMovementEffectStatus();
			if (!Physics.bReady) { return false; }
			const bool bOn=ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Boost);
			const bool bOtherOn=Remote->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Boost);
			const bool bBoosted=FMath::IsNearlyEqual(Physics.Value.AccelerationScale,1.5f) && FMath::IsNearlyEqual(Physics.Value.SpeedScale,1.25f);
			const bool bNeutral=Physics.Value.AccelerationScale==1 && Physics.Value.SpeedScale==1;
			const float Remaining=ASC->GetEffectTimeRemaining(HALGameplayTags::State_Movement_Boost);
			if (!bSecondRequested)
			{
				bRemoteActive |= bOtherOn;
				bRemoteExpired |= bRemoteActive && !bOtherOn;
			}
			if (!bRequested)
			{
				// The client must originate the first real owning RPC; host sees only the client's target change.
				if (bHost && !bOtherOn) { return false; }
				Test->TestFalse(TEXT("Other player's request does not boost this target"),bOn);
				PC->HALMovementBoost();
				if (!bHost) { PC->HALMovementBoost(); PC->HALMovementBoost(); }
				RequestAt=Now; bRequested=true;
				FVehicleInputCmd Input; Input.Throttle=0.1f; Movement->SetInputCommand(Input);
				return false;
			}
			if (!bLocalActive && bOn && bBoosted)
			{
				bLocalActive=true; ActiveRevision=Physics.Value.Revision;
				const auto Durations=ASC->GetActiveEffectsTimeRemainingAndDuration(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(HALGameplayTags::State_Movement_Boost)));
				Test->TestTrue(TEXT("Owner receives native five-second GE"),Durations.Num()==1 && FMath::IsNearlyEqual(Durations[0].Value,5.f,0.01f));
				Test->TestEqual(TEXT("Same-frame repeats remain one layer"),ASC->GetTagCount(HALGameplayTags::State_Movement_Boost),1);
				UE_LOG(LogTemp,Display,TEXT("HAL Boost smoke: Role=%s Active=1 Revision=%u GameTime=%.3f"),bHost?TEXT("Host"):TEXT("Client"),ActiveRevision,Now);
			}
			if (bLocalActive && !bRefreshSent && Now-RequestAt>=2)
			{
				PC->HALMovementBoost(); RefreshAt=Now; bRefreshSent=true;
			}
			if (bRefreshSent && !bRefreshed && Remaining>4.5f)
			{
				bRefreshed=true;
				Test->TestTrue(TEXT("Refresh retains paired physics multipliers"),bBoosted);
				Test->TestEqual(TEXT("Refresh of equal values creates no fake physics boundary"),Physics.Value.Revision,ActiveRevision);
				Test->TestEqual(TEXT("Refresh retains one layer"),ASC->GetTagCount(HALGameplayTags::State_Movement_Boost),1);
				UE_LOG(LogTemp,Display,TEXT("HAL Boost smoke: Role=%s Refreshed=1 Remaining=%.3f Revision=%u GameTime=%.3f"),bHost?TEXT("Host"):TEXT("Client"),Remaining,Physics.Value.Revision,Now);
			}
			if (bRefreshed && !bPastOriginalExpiry && Now-RequestAt>=5.3)
			{
				bPastOriginalExpiry=true; Test->TestTrue(TEXT("Refresh survives original five-second expiry"),bOn && bBoosted);
			}
			if (bPastOriginalExpiry && !bLocalExpired && !bOn && bNeutral)
			{
				bLocalExpired=true;
				Test->TestTrue(TEXT("Natural expiry is five game seconds after refresh"),Now-RefreshAt>=4.8 && Now-RefreshAt<5.6);
				Test->TestEqual(TEXT("Natural expiry advances physics revision once"),Physics.Value.Revision,ActiveRevision+1);
				UE_LOG(LogTemp,Display,TEXT("HAL Boost smoke: Role=%s NaturalExpiry=1 AfterRefresh=%.3f Revision=%u GameTime=%.3f"),bHost?TEXT("Host"):TEXT("Client"),Now-RefreshAt,Physics.Value.Revision,Now);
			}
			if (bLocalExpired && bRemoteExpired && BothExpiredAt<0) { BothExpiredAt=Now; }
			// Keep a replicated neutral interval between cycles so a remote remove/add cannot coalesce.
			if (BothExpiredAt>0 && !bSecondRequested && Now-BothExpiredAt>1)
			{
				bSecondRequested=true; SecondAt=Now; PC->HALUseSkill(); PC->HALMovementBoost();
			}
			if (bSecondRequested)
			{
				bRemoteSecond |= bOtherOn;
				const bool bImmune=ASC->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
				const bool bCooldown=ASC->HasMatchingGameplayTag(HALGameplayTags::Cooldown_ImpactImmunity);
				if (!bClearSent && bOn && bBoosted && bRemoteSecond && bImmune && bCooldown && Now-SecondAt>1)
				{
					bClearSent=true; PC->HALClearMovementBoost();
				}
				if (bClearSent && !bOn && bNeutral && bRemoteSecond && !bOtherOn)
				{
					Test->TestTrue(TEXT("Selective clear preserves original immunity and cooldown"),bImmune && bCooldown);
					Test->TestTrue(TEXT("Original primary still granted"),ASC->HasAbilityForInput(HALGameplayTags::Input_Ability_Primary));
					if (!bHost) { Test->TestTrue(TEXT("Client exercised pre-Playing rejection"),bPreplaySent && bPreplayChecked); }
					Test->TestTrue(TEXT("Clear completed before natural five-second expiry"),Now-SecondAt<4.5);
					Test->TestEqual(TEXT("Start/end/start/clear produce four real changes"),Physics.Value.Revision,ActiveRevision+3);
					Movement->ResetInputCommand(); Complete=FPlatformTime::Seconds();
					UE_LOG(LogTemp,Display,TEXT("HAL Boost smoke: Role=%s Complete=1 SelectiveClear=1 Immune=%d Cooldown=%.3f Revision=%u GameTime=%.3f"),bHost?TEXT("Host"):TEXT("Client"),bImmune,ASC->GetEffectTimeRemaining(HALGameplayTags::Cooldown_ImpactImmunity),Physics.Value.Revision,Now);
					PC->HALGASStatus();
				}
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBoostNetworkTest,"HAL.NetworkSmoke.Phase10F3",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALBoostNetworkTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FBoostLiveTest>(this)); return true;
}
#endif
