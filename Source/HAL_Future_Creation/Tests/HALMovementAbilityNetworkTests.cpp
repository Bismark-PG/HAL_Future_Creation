#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALMovementAbilityTestSupport.h"
#include "../HALMovementAttributeSet.h"
#include "../HALHealthAttributeSet.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../TestVehiclePawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace
{
	class FMovementAbilityLiveTest : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline=FPlatformTime::Seconds()+100, Complete=-1, WaitingAt=-1, RequestAt=-1, RefreshAt=-1;
		bool bConfigured=false,bPreplaySent=false,bPreplayChecked=false,bRequested=false,bActive=false;
		bool bRefreshSent=false,bRefreshed=false,bPastOriginal=false,bExpired=false,bRemoteActive=false,bRemoteExpired=false;
		uint32 ActiveRevision=0;
	public:
		explicit FMovementAbilityLiveTest(FAutomationTestBase* InTest):Test(InTest) {}
		virtual bool Update() override
		{
			if (Complete>0) { return FPlatformTime::Seconds()-Complete>4; }
			if (FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("Movement GA network timeout; matching host/client test processes required")); return true; }
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
			ATestVehiclePawn* Other=nullptr; UHALAbilitySystemComponent* Remote=nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World);It;++It)
			{
				if (*It!=Vehicle) { Other=*It; Remote=Cast<UHALAbilitySystemComponent>(It->GetAbilitySystemComponent()); if (Remote) { break; } }
			}
			if (!ASC || !Remote || !Movement || !Match || !ASC->IsHealthInitialized() || !Remote->IsHealthInitialized()) { return false; }
			if (bHost && !bConfigured)
			{
				// Change only these runtime PlayerState instances, emulating the future Editor loadout replacement.
				if (!HALMovementAbilityTests::ConfigurePrimary(Vehicle->GetPlayerState<AHALPlayerState>())
					|| !HALMovementAbilityTests::ConfigurePrimary(Other->GetPlayerState<AHALPlayerState>()))
				{ Test->AddError(TEXT("Could not replace runtime Primary grants")); return true; }
				FPacketSimulationSettings Settings; Settings.PktLag=80; World->GetNetDriver()->SetPacketSimulationSettings(Settings);
				IConsoleManager::Get().FindConsoleVariable(TEXT("hal.MovementBoostDebug"))->Set(0,ECVF_SetByCode);
				bConfigured=true;
				UE_LOG(LogTemp,Display,TEXT("HAL Movement GA smoke: PrimaryReplaced=1 DebugApplyDisabled=1 HostOutgoingLag=80ms"));
			}
			if (!HALMovementAbilityTests::HasMovementPrimary(ASC)) { return false; }
			const double Now=World->GetTimeSeconds();
			if (Match->GetMatchPhase()==EHALMatchPhase::WaitingForPlayers)
			{
				if (WaitingAt<0) { WaitingAt=Now; }
				const auto* OtherState=Other->GetPlayerState();
				if (bHost && OtherState && OtherState->GetPlayerName()==TEXT("HAL_MoveGA_Ready")) { PC->HALStartMatch(); }
				else if (!bHost && !bPreplaySent) { bPreplaySent=true; Vehicle->RequestPrimarySkill(); }
				else if (!bHost && !bPreplayChecked && Now-WaitingAt>0.5)
				{
					bPreplayChecked=true;
					Test->TestFalse(TEXT("Client skill request before Playing does not apply boost"),ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Boost));
					PC->SetName(TEXT("HAL_MoveGA_Ready"));
				}
				return false;
			}
			const auto Physics=Movement->GetMovementEffectStatus(); if (!Physics.bReady) { return false; }
			const bool bOn=ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Boost);
			const bool bOtherOn=Remote->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Boost);
			bRemoteActive|=bOtherOn; bRemoteExpired|=bRemoteActive && !bOtherOn;
			const bool bBoosted=FMath::IsNearlyEqual(Physics.Value.AccelerationScale,1.5f) && FMath::IsNearlyEqual(Physics.Value.SpeedScale,1.25f);
			const bool bNeutral=Physics.Value.AccelerationScale==1 && Physics.Value.SpeedScale==1;
			if (!bRequested)
			{
				if (bHost && !bOtherOn) { return false; } // Client must genuinely request its own skill first.
				Test->TestFalse(TEXT("Other player's ability cannot boost this target"),bOn);
				Vehicle->RequestPrimarySkill(); if (!bHost) { Vehicle->RequestPrimarySkill(); Vehicle->RequestPrimarySkill(); }
				bRequested=true; RequestAt=Now;
				FVehicleInputCmd Input; Input.Throttle=0.1f; Movement->SetInputCommand(Input); return false;
			}
			if (!bActive && bOn && bBoosted)
			{
				bActive=true; ActiveRevision=Physics.Value.Revision;
				Test->TestEqual(TEXT("Repeated skill requests produce one layer"),ASC->GetTagCount(HALGameplayTags::State_Movement_Boost),1);
				Test->TestFalse(TEXT("Replaced skill does not grant immunity"),ASC->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune));
				Test->TestFalse(TEXT("Replaced skill creates no immunity cooldown"),ASC->HasMatchingGameplayTag(HALGameplayTags::Cooldown_ImpactImmunity));
				for (const auto& Spec:ASC->GetActivatableAbilities()) { Test->TestFalse(TEXT("Instant GA is ended while its GE runs"),Spec.IsActive()); }
				UE_LOG(LogTemp,Display,TEXT("HAL Movement GA smoke: Role=%s Active=1 Revision=%u GameTime=%.3f"),bHost?TEXT("Host"):TEXT("Client"),ActiveRevision,Now);
			}
			if (bActive && !bRefreshSent && Now-RequestAt>=2) { Vehicle->RequestPrimarySkill(); RefreshAt=Now; bRefreshSent=true; }
			const float Remaining=ASC->GetEffectTimeRemaining(HALGameplayTags::State_Movement_Boost);
			if (bRefreshSent && !bRefreshed && Remaining>4.5f)
			{
				bRefreshed=true;
				Test->TestEqual(TEXT("Skill refresh makes no false physics boundary"),Physics.Value.Revision,ActiveRevision);
				Test->TestEqual(TEXT("Still one layer"),ASC->GetTagCount(HALGameplayTags::State_Movement_Boost),1);
				UE_LOG(LogTemp,Display,TEXT("HAL Movement GA smoke: Role=%s Refreshed=1 Remaining=%.3f Revision=%u GameTime=%.3f"),bHost?TEXT("Host"):TEXT("Client"),Remaining,Physics.Value.Revision,Now);
			}
			if (bRefreshed && !bPastOriginal && Now-RequestAt>=5.3)
			{
				bPastOriginal=true; Test->TestTrue(TEXT("Skill refresh survives original expiry"),bOn && bBoosted);
			}
			if (bPastOriginal && !bExpired && !bOn && bNeutral)
			{
				bExpired=true;
				Test->TestTrue(TEXT("Natural five-game-second expiry after last skill press"),Now-RefreshAt>=4.8 && Now-RefreshAt<5.7);
				Test->TestEqual(TEXT("Only expiry advances physics revision"),Physics.Value.Revision,ActiveRevision+1);
				UE_LOG(LogTemp,Display,TEXT("HAL Movement GA smoke: Role=%s NaturalExpiry=1 AfterRefresh=%.3f Revision=%u GameTime=%.3f"),bHost?TEXT("Host"):TEXT("Client"),Now-RefreshAt,Physics.Value.Revision,Now);
			}
			if (bExpired && bRemoteExpired)
			{
				if (!bHost) { Test->TestTrue(TEXT("Real client exercised Waiting rejection"),bPreplaySent && bPreplayChecked); }
				Test->TestTrue(TEXT("Exactly one movement Primary remains"),HALMovementAbilityTests::HasMovementPrimary(ASC));
				Test->TestEqual(TEXT("No unwanted HP change"),ASC->GetSet<UHALHealthAttributeSet>()->GetCurrentHP(),100.f);
				Movement->ResetInputCommand(); Complete=FPlatformTime::Seconds();
				UE_LOG(LogTemp,Display,TEXT("HAL Movement GA smoke: Role=%s Complete=1 Accel=%.2f Speed=%.2f Revision=%u Rewinds=%d GameTime=%.3f"),bHost?TEXT("Host"):TEXT("Client"),Physics.Value.AccelerationScale,Physics.Value.SpeedScale,Physics.Value.Revision,Physics.RewindRequests,Now);
				PC->HALGASStatus();
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMovementAbilityNetworkTest,"HAL.NetworkSmoke.Phase10F3Ability",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALMovementAbilityNetworkTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FMovementAbilityLiveTest>(this)); return true;
}
#endif
