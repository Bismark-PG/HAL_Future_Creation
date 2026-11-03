#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALWidgetExtensionTestAccess.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../HALMatchGameState.h"
#include "../HALRoomComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "HighResScreenshot.h"
#include "Misc/Paths.h"

namespace
{
	class FWidgetContractFlow : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test; int32 Step=0,EntryUpdates=0; bool SawCountdown=false;
		double Deadline=FPlatformTime::Seconds()+150,Next=0;
		TWeakObjectPtr<UHALContractSelectionWidget> FirstSelection;
		void Shot(const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("GameFlow/GF5_WidgetExtension_20261103")/Name,true,false); }
		bool Accept(APlayerController* PC,UButton* Button=nullptr)
		{
			const auto User=PC->GetLocalPlayer()->GetSlateUser();
			if (Button)
			{
				const auto Slate=Button->GetCachedWidget();
				if (!Button->GetIsEnabled() || !Slate) { return false; }
				FSlateApplication::Get().SetUserFocus(User->GetUserIndex(),Slate,EFocusCause::Navigation);
				if (FSlateApplication::Get().GetUserFocusedWidget(User->GetUserIndex())!=Slate) { return false; }
			}
			UE_LOG(LogTemp,Display,TEXT("Widget contract click: Step=%d Control=%s"),Step,*GetNameSafe(Button));
			FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),User->GetUserIndex(),false,0,0));
			FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),User->GetUserIndex(),false,0,0));
			return true;
		}
	public:
		explicit FWidgetContractFlow(FAutomationTestBase* T):Test(T) {}
		virtual bool Update() override
		{
			const double Now=FPlatformTime::Seconds();
			if (Now>Deadline) { Test->AddError(FString::Printf(TEXT("Widget interface flow timed out at step %d"),Step)); return true; }
			if (Now<Next) { return false; } if (Step==16) { return true; }
			UWorld* W=nullptr; for (const auto& C:GEngine->GetWorldContexts()) { if (C.World() && C.World()->IsGameWorld()) { W=C.World(); break; } }
			auto* GI=W?W->GetGameInstance<UHALNetworkGameInstance>():nullptr; auto* PC=GI?GI->GetFirstLocalPlayerController():nullptr;
			if (!PC || !PC->GetLocalPlayer()) { return false; }
			if (Step==0)
			{ FHALWidgetExtensionTestAccess::Configure(GI,true); FHALWidgetExtensionTestAccess::ShowEntry(GI); Step=1; Next=Now+.6; return false; }
			TArray<UUserWidget*> Widgets; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,UHALArcadeEntryWidget::StaticClass(),true);
			auto* Entry=Widgets.IsEmpty()?nullptr:Cast<UHALArcadeEntryWidget>(Widgets[0]);
			if (Step==1 && Entry)
			{ auto* Custom=Cast<UHALContractEntryWidget>(Entry); if (!Test->TestNotNull(TEXT("Configured UMG entry wins over native fallback"),Custom)) { return true; }
				Test->TestTrue(TEXT("Initial entry view event delivered"),Custom->Updates>0); EntryUpdates=Custom->Updates; Shot(TEXT("Custom_Entry.png")); Step=2; Next=Now+.6; return false; }
			if (Step==2 && Entry)
			{ auto* Custom=Cast<UHALContractEntryWidget>(Entry); Test->TestEqual(TEXT("Unchanged entry does not spam view notifications"),Custom->Updates,EntryUpdates);
				if (!Accept(PC,Custom->HostButton)) { return false; } if (!Test->TestTrue(TEXT("UMG button calls checked host request"),GI->IsConnecting())) { return true; } Step=3; Next=Now+.5; return false; }
			if (Step==15 && Entry)
			{ Test->TestEqual(TEXT("Cleared custom entry restores native class"),Entry->GetClass(),UHALArcadeEntryWidget::StaticClass()); Test->TestFalse(TEXT("Native layout restored"),Entry->UsesDesignerLayout()); Shot(TEXT("Restored_Entry.png")); Step=16; Next=Now+.5; return false; }
			auto* P=Cast<AHALPlayerController>(PC); auto* PS=P?P->GetPlayerState<AHALPlayerState>():nullptr;
			if (!PS || PS->GetRoomData().Phase==EHALRoomPlayerPhase::Unassigned) { return false; }
			Widgets.Reset(); UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,UHALPracticeRoomWidget::StaticClass(),true);
			auto* Screen=Widgets.IsEmpty()?nullptr:Cast<UHALPracticeRoomWidget>(Widgets[0]); if (!Screen) { return false; }
			const auto V=Screen->GetRoomView(); auto* Custom=Cast<UHALContractRoomWidget>(Screen); auto* Pause=Cast<UHALContractPauseWidget>(Screen);
			if (Step==3 && Custom && V.bCatalogLoaded)
			{ Test->TestEqual(TEXT("Configured room displays UMG tree"),Screen->GetClass(),UHALContractSelectionWidget::StaticClass());
				Test->TestEqual(TEXT("Four read-only seat views"),V.Seats.Num(),4); Test->TestTrue(TEXT("Vehicle display list available"),V.Vehicles.Num()>0);
				Test->TestTrue(TEXT("Initial room event and local identity"),Custom->Updates>0 && V.LocalNumber>0 && V.bHost); FirstSelection=Cast<UHALContractSelectionWidget>(Screen);
				Shot(TEXT("Custom_Selection.png")); if (!Accept(PC,Custom->NextButton)) { return false; } Step=4; Next=Now+.4; return false; }
			if (Step==4 && Custom && V.bCanConfirm && !V.bRequestPending)
			{ if (!Accept(PC,Custom->ConfirmButton)) { return false; } Step=5; Next=Now+.4; return false; }
			if (Step==5 && V.LocalPhase==EHALRoomPlayerPhase::Ready && !P->bShowMouseCursor)
			{ Test->TestFalse(TEXT("Custom room relinquishes menu input while driving"),V.bMenuVisible); P->ToggleRoomMenu(); Step=6; Next=Now+.4; return false; }
			if (Step==6 && Pause)
			{ Test->TestTrue(TEXT("Sibling pause class selected and menu input owned"),P->bShowMouseCursor && V.bMenuVisible);
				Test->TestTrue(TEXT("Old selection screen removed"),!FirstSelection.IsValid() || !FirstSelection->IsInViewport()); Shot(TEXT("Custom_Pause.png"));
				if (!Accept(PC,Pause->ResumeButton)) { return false; } Step=17; Next=Now+.4; return false; }
			if (Step==17 && Custom && !Pause && !P->bShowMouseCursor)
			{ Screen->ResumeDriving(); Test->TestFalse(TEXT("Repeated resume cannot reopen a closed menu"),P->WantsRoomMenu()); P->ToggleRoomMenu(); Step=18; Next=Now+.4; return false; }
			if (Step==18 && Pause)
			{ if (!Accept(PC,Pause->CancelButton)) { return false; } Step=7; Next=Now+.4; return false; }
			if (Step==7 && Custom && !Pause && V.LocalPhase==EHALRoomPlayerPhase::Selecting && !V.bRequestPending)
			{ Test->TestNull(TEXT("Cancel cleans actual Avatar"),P->GetPawn().Get()); if (!Accept(PC,Custom->NextButton)) { return false; } Step=8; Next=Now+.4; return false; }
			if (Step==8 && Custom && V.bCanConfirm && !V.bRequestPending)
			{ if (!Accept(PC,Custom->ConfirmButton)) { return false; } Step=9; Next=Now+.4; return false; }
			if (Step==9 && V.LocalPhase==EHALRoomPlayerPhase::Ready && !P->bShowMouseCursor)
			{ P->ToggleRoomMenu(); Step=10; Next=Now+.4; return false; }
			if (Step==10 && Pause && V.bCanStart)
			{ if (!Accept(PC,Pause->StartButton)) { return false; } Step=11; Next=Now+.1; return false; }
			if (Step==11)
			{
				if (V.Phase==EHALRoomPhase::Countdown && !SawCountdown)
				{ SawCountdown=true; Test->TestEqual(TEXT("Loading/countdown uses room, not pause screen"),Screen->GetClass(),UHALContractSelectionWidget::StaticClass());
					Test->TestTrue(TEXT("Countdown view is display-only and controls remain closed"),V.CountdownRemaining>0 && !V.bCanSelect && !V.bCanStart && !V.bCanResume); Shot(TEXT("Custom_Countdown.png")); }
				if (V.Phase==EHALRoomPhase::Playing && !P->bShowMouseCursor)
				{ Test->TestTrue(TEXT("Actual configured match world and countdown passed"),SawCountdown && W->GetMapName().Contains(TEXT("MVPB_Match"))); P->ToggleRoomMenu(); Step=12; Next=Now+.4; }
				return false;
			}
			if (Step==12 && Pause && V.bCanReturnToRoom)
			{ if (!Accept(PC,Pause->ReturnButton)) { return false; } Step=13; Next=Now+.2; return false; }
			if (Step==13 && Custom && !Pause && V.Phase==EHALRoomPhase::Open && V.LocalPhase==EHALRoomPlayerPhase::Selecting)
			{ Test->TestTrue(TEXT("Round retained, confirmation reset after return"),V.RoundId==1 && !V.bCanStart); FHALWidgetExtensionTestAccess::Configure(GI,false); GI->ShowPracticeRoom(PC); Step=14; Next=Now+.5; return false; }
			if (Step==14 && !Custom)
			{ Test->TestEqual(TEXT("Clearing room class restores native fallback"),Screen->GetClass(),UHALPracticeRoomWidget::StaticClass());
				Test->TestFalse(TEXT("Restored native screen does not use Designer"),Screen->UsesDesignerLayout()); Shot(TEXT("Restored_Room.png")); Screen->LeaveCurrentRoom(); Step=15; Next=Now+.5; return false; }
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALWidgetContractFlowTest,"HAL.NetworkSmoke.GameFlow.GF5.WidgetInterface",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHALWidgetContractFlowTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FWidgetContractFlow>(this)); return true; }
#endif
