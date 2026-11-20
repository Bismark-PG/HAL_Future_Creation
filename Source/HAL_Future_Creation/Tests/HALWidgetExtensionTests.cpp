#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALWidgetExtensionTestAccess.h"
#include "UObject/UnrealType.h"
#include "../HALMatchHUDWidget.h"
#include "../HALVehicleCatalog.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

struct FHALRoomScreenTestAccess
{
	static void Apply(UHALPracticeRoomWidget* Root,const FHALRoomView& View) { Root->ApplyRoomPresentation(View); }
	static void Clear(UHALPracticeRoomWidget* Root) { Root->ClearRoomScreens(); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALWidgetClassFallbackTest,"HAL.FutureCreation.GameFlow.GF5.WidgetClassesAndNativeFallback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALWidgetClassFallbackTest::RunTest(const FString&)
{
	auto* GI=NewObject<UHALNetworkGameInstance>();
	TestEqual(TEXT("No custom entry uses current native layout"),FHALWidgetExtensionTestAccess::Entry(GI),UHALArcadeEntryWidget::StaticClass());
	TestEqual(TEXT("No custom room uses current native layout"),FHALWidgetExtensionTestAccess::Room(GI,nullptr),UHALPracticeRoomWidget::StaticClass());
	FHALWidgetExtensionTestAccess::Configure(GI,true);
	TestEqual(TEXT("Configured entry wins"),FHALWidgetExtensionTestAccess::Entry(GI),UHALContractEntryWidget::StaticClass());
	TestEqual(TEXT("Selection is independent of the optional pause class"),FHALWidgetExtensionTestAccess::Room(GI,nullptr),UHALContractSelectionWidget::StaticClass());
	FHALWidgetExtensionTestAccess::Configure(GI,false);
	TestEqual(TEXT("Clearing restores native room"),FHALWidgetExtensionTestAccess::Room(GI,nullptr),UHALPracticeRoomWidget::StaticClass());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALWidgetViewContractTest,"HAL.FutureCreation.GameFlow.GF5.ReadOnlyViewAndUnboundActions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALWidgetViewContractTest::RunTest(const FString&)
{
	for (const UScriptStruct* Type:{FHALEntryView::StaticStruct(),FHALRoomView::StaticStruct(),FHALRoomSeatView::StaticStruct(),FHALVehicleDisplayView::StaticStruct()})
	{ for (TFieldIterator<FProperty> It(Type);It;++It) { TestTrue(TEXT("Presentation values are Blueprint read-only"),It->HasAnyPropertyFlags(CPF_BlueprintReadOnly)); } }
	auto* Screen=NewObject<UHALPracticeRoomWidget>(); const auto V=Screen->GetRoomView();
	TestFalse(TEXT("Unbound screen cannot start or confirm"),V.bRoomAvailable || V.bCanStart || V.bCanConfirm || V.bCanReturnToRoom);
	Screen->StartRoomMatch(); Screen->ConfirmSelection(); Screen->CancelSelection(); Screen->ResumeDriving(); Screen->ReturnToRoom();
	TestFalse(TEXT("Requests cannot manufacture a room"),Screen->GetRoomView().bRoomAvailable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoomPresentationTest,"HAL.FutureCreation.GameFlow.UIIntegration.PresentationPriority",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALRoomPresentationTest::RunTest(const FString&)
{
	FHALRoomView V; V.bRequestPending=true; V.bSpectating=true; V.bShowResults=true; V.Phase=EHALRoomPhase::Results;
	TestEqual(TEXT("Unavailable room takes priority over stale results"),UHALPracticeRoomWidget::ResolvePresentation(V).Screen,EHALRoomScreen::Transition);
	V.bRoomAvailable=true;
	TestEqual(TEXT("Results outrank spectator and pending"),UHALPracticeRoomWidget::ResolvePresentation(V).Screen,EHALRoomScreen::Results);
	V.bShowResults=false;
	for (const auto Phase:{EHALRoomPhase::Loading,EHALRoomPhase::Countdown,EHALRoomPhase::Returning,EHALRoomPhase::Locked,EHALRoomPhase::Inactive})
	{
		V.Phase=Phase; const auto P=UHALPracticeRoomWidget::ResolvePresentation(V);
		const auto Expected=Phase==EHALRoomPhase::Loading?EHALRoomScreen::Loading:Phase==EHALRoomPhase::Countdown?EHALRoomScreen::Countdown:EHALRoomScreen::Transition;
		TestEqual(TEXT("Phase transition proceeds during pending"),P.Screen,Expected); TestFalse(TEXT("Transition closes old menu"),P.bShowLocalMenu);
	}
	V.Phase=EHALRoomPhase::Playing; V.bMenuVisible=true;
	TestEqual(TEXT("Spectator does not require catalog"),UHALPracticeRoomWidget::ResolvePresentation(V).Screen,EHALRoomScreen::Spectator);
	V.bSpectating=false;
	TestEqual(TEXT("Match does not require catalog"),UHALPracticeRoomWidget::ResolvePresentation(V).Screen,EHALRoomScreen::Match);
	TestTrue(TEXT("Playing may overlay local menu"),UHALPracticeRoomWidget::ResolvePresentation(V).bShowLocalMenu);
	V.Phase=EHALRoomPhase::Open; V.LocalPhase=EHALRoomPlayerPhase::Ready;
	TestEqual(TEXT("Old match snapshot never reopens results in practice"),UHALPracticeRoomWidget::ResolvePresentation(V).Screen,EHALRoomScreen::Practice);
	for (const auto Phase:{EHALRoomPlayerPhase::Selecting,EHALRoomPlayerPhase::PreparingAvatar})
	{
		V.LocalPhase=Phase; V.bCatalogLoaded=false;
		TestEqual(TEXT("Selection waits for catalog"),UHALPracticeRoomWidget::ResolvePresentation(V).Screen,EHALRoomScreen::Transition);
		V.bCatalogLoaded=true;
		TestEqual(TEXT("Selection and avatar preparation share screen"),UHALPracticeRoomWidget::ResolvePresentation(V).Screen,EHALRoomScreen::Selection);
		TestFalse(TEXT("Selection has no duplicated local menu"),UHALPracticeRoomWidget::ResolvePresentation(V).bShowLocalMenu);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoomScreenLifecycleTest,"HAL.FutureCreation.GameFlow.UIIntegration.ChildLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALRoomScreenLifecycleTest::RunTest(const FString&)
{
	auto* Root=NewObject<UHALPracticeRoomWidget>();
	auto* Selection=NewObject<UHALContractChildScreen>(Root); auto* Match=NewObject<UHALContractChildScreen>(Root);
	auto* Results=NewObject<UHALContractChildScreen>(Root); auto* Menu=NewObject<UHALContractChildScreen>(Root);
	TestFalse(TEXT("Cannot register foreign child"),Root->RegisterRoomScreen(EHALRoomScreen::Match,NewObject<UHALContractChildScreen>()));
	TestFalse(TEXT("Cannot register root itself"),Root->RegisterRoomScreen(EHALRoomScreen::Match,Root));
	TestTrue(TEXT("Register existing selection"),Root->RegisterRoomScreen(EHALRoomScreen::Selection,Selection));
	TestTrue(TEXT("Register existing match"),Root->RegisterRoomScreen(EHALRoomScreen::Match,Match));
	TestTrue(TEXT("Register existing results"),Root->RegisterRoomScreen(EHALRoomScreen::Results,Results));
	TestTrue(TEXT("Register local menu"),Root->RegisterLocalMenu(Menu));
	TestTrue(TEXT("Root reports ownership after registering match"),Root->ManagesMatchHUD());
	TestFalse(TEXT("One child cannot represent multiple screens"),Root->RegisterRoomScreen(EHALRoomScreen::Practice,Selection));
	FHALRoomView V; V.bRoomAvailable=true; V.bCatalogLoaded=true; V.Phase=EHALRoomPhase::Open; V.LocalPhase=EHALRoomPlayerPhase::Selecting;
	FHALRoomScreenTestAccess::Apply(Root,V);
	TestTrue(TEXT("Selection activates"),Selection->bActiveNow); const int32 Activations=Selection->Activations, Focus=Selection->FocusQueries;
	V.bRequestPending=true; V.SelectedVehicleId=TEXT("Car_02"); FHALRoomScreenTestAccess::Apply(Root,V);
	TestEqual(TEXT("Pending still delivers latest data"),Selection->LastView.SelectedVehicleId,V.SelectedVehicleId);
	TestEqual(TEXT("Same screen does not repeat activation"),Selection->Activations,Activations);
	TestEqual(TEXT("Same screen does not reset focus"),Selection->FocusQueries,Focus);
	TestTrue(TEXT("Re-register same child succeeds"),Root->RegisterRoomScreen(EHALRoomScreen::Selection,Selection));
	TestEqual(TEXT("Re-register does not repeat activation"),Selection->Activations,Activations);
	V.Phase=EHALRoomPhase::Playing; V.bMenuVisible=true; FHALRoomScreenTestAccess::Apply(Root,V);
	TestFalse(TEXT("Leaving selection deactivates preview owner"),Selection->bActiveNow);
	TestTrue(TEXT("Match and menu both activate"),Match->bActiveNow && Menu->bActiveNow);
	V.Phase=EHALRoomPhase::Results; V.bSpectating=true; FHALRoomScreenTestAccess::Apply(Root,V);
	TestTrue(TEXT("Results replace match despite pending/spectator"),Results->bActiveNow && !Match->bActiveNow && !Menu->bActiveNow);
	FHALRoomScreenTestAccess::Clear(Root);
	TestFalse(TEXT("Destruct deactivates results"),Results->bActiveNow);
	TestFalse(TEXT("Destruct releases match ownership"),Root->ManagesMatchHUD());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoomMatchHUDViewTest,"HAL.FutureCreation.GameFlow.UIIntegration.MatchHUDView",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALRoomMatchHUDViewTest::RunTest(const FString&)
{
	auto* HUD=NewObject<UHALMatchHUDWidget>(); FHALRoomView V;
	V.bRoomAvailable=true; V.Phase=EHALRoomPhase::Playing; V.Match.bActive=true; V.MatchRemaining=60.01f;
	V.LoadedCatalog=NewObject<UHALVehicleCatalog>();
	V.LocalUltimateEnergy.Energy=42; V.LocalUltimateEnergy.Cost=25; V.bShowUltimateEnergy=true;
	IHALRoomScreenInterface::Execute_ApplyView(HUD,V);
	TestEqual(TEXT("Shared screen and legacy HUD getters expose the same snapshot"),HUD->GetScreenView().MatchRemaining,HUD->GetMatchView().MatchRemaining);
	TestEqual(TEXT("Embedded HUD uses supplied energy without a pawn"),HUD->GetLocalUltimateEnergy().Energy,42.f);
	TestEqual(TEXT("Embedded HUD keeps the root's loaded catalog"),HUD->GetMatchView().LoadedCatalog.Get(),V.LoadedCatalog.Get());
	TestEqual(TEXT("Applying hidden screen does not reveal it"),HUD->GetVisibility(),ESlateVisibility::Collapsed);
	TestEqual(TEXT("View can fill Designer text before activation"),HUD->GetCountdownText().ToString(),FString(TEXT("01:01")));
	IHALRoomScreenInterface::Execute_SetScreenActive(HUD,true);
	TestEqual(TEXT("Embedded HUD reads supplied server-derived remaining time"),HUD->GetCountdownText().ToString(),FString(TEXT("01:01")));
	IHALRoomScreenInterface::Execute_SetScreenActive(HUD,false); V.MatchRemaining=30; V.LocalUltimateEnergy.Energy=7;
	V.LoadedCatalog=nullptr; IHALRoomScreenInterface::Execute_ApplyView(HUD,V);
	TestEqual(TEXT("Later data never revives deactivated HUD"),HUD->GetVisibility(),ESlateVisibility::Collapsed);
	TestEqual(TEXT("Hidden HUD receives the latest energy snapshot"),HUD->GetLocalUltimateEnergy().Energy,7.f);
	TestNull(TEXT("Cleared root catalog does not retain an old reference"),HUD->GetMatchView().LoadedCatalog.Get());
	IHALRoomScreenInterface::Execute_SetScreenActive(HUD,true);
	TestEqual(TEXT("Reactivation uses latest snapshot"),HUD->GetCountdownText().ToString(),FString(TEXT("00:30")));
	V.Match.bActive=false; IHALRoomScreenInterface::Execute_ApplyView(HUD,V);
	TestEqual(TEXT("Root energy visibility works without local pawn permission queries"),HUD->GetVisibility(),ESlateVisibility::HitTestInvisible);
	V.bShowUltimateEnergy=false; IHALRoomScreenInterface::Execute_ApplyView(HUD,V);
	TestEqual(TEXT("Root can hide an energy-only HUD"),HUD->GetVisibility(),ESlateVisibility::Collapsed);
	V.bShowResults=true; IHALRoomScreenInterface::Execute_ApplyView(HUD,V);
	TestTrue(TEXT("Results remove match countdown"),HUD->GetCountdownText().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoomMatchHUDLifecycleTest,"HAL.FutureCreation.GameFlow.UIIntegration.MatchHUDRootLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALRoomMatchHUDLifecycleTest::RunTest(const FString&)
{
	auto* Root=NewObject<UHALPracticeRoomWidget>(); auto* HUD=NewObject<UHALMatchHUDWidget>(Root);
	HUD->WidgetTree=NewObject<UWidgetTree>(HUD);
	auto* Panel=HUD->WidgetTree->ConstructWidget<UVerticalBox>(); HUD->WidgetTree->RootWidget=Panel;
	auto* Title=HUD->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("TitleText"));
	Title->SetText(FText::FromString(TEXT("HUD TITLE"))); Panel->AddChild(Title);
	auto* Status=HUD->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("StatusText"));
	Status->SetText(FText::FromString(TEXT("HUD STATUS"))); Panel->AddChild(Status);
	Panel->AddChild(HUD->WidgetTree->ConstructWidget<UButton>());
	TestTrue(TEXT("Register HUD through its shared screen parent"),Root->RegisterRoomScreen(EHALRoomScreen::Match,HUD));
	TestTrue(TEXT("Root still suppresses the standalone HUD"),Root->ManagesMatchHUD());
	TestEqual(TEXT("Registration deactivates the HUD"),HUD->GetVisibility(),ESlateVisibility::Collapsed);
	FHALRoomView V; V.bRoomAvailable=true; V.Phase=EHALRoomPhase::Playing; V.Match.bActive=true;
	V.MatchRemaining=61; V.RoundId=7; V.Status=FText::FromString(TEXT("ROOM STATUS"));
	FHALRoomScreenTestAccess::Apply(Root,V);
	TestEqual(TEXT("Root activates the inherited HUD lifecycle"),HUD->GetVisibility(),ESlateVisibility::HitTestInvisible);
	TestEqual(TEXT("Legacy getter reads the shared parent cache"),HUD->GetMatchView().RoundId,int64(7));
	TestEqual(TEXT("Shared getter receives the root view"),HUD->GetScreenView().RoundId,int64(7));
	TestEqual(TEXT("Generic panel title does not overwrite HUD layout"),Title->GetText().ToString(),FString(TEXT("HUD TITLE")));
	TestEqual(TEXT("Generic panel status does not overwrite HUD layout"),Status->GetText().ToString(),FString(TEXT("HUD STATUS")));
	TestNull(TEXT("Passive HUD never supplies a menu focus target"),IHALRoomScreenInterface::Execute_GetDefaultFocusTarget(HUD));
	V.Phase=EHALRoomPhase::Results; FHALRoomScreenTestAccess::Apply(Root,V);
	TestEqual(TEXT("Results deactivate the inherited HUD"),HUD->GetVisibility(),ESlateVisibility::Collapsed);
	V.Phase=EHALRoomPhase::Playing; V.RoundId=8; V.MatchRemaining=30; V.bRequestPending=true;
	FHALRoomScreenTestAccess::Apply(Root,V);
	TestEqual(TEXT("Reactivation displays the new round snapshot"),HUD->GetCountdownText().ToString(),FString(TEXT("00:30")));
	TestEqual(TEXT("Legacy cache cannot retain the previous round"),HUD->GetMatchView().RoundId,int64(8));
	FHALRoomScreenTestAccess::Clear(Root);
	TestEqual(TEXT("Root cleanup deactivates the shared HUD"),HUD->GetVisibility(),ESlateVisibility::Collapsed);
	return true;
}
#endif
