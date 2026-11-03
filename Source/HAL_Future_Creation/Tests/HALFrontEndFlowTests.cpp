#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../HALArcadeEntryWidget.h"
#include "../HALPracticeRoomWidget.h"
#include "../HALNetworkGameInstance.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../HALMatchGameState.h"
#include "../HALRoomComponent.h"
#include "../TestVehiclePawn.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "HighResScreenshot.h"
#include "Misc/Paths.h"

struct FHALFrontEndTestAccess
{ static void Address(UHALArcadeEntryWidget* W,const TCHAR* Value) { W->Address->SetText(FText::FromString(Value)); } };
namespace
{
	class FFrontEndFlow : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline=FPlatformTime::Seconds()+120,Next=FPlatformTime::Seconds()+1;
		int32 Step=0;
		bool CountdownShot=false;
		void Screenshot(const TCHAR* Name)
		{ FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("GameFlow/GF5_20261103")/Name,true,false); }
		void Key(APlayerController* PC,const FKey& K)
		{
			const auto User=PC->GetLocalPlayer()->GetSlateUser();
			FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(K,FModifierKeysState(),User->GetUserIndex(),false,0,0));
			FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(K,FModifierKeysState(),User->GetUserIndex(),false,0,0));
		}
	public:
		explicit FFrontEndFlow(FAutomationTestBase* T):Test(T) {}
		virtual bool Update() override
		{
			const double Now=FPlatformTime::Seconds(); if (Now>Deadline) { Test->AddError(FString::Printf(TEXT("GF5 UI flow timed out at step %d"),Step)); return true; }
			if (Now<Next) { return false; }
			if (Step==10) { return true; }
			UWorld* W=nullptr; for (const auto& C:GEngine->GetWorldContexts()) { if (C.World() && C.World()->IsGameWorld()) { W=C.World(); break; } }
			auto* GI=W?W->GetGameInstance<UHALNetworkGameInstance>():nullptr; auto* PC=GI?GI->GetFirstLocalPlayerController():nullptr;
			if (!PC || !PC->GetLocalPlayer()) { return false; }
			TArray<UUserWidget*> Widgets; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,UHALArcadeEntryWidget::StaticClass(),true);
			auto* Entry=Widgets.IsEmpty()?nullptr:Cast<UHALArcadeEntryWidget>(Widgets[0]);
			if (Entry && Step<4) { UE_LOG(LogTemp,Verbose,TEXT("GF5 UI step=%d attract=%d"),Step,Entry->IsAttractPage()); }
			if (Step==9 && Entry)
			{ Test->TestFalse(TEXT("Return exposes recovery actions, not attract page"),Entry->IsAttractPage()); Test->TestFalse(TEXT("Return clears pending connection"),GI->IsConnecting()); Screenshot(TEXT("GF5_ReturnEntry.png")); Step=10; Next=Now+.6; return false; }
			if (Step==0 && Entry)
			{ if (!Test->TestTrue(TEXT("Native attract page without BP rewrite"),Entry->IsAttractPage())) { return true; } Screenshot(TEXT("GF5_Attract.png")); Step=1; Next=Now+.6; return false; }
			if (Step==1 && Entry)
			{ Key(PC,EKeys::Enter); Test->TestFalse(TEXT("Real Slate Enter opens connections"),Entry->IsAttractPage()); Screenshot(TEXT("GF5_Connections.png")); Step=2; Next=Now+.6; return false; }
			if (Step==2 && Entry)
			{ Key(PC,EKeys::Down); FHALFrontEndTestAccess::Address(Entry,TEXT("not-an-address")); Key(PC,EKeys::Enter); if (!Test->TestTrue(TEXT("Focused address submits checked join"),GI->GetConnectionMessage().ToString().Contains(TEXT("IPv4")))) { return true; } Test->TestFalse(TEXT("Invalid join keeps entry usable"),GI->IsConnecting()); Screenshot(TEXT("GF5_InvalidAddress.png")); Step=11; Next=Now+.6; return false; }
			if (Step==11 && Entry)
			{ FHALFrontEndTestAccess::Address(Entry,TEXT("127.0.0.1")); Key(PC,EKeys::Enter); if (!Test->TestTrue(TEXT("Valid join enters pending connection"),GI->IsConnecting())) { return true; } Step=12; Next=Now+.6; return false; }
			if (Step==12 && Entry)
			{ Key(PC,EKeys::Gamepad_FaceButton_Right); Step=3; Next=Now+.8; return false; }
			if (Step==3 && Entry)
			{ if (!Test->TestFalse(TEXT("Cancel clears pending join before hosting"),GI->IsConnecting())) { return true; } Key(PC,EKeys::Gamepad_FaceButton_Bottom); if (!Test->TestTrue(TEXT("Focused Create Room starts actual Host after cancelled join"),GI->IsConnecting())) { return true; } Step=4; Next=Now+.6; return false; }
			auto* P=Cast<AHALPlayerController>(PC); auto* PS=P?P->GetPlayerState<AHALPlayerState>():nullptr;
			auto* S=W->GetGameState<AHALMatchGameState>(); const auto* R=S?S->GetRoom():nullptr;
			Widgets.Reset(); UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,UHALPracticeRoomWidget::StaticClass(),true);
			auto* Room=Widgets.IsEmpty()?nullptr:Cast<UHALPracticeRoomWidget>(Widgets[0]);
			if (!P || !PS || !R || !Room) { return false; }
			if (Step==4 && R->GetData().Phase==EHALRoomPhase::Open)
			{ Screenshot(TEXT("GF5_Selection.png")); Key(PC,EKeys::Right); Step=5; Next=Now+.6; return false; }
			if (Step==5 && !PS->GetRoomData().VehicleId.IsNone() && !P->IsRoomRequestPending())
			{ Test->TestTrue(TEXT("Selection screen owns cursor"),P->bShowMouseCursor); Key(PC,EKeys::Down); Key(PC,EKeys::Down); Key(PC,EKeys::Enter); Step=6; Next=Now+.6; return false; }
			if (Step==6 && PS->GetRoomData().Phase==EHALRoomPlayerPhase::Ready && !P->bShowMouseCursor)
			{ auto* Vehicle=Cast<ATestVehiclePawn>(P->GetPawn()); if (!Vehicle) { Test->AddError(TEXT("Confirm did not spawn a driving vehicle")); return true; } Test->TestTrue(TEXT("Menu transition cleared vehicle input"),Vehicle->GetCurrentInputCommand().Throttle==0); Screenshot(TEXT("GF5_PracticeHUD.png")); P->ToggleRoomMenu(); Step=7; Next=Now+.6; return false; }
			if (Step==7 && P->bShowMouseCursor)
			{ Key(PC,EKeys::Down); Key(PC,EKeys::Enter); Step=8; Next=Now+.2; return false; }
			if (Step==8)
			{
				if (R->GetData().Phase==EHALRoomPhase::Countdown && !CountdownShot) { Screenshot(TEXT("GF5_Countdown.png")); CountdownShot=true; }
				if (R->GetData().Phase==EHALRoomPhase::Playing)
				{ Test->TestTrue(TEXT("UI host start passed actual loading/countdown"),CountdownShot); Test->TestFalse(TEXT("Match automatically returns gameplay input"),P->bShowMouseCursor); P->LeaveRoom(); Step=9; Next=Now+.5; }
			}
			return false;
		}
	};
}
// Entry completion is checked separately before Room lookup because the old World no longer exists.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALFrontEndFlowTest,"HAL.NetworkSmoke.GameFlow.GF5.FrontEnd",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHALFrontEndFlowTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FFrontEndFlow>(this)); return true; }
#endif
