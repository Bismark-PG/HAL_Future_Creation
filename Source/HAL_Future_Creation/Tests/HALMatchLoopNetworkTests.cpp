#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALWidgetExtensionTestAccess.h"
#include "../HALPracticeRoomGameMode.h"
#include "../HALRoomComponent.h"
#include "../HALRoomConfig.h"
#include "../HALMatchGameState.h"
#include "../HALMatchHUDWidget.h"
#include "../HALMatchFeverDefinition.h"
#include "../HALArenaEffectGenerator.h"
#include "../HALSpecialBallPad.h"
#include "../SpringPunchGimmick.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALHealthAttributeSet.h"
#include "../HALGameFlowPermissions.h"
#include "../TestVehiclePawn.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Camera/CameraActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HighResScreenshot.h"
#include "UObject/UnrealType.h"
#include "Widgets/SWidget.h"

struct FHALFeverNetworkTestAccess
{
	static bool ArenaUsesFever(UWorld* World, const UHALMatchFeverDefinition& Fever)
	{
		int32 Punches = 0;
		for (TActorIterator<ASpringPunchGimmick> It(World); It; ++It)
		{
			const auto& Punch = **It;
			const float NewCooldown = FMath::Max(.01f, Punch.CooldownDuration * Fever.GimmickWaitMultiplier);
			if (!Punch.Cycle.bValid || !FMath::IsNearlyEqual(Punch.Cycle.CooldownSeconds, NewCooldown)) { return false; }
			++Punches;
		}
		if (!Punches) { return false; }
		if (World->GetNetMode() == NM_Client) { return true; }
		int32 Pillars = 0;
		for (TActorIterator<AHALArenaEffectGenerator> It(World); It; ++It)
		{
			if (It->WaitMultiplier != Fever.GimmickWaitMultiplier) { return false; }
			++Pillars;
		}
		const auto* Supply = World->GetGameState<AHALMatchGameState>()->FindComponentByClass<UHALSpecialBallSupply>();
		if (!Pillars || !Supply || !Supply->Settings || Supply->Pads.IsEmpty() || Supply->IntervalMultiplier != Fever.SpecialBallIntervalMultiplier) { return false; }
		UE_LOG(LogTemp, Display, TEXT("Fever arena Punches=%d Pillars=%d Pads=%d ExpectedBallInterval=%.2f Warning=%.2f"),
			Punches, Pillars, Supply->Pads.Num(), Supply->Settings->DropInterval * Supply->IntervalMultiplier, Supply->WarningDuration);
		return true;
	}
	static bool Visible(const UHALMatchHUDWidget* HUD, bool bBanner)
	{
		const auto& Widget = bBanner ? HUD->FeverBanner : HUD->FeverBadge;
		return Widget && Widget->GetVisibility().IsVisible();
	}
	static void ReachDeadline(AHALPracticeRoomGameMode* Mode)
	{
		Mode->ActiveMatch.EndServerTime = Mode->GetGameState<AHALMatchGameState>()->GetServerWorldTimeSeconds();
		Mode->PollMatchRules();
	}
};

namespace
{
	class FMatchLoopProbe : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test; bool Four=false,Known=false,Host=false,Configured=false,Contract=false,Final=false,Departed=false;
		bool FollowingFour=false,Retargeted=false; uint32 ActionRound=0,ObservedResult=0,FreshRound=0;
		bool ReturnRequested=false;
		bool bCheckFever=false;
		uint32 BannerRound=0, BadgeRound=0;
		double SeenFeverStart=-1.;
		double ReturnAt=-1;
		TWeakObjectPtr<UHALPracticeRoomWidget> ResultScreen;
		double Deadline=FPlatformTime::Seconds()+220,Next=0,DoneAt=-1,FinishAt=-1,FollowAt=-1;
		FString SentName;
		double LastNameAttempt=-1;
		double NextResultDiagnostic=0;
		void SetProbeName(AHALPlayerController* PC,const FString& Value)
		{ const double Now=FPlatformTime::Seconds(); if (SentName!=Value || Now-LastNameAttempt>.5) { SentName=Value; LastNameAttempt=Now; PC->SetName(Value); } }
		void Shot(const FString& Name)
		{
			if (FParse::Param(FCommandLine::Get(),TEXT("GF7Screenshot")))
			{ FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("GameFlow/GF7_20261105")/Name,true,true); }
		}
	public:
		FMatchLoopProbe(FAutomationTestBase* T,bool F,bool CheckFever=false):Test(T),Four(F),bCheckFever(CheckFever) {}
		void CheckFeverHUD(UWorld* World, AHALPlayerController* PC, const FHALRoomData& Room)
		{
			if (!Room.Match.IsFeverActive() || BadgeRound==Room.RoundId) { return; }
			TArray<UUserWidget*> Widgets;
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World,Widgets,UHALMatchHUDWidget::StaticClass(),false);
			for (UUserWidget* Widget : Widgets)
			{
				auto* HUD = CastChecked<UHALMatchHUDWidget>(Widget);
				if (HUD->GetOwningPlayer()!=PC || !HUD->IsVisible()) { continue; }
				const auto View = HUD->GetMatchView();
				if (!View.Match.IsFeverActive() || View.Match.FeverStartedAt != Room.Match.FeverStartedAt) { continue; }
				const bool ShowingBanner = FHALFeverNetworkTestAccess::Visible(HUD, true);
				if (ShowingBanner && BannerRound==Room.RoundId) { return; }
				if (!ShowingBanner && !FHALFeverNetworkTestAccess::Visible(HUD, false)) { continue; }
				if (Four && Host && !PC->IsRoundSpectating()) { continue; }
				if (!ShowingBanner && !FHALFeverNetworkTestAccess::ArenaUsesFever(World,*View.Match.FeverDefinition)) { continue; }
				if (!ShowingBanner && !PC->IsRoundSpectating())
				{
					auto* Car = Cast<ATestVehiclePawn>(PC->GetPawn());
					if (!Car || !FMath::IsNearlyEqual(HUD->GetLocalUltimateEnergy().FullRechargeSeconds,
						Car->GetVehicleAbilityConfig().FullRechargeSeconds / View.Match.FeverDefinition->NaturalEnergyRateMultiplier)) { continue; }
				}
				if (ShowingBanner) { BannerRound=Room.RoundId; SeenFeverStart=Room.Match.FeverStartedAt; }
				else
				{
					Test->TestEqual(TEXT("Entrance shown before persistent badge"), BannerRound, Room.RoundId);
					Test->TestEqual(TEXT("One authoritative Fever timestamp throughout presentation"), SeenFeverStart, Room.Match.FeverStartedAt);
					const double RemainingAtStart = Room.Match.EndServerTime - Room.Match.FeverStartedAt;
					Test->TestTrue(TEXT("Correct trigger for initial player count"), Four
						? RemainingAtStart > View.Match.FeverDefinition->RemainingSeconds
						: FMath::IsNearlyEqual(RemainingAtStart,double(View.Match.FeverDefinition->RemainingSeconds),.4));
					BadgeRound=Room.RoundId;
				}
				const auto Kind = ShowingBanner ? TEXT("Banner") : TEXT("Badge");
				UE_LOG(LogTemp,Display,TEXT("Fever HUD %s Round=%u Seat=%d Spectator=%d Started=%.3f"),Kind,Room.RoundId,View.LocalNumber,PC->IsRoundSpectating(),Room.Match.FeverStartedAt);
				if (FParse::Param(FCommandLine::Get(),TEXT("HALFeverShots")))
				{
					const FString File = FString::Printf(TEXT("MatchFever/Fever_%dP_Seat%d_R%u_%s.png"),Four?4:2,View.LocalNumber,Room.RoundId,Kind);
					FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/File),true,false);
				}
				return;
			}
		}
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
				if (bCheckFever)
				{
					if (!Test->TestNotNull(TEXT("Saved room profile references Fever"), C->MatchFever.Get())) { return true; }
					C->MatchRules.DurationSeconds=C->MatchFever->RemainingSeconds+(Four?30.f:6.f);
				}
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
					if (D.RoundId==(Four?1:3)) { if (Host) { Test->TestTrue(TEXT("Host returned through the widget action"),ReturnRequested); } Final=true; return false; }
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
				if (bCheckFever) { CheckFeverHUD(W,PC,D); }
				const bool WaitForFever = bCheckFever && !Four && D.RoundId!=3;
				const bool SawBadge = BadgeRound==D.RoundId;
				const auto Playing=FString::Printf(TEXT("GF7_%s_%u"),bCheckFever && SawBadge ? TEXT("FEVER") : TEXT("PLAY"),D.RoundId);
				if (!Host && !Departed) { SetProbeName(PC,Playing); }
				if (Host && ActionRound!=D.RoundId && (!WaitForFever || SawBadge) && OthersNamed(Playing))
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
				if (bCheckFever && Host && SawBadge && ActionRound==D.RoundId && (Four ? Retargeted : D.RoundId==2) && OthersNamed(Playing))
				{ FHALFeverNetworkTestAccess::ReachDeadline(W->GetAuthGameMode<AHALPracticeRoomGameMode>()); }
				return false;
			}
			if (D.Phase==EHALRoomPhase::Results && ObservedResult!=D.RoundId && Now>=NextResultDiagnostic)
			{
				NextResultDiagnostic=Now+1.;
				TArray<UUserWidget*> Visible,All; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Visible,UHALPracticeRoomWidget::StaticClass(),true);
				UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,All,UHALPracticeRoomWidget::StaticClass(),false);
				auto* Screen=Visible.IsEmpty()?nullptr:Cast<UHALPracticeRoomWidget>(Visible[0]);
				auto* Custom=Cast<UHALContractRoomWidget>(Screen); const auto* Mine=D.Match.Find(Identity.PlayerKey);
				const auto View=Screen?Screen->GetRoomView():FHALRoomView();
				UE_LOG(LogTemp,Display,TEXT("GF7 ResultsWait Host=%d Round=%u Mine=%d Life=%d Pawn=%s Camera=%s Visible=%d All=%d Screen=%s Owner=%s PC=%s Show=%d Complete=%d ContractSeen=%d ContractRound=%lld"),Host,D.RoundId,Mine!=nullptr,Mine?int32(Mine->Life):-1,*GetNameSafe(PC->GetPawn()),*GetNameSafe(PC->GetViewTarget()),Visible.Num(),All.Num(),*GetNameSafe(Screen),*GetNameSafe(Screen?Screen->GetOwningPlayer():nullptr),*GetNameSafe(PC),View.bShowResults,View.Match.bComplete,Custom?Custom->LastWasResults:false,Custom?Custom->LastMatch.RoundId:0);
			}
			if (D.Phase==EHALRoomPhase::Results && Host && ObservedResult==D.RoundId && ReturnAt>0 && Now>=ReturnAt)
			{
				ReturnAt=-1;
				if (auto* Screen=ResultScreen.Get())
				{ Test->TestTrue(TEXT("Results remain after the former automatic deadline"),S->GetServerWorldTimeSeconds()>=D.Match.ReturnServerTime);
					Test->TestTrue(TEXT("Finished match permits host return without test opt-in"),Screen->GetRoomView().bCanReturnToRoom);
					if (auto* Custom=Cast<UHALContractRoomWidget>(Screen)) { Custom->ReturnButton->OnClicked.Broadcast(); }
					else { Screen->ReturnToRoom(); }
					ReturnRequested=true; }
				return false;
			}
			if (D.Phase==EHALRoomPhase::Results && ObservedResult!=D.RoundId)
			{
				const auto* Mine=D.Match.Find(Identity.PlayerKey); if (!Mine) { return false; }
				if (Mine->Life!=EHALRoundLife::Alive && (PC->GetPawn() || !Cast<ACameraActor>(PC->GetViewTarget()))) { return false; }
				TArray<UUserWidget*> Screens; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Screens,UHALPracticeRoomWidget::StaticClass(),true);
				auto* Screen=Screens.IsEmpty()?nullptr:Cast<UHALPracticeRoomWidget>(Screens[0]); if (!Screen) { return false; }
				const auto View=Screen->GetRoomView(); if (!View.bShowResults || !View.Match.bComplete) { return false; }
				if (bCheckFever)
				{
					Test->TestFalse(TEXT("Fever retires at results"), D.Match.IsFeverActive());
					Test->TestEqual(TEXT("Expected per-round Fever latch"), D.Match.FeverStartedAt>=0., Four || D.RoundId!=3);
					if (D.Match.FeverStartedAt>=0.) { Test->TestEqual(TEXT("Local HUD, arena and owner rate reached Fever together"), BadgeRound, D.RoundId); }
				}
				if (Contract)
				{ auto* Custom=Cast<UHALContractRoomWidget>(Screen); if (!Custom || !Custom->LastWasResults || Custom->LastMatch.RoundId!=D.RoundId) { return false; }
					Test->TestTrue(TEXT("Designer event receives structured result without native layout"),Screen->UsesDesignerLayout() && Custom->LastMatch.bComplete); }
				const auto Expected=Four?EHALMatchEndReason::TimeExpired:D.RoundId==1?EHALMatchEndReason::LastSurvivor:D.RoundId==2?EHALMatchEndReason::TimeExpired:EHALMatchEndReason::SharedVictory;
				Test->TestEqual(TEXT("Expected authoritative end reason"),D.Match.Reason,Expected);
				int32 Winners=0; for (const auto& P:D.Match.Players) { if (P.bWinner) { ++Winners; } }
				Test->TestEqual(TEXT("Correct winner count including shared final deaths"),Winners,Four?2:D.RoundId==1?1:2);
				Test->TestEqual(TEXT("Full ranked snapshot replicates with the result"),D.Match.RankedPlayers.Num(),D.Match.Players.Num());
				int32 RankedWinners=0,PreviousRank=0;
				for (const auto& P:D.Match.RankedPlayers)
				{
					const auto* Original=D.Match.Find(P.PlayerKey);
					Test->TestTrue(TEXT("Ranked identity matches immutable participant data"),Original && Original->Number==P.Number && Original->FinalRank==P.FinalRank && Original->bWinner==P.bWinner && Original->HP==P.HP && Original->EliminationBatch==P.EliminationBatch);
					Test->TestTrue(TEXT("Received ranks are complete and ordered"),P.FinalRank>0 && P.FinalRank>=PreviousRank);
					PreviousRank=P.FinalRank; RankedWinners+=P.bWinner;
				}
				Test->TestEqual(TEXT("Ranked snapshot carries every shared winner"),RankedWinners,Winners);
				Test->TestFalse(TEXT("Results prohibit damage"),HALGameFlow::Permissions(W,PC->GetPawn()).bDamage);
				Test->TestFalse(TEXT("Results cannot cycle spectator"),View.bCanCycleSpectator);
				if (!Host) { Test->TestFalse(TEXT("Client cannot finish host's results screen early"),View.bCanReturnToRoom); }
				if (Four && Host) { Test->TestTrue(TEXT("Follow target quit automatically retargeted"),Retargeted); }
				if (Host) { ResultScreen=Screen; ReturnAt=Now+D.Match.Rules.ResultsSeconds+1.5; }
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALFeverDuelNetworkTest,"HAL.NetworkSmoke.MatchFever.Duel",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALFeverDuelNetworkTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FMatchLoopProbe>(this,false,true)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALFeverFinalistsNetworkTest,"HAL.NetworkSmoke.MatchFever.FinalistsAndSpectator",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALFeverFinalistsNetworkTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FMatchLoopProbe>(this,true,true)); return true; }
#endif
