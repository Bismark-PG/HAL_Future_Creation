#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../HALPracticeRoomGameMode.h"
#include "../HALRoomComponent.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerState.h"
#include "../HALPlayerController.h"
#include "../HALNetworkGameInstance.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALHealthAttributeSet.h"
#include "../HALMovementAttributeSet.h"
#include "../HALMovementBoostEffect.h"
#include "../HALImpactImmunityEffects.h"
#include "../HALGameplayTags.h"
#include "../VehicleDefinition.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../TestVehiclePawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HALWidgetExtensionTestAccess.h"

namespace
{
	class FRoundProbe : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		bool Solo=false,Timeout=false,RoleKnown=false,Host=false,Verified=false,SawLoading=false,SawCountdown=false,Disconnect=false,BadMap=false;
		uint32 LoadedRound=0, CountdownRound=0;
		bool ExitLoading=false,ScreenshotRequested=false,ContractConfigured=false,Exhibition=false;
		uint32 PracticeWorldId=0, DirtyRound=MAX_uint32, ReturnedRound=0, CountedPlaying=0;
		FGuid Key,FirstEpoch,FirstFlow,RoundOneEpoch; int32 Seat=INDEX_NONE;
		FLinearColor Color; FString PracticeMap;
		double Deadline=FPlatformTime::Seconds()+180, NextAction=0, Finished=-1, ExitAt=-1;
	public:
		FRoundProbe(FAutomationTestBase* T,bool S,bool O,bool D=false,bool B=false,bool H=false,bool E=false):Test(T),Solo(S),Timeout(O),Disconnect(D),BadMap(B),ExitLoading(H),Exhibition(E) {}
		virtual bool Update() override
		{
			const double Now=FPlatformTime::Seconds();
			if (Finished>0) { return Now-Finished>2; }
			if (Now>Deadline) { Test->AddError(FString::Printf(TEXT("GF4 timeout: Host=%d Return=%u Playing=%u Loading=%d Countdown=%d"),Host,ReturnedRound,CountedPlaying,SawLoading,SawCountdown)); return true; }
			UWorld* W=nullptr;
			for (const auto& C:GEngine->GetWorldContexts()) { if (auto* V=C.World(); V && V->IsGameWorld()) { W=V; break; } }
			if (!W) { return false; }
			if (Verified && W->GetMapName().Contains(TEXT("MVPB_NetEntry")))
			{
				if (ExitLoading) { Test->TestTrue(TEXT("Host exit while loading retains the reason"),W->GetGameInstance<UHALNetworkGameInstance>()->GetConnectionMessage().ToString().Contains(TEXT("Host closed"))); }
				UE_LOG(LogTemp,Display,TEXT("HAL GF4 probe: Host=%d Solo=%d TimeoutRecovery=%d PlayingRounds=%u Identity=1 Fresh=1 Entry=1"),Host,Solo,Timeout,CountedPlaying);
				Finished=Now; return false;
			}
			auto* S=W->GetGameState<AHALMatchGameState>(); if (!S) { return false; }
			auto* R=S->GetRoom(); if (!R->GetData().bPracticeRoom) { return false; }
			AHALPlayerController* PC=nullptr;
			for (auto It=W->GetPlayerControllerIterator();It;++It) { if (auto* P=Cast<AHALPlayerController>(It->Get()); P && P->IsLocalController()) { PC=P; break; } }
			auto* PS=PC?PC->GetPlayerState<AHALPlayerState>():nullptr;
			if (!PS || PS->GetRoomData().Phase==EHALRoomPlayerPhase::Unassigned) { return false; }
			if (!RoleKnown) { Host=Solo || W->GetNetMode()==NM_ListenServer; RoleKnown=true; }
			if (!ContractConfigured && FParse::Param(FCommandLine::Get(),TEXT("HALContractUI")))
			{
				if (auto* GI=W->GetGameInstance<UHALNetworkGameInstance>())
				{ FHALWidgetExtensionTestAccess::Configure(GI,true); GI->ShowPracticeRoom(PC); ContractConfigured=true;
					UE_LOG(LogTemp,Display,TEXT("Widget contract UI configured for local role Host=%d"),Host); }
			}
			const auto D=R->GetData(); const auto P=PS->GetRoomData();
			AHALPlayerState* Other=nullptr;
			for (const auto& Member:D.Members) { if (Member && Member!=PS) { Other=Member; break; } }
			const auto AllOthersNamed=[&](const FString& Name)
			{
				if (D.Members.Num()!=(Exhibition?4:2)) { return false; }
				for (const auto& Member:D.Members) { if (!Member || (Member!=PS && Member->GetPlayerName()!=Name)) { return false; } }
				return true;
			};
			auto* Pawn=Cast<ATestVehiclePawn>(PC->GetPawn()); auto* ASC=PS->GetHALAbilitySystemComponent();
			auto* M=Pawn?Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>():nullptr;
			const auto Physics=M?M->GetMovementEffectStatus():FHALMovementStatus();
			const bool Ready=Pawn && P.ReadyAvatar==Pawn && Pawn->GetPlayerState()==PS && ASC->GetAvatarActor()==Pawn && Physics.bReady;
			if (!Key.IsValid())
			{ Key=P.PlayerKey; Seat=P.Seat; Color=P.Color; FirstFlow=D.FlowId; PracticeMap=W->GetOutermost()->GetName(); PracticeWorldId=W->GetUniqueID(); }
			if (P.PlayerKey!=Key || Seat!=P.Seat || Color!=P.Color || D.FlowId!=FirstFlow)
			{ Test->AddError(TEXT("GF4 identity changed across worlds")); return true; }
			if (Verified)
			{
				if (!Host) { PC->SetName(TEXT("GF4_DONE")); }
				else if ((Solo || Disconnect || AllOthersNamed(TEXT("GF4_DONE"))) && ExitAt<0) { ExitAt=Now+1; }
				if (Host && ExitAt>0 && Now>=ExitAt) { ExitAt=MAX_dbl; PC->LeaveRoom(); }
				return false;
			}
			if (D.Phase==EHALRoomPhase::Loading || D.Phase==EHALRoomPhase::Countdown)
			{
				if (!Ready) { return false; }
				if (Exhibition)
				{ Test->TestEqual(TEXT("Actual Exhibition requires four members"),D.Rules.MinimumPlayers(),4); Test->TestEqual(TEXT("All four real processes travel together"),D.ExpectedPlayers,4); }
				Test->TestTrue(TEXT("Actual new World and distinct match package"),W->GetUniqueID()!=PracticeWorldId && W->GetOutermost()->GetName()!=PracticeMap);
				Test->TestTrue(TEXT("Fresh physics epoch after map travel"),FirstEpoch.IsValid() && Physics.Value.Epoch!=FirstEpoch);
				if (D.RoundId==1) { RoundOneEpoch=Physics.Value.Epoch; }
				else { Test->TestTrue(TEXT("Second round cannot reuse first round epoch"),RoundOneEpoch.IsValid() && RoundOneEpoch!=Physics.Value.Epoch); }
				Test->TestTrue(TEXT("Loading/countdown physical controls blocked"),Physics.Value.bControlsBlocked);
				const auto* H=ASC->GetSet<UHALHealthAttributeSet>();
				Test->TestTrue(TEXT("Full configured HP in the new round"),H && FMath::IsNearlyEqual(H->GetCurrentHP(),Pawn->GetVehicleDefinition()->Health.Health.MaxHP));
				Test->TestTrue(TEXT("Practice effects and cooldown do not cross maps"),!ASC->IsStunned() && !ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Boost)
					&& ASC->GetEffectTimeRemaining(HALGameplayTags::Cooldown_ImpactImmunity)==0 && ASC->GetSet<UHALMovementAttributeSet>()->GetAccelerationMultiplierDelta()==0);
				if (D.Phase==EHALRoomPhase::Loading)
				{
					SawLoading=true; LoadedRound=D.RoundId;
					if (Disconnect && !Host) { Verified=true; PC->LeaveRoom(); }
					if (ExitLoading)
					{
						if (!Host) { PC->SetName(TEXT("GF4_DONE")); Verified=true; }
						else if (Other && Other->GetPlayerName()==TEXT("GF4_DONE")) { Verified=true; PC->LeaveRoom(); }
					}
				}
				else
				{
					SawCountdown=true;
					if (CountdownRound!=D.RoundId) { UE_LOG(LogTemp,Display,TEXT("GF4 countdown sample: Host=%d Round=%u End=%.3f"),Host,D.RoundId,D.CountdownEndServerTime); }
					CountdownRound=D.RoundId; Test->TestTrue(TEXT("Shared future server deadline"),D.CountdownEndServerTime>0);
					if (Host && !ScreenshotRequested && FParse::Param(FCommandLine::Get(),TEXT("GF4Screenshot")))
					{ ScreenshotRequested=true; FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("GameFlow/GF4_20261103/GF4_Countdown.png"),true,false); }
				}
				return false;
			}
			if (D.Phase==EHALRoomPhase::Playing)
			{
				if (!Ready || Physics.Value.bControlsBlocked) { return false; }
				if (Timeout) { Test->AddError(TEXT("Timeout fixture must never enter Playing")); return true; }
				CountedPlaying=FMath::Max(CountedPlaying,D.RoundId);
				Test->TestTrue(TEXT("Barrier and countdown observed for this round"),LoadedRound==D.RoundId && CountdownRound==D.RoundId);
				Test->TestEqual(TEXT("Explicit new round identity"),PS->GetVehicleLifecycle().RoundId,D.RoundId);
				if (D.RoundId==1)
				{
					if (!Host) { PC->SetName(TEXT("GF4_PLAY_1")); }
					if (Host && (Solo || AllOthersNamed(TEXT("GF4_PLAY_1"))) && Now>=NextAction)
					{
						NextAction=Now+.25;
						if (Exhibition) { Test->TestTrue(TEXT("Server launch opt-in permits Exhibition host return"),W->GetAuthGameMode<AHALPracticeRoomGameMode>()->CanRequestRoundReturn()); }
						PC->HALReturnToRoom();
					}
				}
				else { Verified=true; }
				return false;
			}
			if (D.Phase!=EHALRoomPhase::Open || Now<NextAction || PC->IsRoomRequestPending()) { return false; }
			NextAction=Now+.15;
			if (D.RoundId && ReturnedRound!=D.RoundId)
			{
				if (P.Phase!=EHALRoomPlayerPhase::Selecting) { return false; }
				ReturnedRound=D.RoundId;
				Test->TestTrue(TEXT("Returned practice package in another World"),W->GetOutermost()->GetName()==PracticeMap && W->GetUniqueID()!=PracticeWorldId);
				Test->TestTrue(TEXT("Return retains model and clears confirmation/Pawn"),P.Phase==EHALRoomPlayerPhase::Selecting && !Pawn && !P.VehicleId.IsNone());
				if (Timeout || Disconnect)
				{ Test->TestTrue(TEXT("Failure has a recoverable reason"),D.FlowMessage.Contains(Timeout?TEXT("timed out"):TEXT("participant left"))); Verified=true; return false; }
			}
			if (P.Phase==EHALRoomPlayerPhase::Selecting)
			{
				if (P.VehicleId.IsNone()) { PC->SelectRoomVehicle(Host?TEXT("Car_01"):TEXT("Car_02")); }
				else { PC->ConfirmRoomVehicle(); }
			}
			else if (Ready && !Physics.Value.bControlsBlocked)
			{
				if (BadMap)
				{
					if (PC->GetLastRoomResult()==EHALRoomResult::InvalidConfiguration)
					{ Test->TestTrue(TEXT("Missing target rejects start without retiring practice Avatar"),D.Phase==EHALRoomPhase::Open && Pawn->IsConfigurationValid()); Verified=true; }
					else { PC->RequestStartMatch(); }
					return false;
				}
				if (!FirstEpoch.IsValid()) { FirstEpoch=Physics.Value.Epoch; }
				if (!Host) { PC->SetName(FString::Printf(TEXT("GF4_READY_%u"),D.RoundId)); }
				if (Host && R->CheckStart(PC)==EHALRoomResult::Accepted && (Solo || AllOthersNamed(FString::Printf(TEXT("GF4_READY_%u"),D.RoundId))))
				{
					if (DirtyRound!=D.RoundId)
					{
						for (auto It=W->GetPlayerControllerIterator();It;++It)
						{
							auto* C=Cast<AHALPlayerController>(It->Get()); auto* State=C?C->GetPlayerState<AHALPlayerState>():nullptr; if (!State) { continue; }
							auto* A=State->GetHALAbilitySystemComponent(); A->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(),11);
							A->ApplyGameplayEffectToSelf(GetDefault<UHALMovementBoostEffect>(),1,A->MakeEffectContext());
							A->ApplyGameplayEffectToSelf(GetDefault<UHALImpactImmunityCooldownEffect>(),1,A->MakeEffectContext()); A->ApplyStun(10,C);
						}
						DirtyRound=D.RoundId;
					}
					PC->RequestStartMatch();
				}
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoundNetworkTest,"HAL.NetworkSmoke.GameFlow.GF4.TwoRounds",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALRoundNetworkTest::RunTest(const FString&) { FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRoundProbe>(this,false,false)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALExhibitionRoundNetworkTest,"HAL.NetworkSmoke.GameFlow.GF6.ExhibitionFourProcessesTwoRounds",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALExhibitionRoundNetworkTest::RunTest(const FString&) { FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRoundProbe>(this,false,false,false,false,false,true)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoundTimeoutNetworkTest,"HAL.NetworkSmoke.GameFlow.GF4.TimeoutRecovery",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALRoundTimeoutNetworkTest::RunTest(const FString&) { FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRoundProbe>(this,false,true)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoundSingleTest,"HAL.NetworkSmoke.GameFlow.GF4.SingleHost",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALRoundSingleTest::RunTest(const FString&) { FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRoundProbe>(this,true,false)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoundDisconnectTest,"HAL.NetworkSmoke.GameFlow.GF4.ClientLeavesLoading",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALRoundDisconnectTest::RunTest(const FString&) { FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRoundProbe>(this,false,false,true)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoundMissingTest,"HAL.NetworkSmoke.GameFlow.GF4.MissingMap",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALRoundMissingTest::RunTest(const FString&) { FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRoundProbe>(this,true,false,false,true)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoundHostExitTest,"HAL.NetworkSmoke.GameFlow.GF4.HostLeavesLoading",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALRoundHostExitTest::RunTest(const FString&) { FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRoundProbe>(this,false,false,false,false,true)); return true; }
#endif
