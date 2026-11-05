#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALWidgetExtensionTestAccess.h"
#include "../HALPracticeRoomGameMode.h"
#include "../HALRoomComponent.h"
#include "../HALRoomConfig.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALHealthAttributeSet.h"
#include "../HALGameFlowPermissions.h"
#include "../TestVehiclePawn.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/CameraActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HighResScreenshot.h"
#include "UObject/UnrealType.h"

namespace
{
	class FMatchLoopProbe : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test; bool Four=false,Known=false,Host=false,Configured=false,Contract=false,Final=false,Departed=false;
		bool FollowingFour=false,Retargeted=false; uint32 ActionRound=0,ObservedResult=0,FreshRound=0;
		bool EarlyReturnRequested=false;
		double EarlyReturnAt=-1;
		TWeakObjectPtr<UHALPracticeRoomWidget> ResultScreen;
		double Deadline=FPlatformTime::Seconds()+220,Next=0,DoneAt=-1,FinishAt=-1,FollowAt=-1;
		FString SentName;
		double LastNameAttempt=-1;
		void SetProbeName(AHALPlayerController* PC,const FString& Value)
		{ const double Now=FPlatformTime::Seconds(); if (SentName!=Value || Now-LastNameAttempt>.5) { SentName=Value; LastNameAttempt=Now; PC->SetName(Value); } }
		void Shot(const FString& Name)
		{
			if (FParse::Param(FCommandLine::Get(),TEXT("GF7Screenshot")))
			{ FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("GameFlow/GF7_20261105")/Name,true,true); }
		}
	public:
		FMatchLoopProbe(FAutomationTestBase* T,bool F):Test(T),Four(F) {}
		virtual bool Update() override
		{
			const double Now=FPlatformTime::Seconds();
			if (FinishAt>0) { return Now>=FinishAt; }
			if (Now>Deadline) { Test->AddError(FString::Printf(TEXT("GF7 timeout Four=%d Host=%d Action=%u Result=%u Departed=%d"),Four,Host,ActionRound,ObservedResult,Departed)); return true; }
			UWorld* W=nullptr; for (const auto& C:GEngine->GetWorldContexts()) { if (C.World() && C.World()->IsGameWorld()) { W=C.World(); break; } }
			if (!W) { return false; }
			if ((Final || Departed) && W->GetMapName().Contains(TEXT("MVPB_NetEntry")))
			{ UE_LOG(LogTemp,Display,TEXT("GF7 probe complete Four=%d Host=%d Results=%u Forfeit=%d"),Four,Host,ObservedResult,Departed); FinishAt=Now+1; return false; }
			auto* S=W->GetGameState<AHALMatchGameState>(); auto* R=S?S->GetRoom():nullptr;
			if (!R || !R->GetData().bPracticeRoom) { return false; }
			AHALPlayerController* PC=nullptr; for (auto It=W->GetPlayerControllerIterator();It;++It)
			{ if (auto* C=Cast<AHALPlayerController>(It->Get()); C && C->IsLocalController()) { PC=C; break; } }
			auto* PS=PC?PC->GetPlayerState<AHALPlayerState>():nullptr;
			if (!PS || !PC->GetLocalPlayer() || (W->GetNetMode()==NM_Client && !PC->GetNetConnection()) || !PS->GetRoomData().PlayerKey.IsValid()) { return false; }
			if (!Known) { Host=W->GetNetMode()==NM_ListenServer; Known=true; }
			if (!Contract && FParse::Param(FCommandLine::Get(),TEXT("HALContractUI")))
			{ FHALWidgetExtensionTestAccess::Configure(W->GetGameInstance<UHALNetworkGameInstance>(),true); W->GetGameInstance<UHALNetworkGameInstance>()->ShowPracticeRoom(PC); Contract=true; }
			const auto D=R->GetData(); const auto Identity=PS->GetRoomData();
			if (Host && !Configured)
			{
				// Only the spawned GameMode instance receives a transient copy; no BP/CDO/DA is changed.
				auto* Mode=W->GetAuthGameMode<AHALPracticeRoomGameMode>(); if (!Mode || !R->GetServerConfig()) { return false; }
				auto* C=DuplicateObject<UHALRoomConfig>(R->GetServerConfig(),Mode); C->SetFlags(RF_Transient); C->ClearFlags(RF_Public|RF_Standalone);
				C->MatchRules.DurationSeconds=Four?12:6; C->MatchRules.ResultsSeconds=3;
				FindFProperty<FObjectProperty>(Mode->GetClass(),TEXT("RoomConfig"))->SetObjectPropertyValue_InContainer(Mode,C); Configured=true;
			}
			const auto OthersNamed=[&](const FString& Name)
			{ for (const auto& M:D.Members) { if (M && M!=PS && M->GetPlayerName()!=Name) { return false; } } return true; };
			if (Final)
			{
				if (!Host) { SetProbeName(PC,TEXT("GF7_DONE")); }
				else if (OthersNamed(TEXT("GF7_DONE"))) { if (DoneAt<0) { DoneAt=Now+.5; } if (Now>=DoneAt) { PC->LeaveRoom(); } }
				return false;
			}
			if (D.Phase==EHALRoomPhase::Open)
			{
				if (D.RoundId)
				{
					Test->TestEqual(TEXT("Result observed before seamless return"),ObservedResult,D.RoundId);
					Test->TestTrue(TEXT("Copied value result retained in practice"),D.Match.bComplete && !D.Match.bActive && D.Match.RoundId==D.RoundId);
					if (D.RoundId==(Four?1:3)) { if (Four && Host) { Test->TestTrue(TEXT("Exhibition host returned early through the widget action"),EarlyReturnRequested); } Final=true; return false; }
				}
				if (Now<Next || PC->IsRoomRequestPending()) { return false; } Next=Now+.15;
				if (Identity.Phase==EHALRoomPlayerPhase::Selecting)
				{ if (Identity.VehicleId.IsNone()) { PC->SelectRoomVehicle(Host?TEXT("Car_01"):TEXT("Car_02")); } else { PC->ConfirmRoomVehicle(); } }
				else if (Identity.Phase==EHALRoomPlayerPhase::Ready && PC->GetPawn() && PS->AreVehicleAbilitiesReady(Cast<ATestVehiclePawn>(PC->GetPawn())))
				{
					const auto Ready=FString::Printf(TEXT("GF7_READY_%u"),D.RoundId); if (!Host) { SetProbeName(PC,Ready); }
					if (Host && D.Members.Num()==(Four?4:2) && OthersNamed(Ready) && R->CheckStart(PC)==EHALRoomResult::Accepted) { PC->RequestStartMatch(); }
				}
				return false;
			}
			if (D.Phase==EHALRoomPhase::Playing)
			{
				if (!D.Match.bActive || D.Match.bComplete) { return false; }
				if (FreshRound!=D.RoundId)
				{
					auto* A=PS->GetHALAbilitySystemComponent(); const auto* H=A->GetSet<UHALHealthAttributeSet>();
					if (!PC->GetPawn() || !H || !FMath::IsNearlyEqual(H->GetCurrentHP(),H->GetMaxHP()) || Identity.RoundLife!=EHALRoundLife::Alive) { return false; }
					Test->TestTrue(TEXT("Fresh full HP and source skills after each round"),PS->AreVehicleAbilitiesReady(Cast<ATestVehiclePawn>(PC->GetPawn())));
					Test->TestTrue(TEXT("Server deadline and current round identity"),D.Match.EndServerTime>0 && D.Match.RoundId==D.RoundId);
					FreshRound=D.RoundId;
				}
				const auto Playing=FString::Printf(TEXT("GF7_PLAY_%u"),D.RoundId);
				if (!Host && !Departed) { SetProbeName(PC,Playing); }
				if (Host && ActionRound!=D.RoundId && OthersNamed(Playing))
				{
					ActionRound=D.RoundId;
					for (const auto& Member:D.Members)
					{
						const int32 N=Member->GetRoomData().Number;
						const float HP=Four?(N==1?0:N==4?20:80):D.RoundId==1?(Member==PS?0:100):D.RoundId==2?50:0;
						Member->GetHALAbilitySystemComponent()->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(),HP);
					}
				}
				if (Four && Host && PC->IsRoundSpectating())
				{
					if (!FollowingFour && PC->GetSpectatedPlayerNumber())
					{
						for (int32 I=0;I<4 && PC->GetSpectatedPlayerNumber()!=4;++I) { PC->CycleSpectatedPlayer(1); }
						if (PC->GetSpectatedPlayerNumber()==4)
						{ FollowingFour=true; FollowAt=Now+.4; if (PC->WantsRoomMenu()) { PC->ToggleRoomMenu(); } Shot(TEXT("GF7_NativeSpectator.png")); }
					}
					if (FollowingFour && Now>=FollowAt) { SetProbeName(PC,TEXT("GF7_FOLLOW4")); }
					const auto* Quitter=D.Match.Players.FindByPredicate([](const auto& P){return P.Number==4;});
					if (Quitter && Quitter->Life==EHALRoundLife::Forfeited && PC->GetSpectatedPlayerNumber()!=4 && PC->GetSpectatedPlayerNumber()>0)
					{
						Test->TestEqual(TEXT("Forfeit keeps remaining three connected"),D.Members.Num(),3);
						Test->TestTrue(TEXT("Spectator uses its own camera, not another Pawn"),Cast<ACameraActor>(PC->GetViewTarget())!=nullptr && !PC->GetPawn());
						Retargeted=true;
					}
				}
				if (Four && !Host && Identity.Number==4 && D.Host && D.Host->GetPlayerName()==TEXT("GF7_FOLLOW4"))
				{ Test->TestEqual(TEXT("Client departure occurs while match still playing"),D.Phase,EHALRoomPhase::Playing); Departed=true; PC->LeaveRoom(); }
				return false;
			}
			if (D.Phase==EHALRoomPhase::Results && Four && Host && ObservedResult==D.RoundId && EarlyReturnAt>0 && Now>=EarlyReturnAt)
			{
				EarlyReturnAt=-1;
				if (auto* Screen=ResultScreen.Get())
				{ Test->TestTrue(TEXT("Finished Exhibition permits host return without test opt-in"),Screen->GetRoomView().bCanReturnToRoom);
					Screen->ReturnToRoom(); EarlyReturnRequested=true; }
				return false;
			}
			if (D.Phase==EHALRoomPhase::Results && ObservedResult!=D.RoundId)
			{
				const auto* Mine=D.Match.Find(Identity.PlayerKey); if (!Mine) { return false; }
				if (Mine->Life!=EHALRoundLife::Alive && (PC->GetPawn() || !Cast<ACameraActor>(PC->GetViewTarget()))) { return false; }
				TArray<UUserWidget*> Screens; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Screens,UHALPracticeRoomWidget::StaticClass(),true);
				auto* Screen=Screens.IsEmpty()?nullptr:Cast<UHALPracticeRoomWidget>(Screens[0]); if (!Screen) { return false; }
				const auto View=Screen->GetRoomView(); if (!View.bShowResults || !View.Match.bComplete) { return false; }
				if (Contract)
				{ auto* Custom=Cast<UHALContractRoomWidget>(Screen); if (!Custom || !Custom->LastWasResults || Custom->LastMatch.RoundId!=D.RoundId) { return false; }
					Test->TestTrue(TEXT("Designer event receives structured result without native layout"),Screen->UsesDesignerLayout() && Custom->LastMatch.bComplete); }
				const auto Expected=Four?EHALMatchEndReason::TimeExpired:D.RoundId==1?EHALMatchEndReason::LastSurvivor:D.RoundId==2?EHALMatchEndReason::TimeExpired:EHALMatchEndReason::NoSurvivors;
				Test->TestEqual(TEXT("Expected authoritative end reason"),D.Match.Reason,Expected);
				int32 Winners=0; for (const auto& P:D.Match.Players) { if (P.bWinner) { ++Winners; } }
				Test->TestEqual(TEXT("Correct winner count and shared victory/draw"),Winners,Four?2:D.RoundId==1?1:D.RoundId==2?2:0);
				Test->TestFalse(TEXT("Results prohibit damage"),HALGameFlow::Permissions(W,PC->GetPawn()).bDamage);
				Test->TestFalse(TEXT("Results cannot cycle spectator"),View.bCanCycleSpectator);
				if (!Host) { Test->TestFalse(TEXT("Client cannot finish host's results screen early"),View.bCanReturnToRoom); }
				if (Four && Host) { Test->TestTrue(TEXT("Follow target quit automatically retargeted"),Retargeted); }
				if (Four && Host) { ResultScreen=Screen; EarlyReturnAt=Now+1.5; }
				ObservedResult=D.RoundId; Shot(FString::Printf(TEXT("GF7_Results_%s_%u.png"),Host?TEXT("Host"):TEXT("Client"),D.RoundId));
				UE_LOG(LogTemp,Display,TEXT("GF7 result verified Four=%d Host=%d Round=%u Reason=%d Winners=%d Designer=%d"),Four,Host,D.RoundId,int32(D.Match.Reason),Winners,Contract);
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALThreeRoundMatchNetworkTest,"HAL.NetworkSmoke.GameFlow.GF7.ThreeRoundMatch",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALThreeRoundMatchNetworkTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FMatchLoopProbe>(this,false)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALFourPlayerMatchNetworkTest,"HAL.NetworkSmoke.GameFlow.GF7.FourPlayerForfeitSpectator",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALFourPlayerMatchNetworkTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FMatchLoopProbe>(this,true)); return true; }
#endif
