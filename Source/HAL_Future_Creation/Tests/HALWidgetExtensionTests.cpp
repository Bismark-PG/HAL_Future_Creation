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
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Widgets/SOverlay.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALTravelLoadingRoutingTest,"HAL.FutureCreation.GameFlow.UIIntegration.TravelLoadingRouting",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALTravelLoadingRoutingTest::RunTest(const FString&)
{
	auto* GI=NewObject<UHALNetworkGameInstance>();
	const FString Practice=TEXT("/Game/Maps/ContractPractice"),Match=TEXT("/Game/Maps/ContractMatch?game=/Script/HAL_Future_Creation.HALPracticeRoomGameMode");
	TestFalse(TEXT("Create/Join without a Room preserves legacy loading"),FHALWidgetExtensionTestAccess::MatchTravel(GI,Practice,Match));
	FHALWidgetExtensionTestAccess::ConfigureTravel(GI,UHALGameFlowScreenWidget::StaticClass(),Practice);
	TestTrue(TEXT("Practice to match selects the new loading without relying on replicated phase"),FHALWidgetExtensionTestAccess::MatchTravel(GI,Practice,Match));
	TestTrue(TEXT("PIE package prefixes do not change travel direction"),FHALWidgetExtensionTestAccess::MatchTravel(GI,TEXT("/Game/Maps/UEDPIE_1_ContractPractice"),TEXT("/Game/Maps/UEDPIE_1_ContractMatch?listen")));
	TestFalse(TEXT("Results return preserves legacy loading"),FHALWidgetExtensionTestAccess::MatchTravel(GI,TEXT("/Game/Maps/ContractMatch"),Practice));
	TestFalse(TEXT("Travel to the current practice map preserves legacy loading"),FHALWidgetExtensionTestAccess::MatchTravel(GI,Practice,Practice));
	TestFalse(TEXT("Disconnect to entry preserves legacy loading"),FHALWidgetExtensionTestAccess::MatchTravel(GI,Practice,TEXT("/Game/Maps/MVPB_NetEntry")));
	TestFalse(TEXT("Ordinary Create/Join travel preserves legacy loading"),FHALWidgetExtensionTestAccess::MatchTravel(GI,Practice,Match,false));
	TestFalse(TEXT("Missing destination cannot select match loading"),FHALWidgetExtensionTestAccess::MatchTravel(GI,Practice,TEXT("")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALTravelLoadingLifecycleTest,"HAL.FutureCreation.GameFlow.UIIntegration.TravelLoadingLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALTravelLoadingLifecycleTest::RunTest(const FString&)
{
	auto* GI=NewObject<UHALNetworkGameInstance>(GEngine); GI->InitializeStandalone();
	auto* World=GI->GetWorld();
	auto* Viewport=NewObject<UGameViewportClient>(GEngine);
	// UE 5.8.3 has not exposed the suggested Bind replacement; this fixture needs no real window.
	const auto Overlay=SNew(SOverlay);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Viewport->SetViewportOverlayWidget(nullptr,Overlay);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	auto* Context=GI->GetWorldContext(); Context->GameViewport=Viewport;
	FHALWidgetExtensionTestAccess::ConfigureTravel(GI,UHALGameFlowScreenWidget::StaticClass(),UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()));
	GI->BeginTravelPresentation();
	TestNull(TEXT("Configured match loading does not replace Create/Join/return overlay"),FHALWidgetExtensionTestAccess::TravelWidget(GI));
	TestEqual(TEXT("Legacy travel still mounts one overlay"),Overlay->GetChildren()->Num(),1);
	GI->HideTravelWidgets(TEXT("/Game/Maps/ContractMatch"),true);
	auto* Loading=FHALWidgetExtensionTestAccess::TravelWidget(GI);
	if (TestNotNull(TEXT("Match travel creates the configured placeholder"),Loading))
	{
		TestEqual(TEXT("Temporary placeholder belongs to the GameInstance"),Loading->GetOuter(),static_cast<UObject*>(GI));
		TestNull(TEXT("Temporary placeholder keeps no departing PlayerController"),Loading->GetOwningPlayer());
		TestEqual(TEXT("Temporary placeholder does not capture input"),Loading->GetVisibility(),ESlateVisibility::HitTestInvisible);
		TestFalse(TEXT("Temporary placeholder does not read gameplay data"),CastChecked<UHALGameFlowScreenWidget>(Loading)->GetScreenView().bRoomAvailable);
		TestTrue(TEXT("Temporary placeholder has a Slate widget"),Loading->GetCachedWidget().IsValid());
		GI->BeginTravelPresentation(true);
		TestEqual(TEXT("Repeated begin reuses the current placeholder"),FHALWidgetExtensionTestAccess::TravelWidget(GI),Loading);
	}
	TestEqual(TEXT("Match travel mounts only one overlay"),Overlay->GetChildren()->Num(),1);
	FHALWidgetExtensionTestAccess::EndTravel(GI);
	TestNull(TEXT("Existing end-travel handoff releases the placeholder"),FHALWidgetExtensionTestAccess::TravelWidget(GI));
	TestEqual(TEXT("Handoff removes the viewport overlay"),Overlay->GetChildren()->Num(),0);
	FHALWidgetExtensionTestAccess::ConfigureTravel(GI,nullptr,TEXT("/Game/Maps/ContractPractice")); GI->BeginTravelPresentation(true);
	TestNull(TEXT("Unconfigured match loading falls back to legacy"),FHALWidgetExtensionTestAccess::TravelWidget(GI));
	TestEqual(TEXT("Fallback still displays a loading overlay"),Overlay->GetChildren()->Num(),1);
	FHALWidgetExtensionTestAccess::EndTravel(GI);
	FHALWidgetExtensionTestAccess::ConfigureTravel(GI,UUserWidget::StaticClass(),TEXT("/Game/Maps/ContractPractice")); GI->BeginTravelPresentation(true);
	TestNull(TEXT("Abstract class safely uses the legacy overlay"),FHALWidgetExtensionTestAccess::TravelWidget(GI));
	GI->Shutdown();
	TestEqual(TEXT("Shutdown removes the fallback overlay"),Overlay->GetChildren()->Num(),0);
	Context->GameViewport=nullptr; World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}

struct FHALRoomScreenTestAccess
{
	static void Apply(UHALPracticeRoomWidget* Root,const FHALRoomView& View) { Root->ApplyRoomPresentation(View); }
	static void Clear(UHALPracticeRoomWidget* Root) { Root->ClearRoomScreens(); }
	static void Discover(UHALPracticeRoomWidget* Root) { Root->RegisterDesignerScreens(); }
	static void Deliver(UHALPracticeRoomWidget* Root,const FHALRoomView& View) { Root->DeliverRoomView(View); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALLoadingPartsTest,"HAL.FutureCreation.GameFlow.UIIntegration.LoadingParts",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALLoadingPartsTest::RunTest(const FString&)
{
	auto* GI=NewObject<UHALNetworkGameInstance>(GEngine); GI->InitializeStandalone(); auto* World=GI->GetWorld();
	auto* Screen=CreateWidget<UHALGameFlowScreenWidget>(GI); Screen->Panel=EHALGameFlowPanel::Loading;
	auto* Tree=Screen->WidgetTree.Get();
	auto* Root=Tree->ConstructWidget<UVerticalBox>(); Tree->RootWidget=Root;
	auto* Map=Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("MapLoadingPanel")); Root->AddChild(Map);
	auto* Players=Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("PlayersLoadingPanel")); Root->AddChild(Players);
	auto* MapText=Tree->ConstructWidget<UTextBlock>(); Map->AddChild(MapText); MapText->SetText(FText::FromString(TEXT("MY MAP LOADING")));
	auto* Title=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("TitleText")); Players->AddChild(Title);
	auto* Detail=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("DetailText")); Players->AddChild(Detail);
	Map->SetVisibility(ESlateVisibility::Collapsed); Players->SetVisibility(ESlateVisibility::Visible);
	TSharedPtr<SWidget> Slate=Screen->TakeWidget();
	TestEqual(TEXT("Before a Room view only map loading is shown"),Map->GetVisibility(),ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("Map travel hides the player-count section"),Players->GetVisibility(),ESlateVisibility::Collapsed);
	TestTrue(TEXT("Map travel does not fabricate player counts"),Detail->GetText().IsEmpty());
	FHALRoomView V; V.bRoomAvailable=true; V.Phase=EHALRoomPhase::Loading; V.LoadedPlayers=1; V.ExpectedPlayers=4;
	Screen->PrepareView(V); IHALRoomScreenInterface::Execute_ApplyView(Screen,V); Screen->FinishView();
	TestEqual(TEXT("Room data hides map loading"),Map->GetVisibility(),ESlateVisibility::Collapsed);
	TestEqual(TEXT("Room data shows the player-count section"),Players->GetVisibility(),ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("Player counts still use the Room snapshot"),Detail->GetText().ToString(),FString(TEXT("PLAYERS LOADED  1 / 4")));
	TestEqual(TEXT("Map section keeps its Designer text"),MapText->GetText().ToString(),FString(TEXT("MY MAP LOADING")));
	V.LoadedPlayers=3; Screen->PrepareView(V); Screen->FinishView();
	TestEqual(TEXT("Repeated Room views update player counts"),Detail->GetText().ToString(),FString(TEXT("PLAYERS LOADED  3 / 4")));
	Screen->Panel=EHALGameFlowPanel::Countdown; Map->SetVisibility(ESlateVisibility::Hidden); Players->SetVisibility(ESlateVisibility::Visible); Screen->FinishView();
	TestEqual(TEXT("Other panels preserve their own visibility"),Map->GetVisibility(),ESlateVisibility::Hidden);
	Screen->Panel=EHALGameFlowPanel::Loading; Players->Rename(TEXT("LegacyPlayersPanel"),Tree); Screen->FinishView();
	TestEqual(TEXT("Missing one container preserves the existing layout"),Map->GetVisibility(),ESlateVisibility::Hidden);
	Players->Rename(TEXT("PlayersLoadingPanel"),Tree); V.bRoomAvailable=false; Screen->PrepareView(V); Screen->FinishView();
	TestEqual(TEXT("Unavailable Room hides stale player counts"),Players->GetVisibility(),ESlateVisibility::Collapsed);
	TestEqual(TEXT("Unavailable Room restores the map section"),Map->GetVisibility(),ESlateVisibility::SelfHitTestInvisible);
	Slate.Reset(); GI->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoomMatchFlowTest,"HAL.FutureCreation.GameFlow.UIIntegration.MatchFlowOwnsPages",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALRoomMatchFlowTest::RunTest(const FString&)
{
	auto* Root=NewObject<UHALPracticeRoomWidget>(); Root->WidgetTree=NewObject<UWidgetTree>(Root);
	auto* RootPanel=Root->WidgetTree->ConstructWidget<UVerticalBox>(); Root->WidgetTree->RootWidget=RootPanel;
	auto* Flow=Root->WidgetTree->ConstructWidget<UHALContractMatchFlowWidget>(); RootPanel->AddChild(Flow); Flow->bMatchFlowContainer=true;
	Flow->WidgetTree=NewObject<UWidgetTree>(Flow);
	auto* Switcher=Flow->WidgetTree->ConstructWidget<UWidgetSwitcher>(); Flow->WidgetTree->RootWidget=Switcher;
	auto* Loading=Flow->WidgetTree->ConstructWidget<UHALGameFlowScreenWidget>(); Loading->Panel=EHALGameFlowPanel::Loading; Switcher->AddChild(Loading);
	auto* Countdown=Flow->WidgetTree->ConstructWidget<UHALGameFlowScreenWidget>(); Countdown->Panel=EHALGameFlowPanel::Countdown; Switcher->AddChild(Countdown);
	auto* HUD=Flow->WidgetTree->ConstructWidget<UHALMatchHUDWidget>(); Switcher->AddChild(HUD);
	auto* Results=Flow->WidgetTree->ConstructWidget<UHALGameFlowScreenWidget>(); Results->Panel=EHALGameFlowPanel::Results; Switcher->AddChild(Results);
	Results->WidgetTree=NewObject<UWidgetTree>(Results);
	auto* ReturnButton=Results->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("ReturnButton")); Results->WidgetTree->RootWidget=ReturnButton;
	// UWidget::IsVisible requires a live Slate widget, as it has in the actual Switcher.
	const auto ReturnSlate=ReturnButton->TakeWidget();
	TestTrue(TEXT("Focus fixture has a visible Slate return button"),ReturnSlate->GetVisibility().IsVisible());
	Results->Actions.Add(TEXT("ReturnButton"),EHALGameFlowUIAction::Return);
	Flow->Pages.Add(EHALRoomPhase::Loading,Loading); Flow->Pages.Add(EHALRoomPhase::Countdown,Countdown);
	Flow->Pages.Add(EHALRoomPhase::Playing,HUD); Flow->Pages.Add(EHALRoomPhase::Results,Results);
	auto* Selection=Root->WidgetTree->ConstructWidget<UHALContractChildScreen>(); RootPanel->AddChild(Selection);
	auto* Menu=Root->WidgetTree->ConstructWidget<UHALContractChildScreen>(); RootPanel->AddChild(Menu);
	FHALRoomScreenTestAccess::Discover(Root);
	TestTrue(TEXT("Designer flag discovers the flow without changing Room Blueprint"),Root->ManagesMatchHUD());
	TestTrue(TEXT("Existing Room GetMatchHUD registration remains valid"),Root->RegisterRoomScreen(EHALRoomScreen::Match,HUD));
	Root->RegisterRoomScreen(EHALRoomScreen::Selection,Selection); Root->RegisterLocalMenu(Menu);
	FHALRoomView V; V.bRoomAvailable=true; V.bCatalogLoaded=true; V.Phase=EHALRoomPhase::Open; V.LocalPhase=EHALRoomPlayerPhase::Selecting;
	FHALRoomScreenTestAccess::Apply(Root,V);
	TestTrue(TEXT("Selection keeps its existing root routing"),Selection->bActiveNow);
	TestEqual(TEXT("MatchFlow stays hidden during selection"),Flow->GetVisibility(),ESlateVisibility::Collapsed);
	for (const auto Phase:{EHALRoomPhase::Loading,EHALRoomPhase::Countdown,EHALRoomPhase::Playing})
	{
		V.Phase=Phase; V.RoundId=10; V.Match.bActive=true; V.MatchRemaining=61; V.LocalUltimateEnergy.Energy=37;
		FHALRoomScreenTestAccess::Apply(Root,V);
		TestEqual(TEXT("Flow selects each internal page"),Switcher->GetActiveWidget(),static_cast<UWidget*>(Flow->Pages.FindRef(Phase).Get()));
		TestEqual(TEXT("Flow receives the current phase"),Flow->GetScreenView().Phase,Phase);
	}
	TestEqual(TEXT("Internal pages never reactivate the container"),Flow->Activations,1);
	TestEqual(TEXT("Nested HUD consumes the root's supplied energy"),HUD->GetLocalUltimateEnergy().Energy,37.f);
	TestEqual(TEXT("Nested HUD consumes the shared countdown"),HUD->GetCountdownText().ToString(),FString(TEXT("01:01")));
	V.bMenuVisible=true; FHALRoomScreenTestAccess::Apply(Root,V);
	TestTrue(TEXT("Local menu still overlays the flow"),Menu->bActiveNow);
	// A late, unchanged legacy registration must not leave the selected HUD deactivated.
	Root->RegisterRoomScreen(EHALRoomScreen::Match,HUD);
	V.Phase=EHALRoomPhase::Results; V.bShowResults=true; V.bRequestPending=true; V.bCanReturnToRoom=true;
	V.ResultSummary=FText::FromString(TEXT("P1 WINS")); Flow->SetVisibility(ESlateVisibility::Collapsed);
	FHALRoomScreenTestAccess::Apply(Root,V);
	TestEqual(TEXT("Flow stays visible through results"),Flow->GetVisibility(),ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("Results replace HUD without root registration"),Switcher->GetActiveWidget(),static_cast<UWidget*>(Results));
	TestEqual(TEXT("Old HUD stays inactive"),HUD->GetVisibility(),ESlateVisibility::Collapsed);
	TestFalse(TEXT("Results close the local menu"),Menu->bActiveNow);
	TestEqual(TEXT("Results receive the same full view"),Results->GetScreenView().ResultSummary.ToString(),V.ResultSummary.ToString());
	TestTrue(TEXT("Root return permission configures the native button"),ReturnButton->GetIsEnabled());
	TestTrue(TEXT("Return action is bound after injecting the root"),ReturnButton->OnClicked.IsBound());
	TestEqual(TEXT("Flow supplies its current child's focus"),IHALRoomScreenInterface::Execute_GetDefaultFocusTarget(Flow),static_cast<UWidget*>(ReturnButton));
	const int32 FlowActivations=Flow->Activations;
	V.bCanReturnToRoom=false; FHALRoomScreenTestAccess::Apply(Root,V);
	TestEqual(TEXT("Client does not receive the host return button"),ReturnButton->GetVisibility(),ESlateVisibility::Collapsed);
	TestEqual(TEXT("Results refresh never reactivates flow"),Flow->Activations,FlowActivations);
	TestFalse(TEXT("Flow rejects another container's page"),Flow->ShowFlowScreen(NewObject<UHALGameFlowScreenWidget>(Root)));
	TestFalse(TEXT("Non-container cannot drive pages"),Results->ShowFlowScreen(HUD));
	// The root supplies data; the container, not Room's Results enum, chooses the displayed widget.
	Flow->Pages.Add(EHALRoomPhase::Results,Countdown); FHALRoomScreenTestAccess::Apply(Root,V);
	TestEqual(TEXT("Flow alone decides the phase-to-page mapping"),Switcher->GetActiveWidget(),static_cast<UWidget*>(Countdown));
	TestEqual(TEXT("Replaced results become inactive"),Results->GetVisibility(),ESlateVisibility::Collapsed);
	V.Phase=EHALRoomPhase::Returning; V.bShowResults=false; FHALRoomScreenTestAccess::Apply(Root,V);
	TestEqual(TEXT("Returning deactivates the entire flow"),Flow->GetVisibility(),ESlateVisibility::Collapsed);
	TestEqual(TEXT("Returning deactivates the current child"),Countdown->GetVisibility(),ESlateVisibility::Collapsed);
	V.Phase=EHALRoomPhase::Open; FHALRoomScreenTestAccess::Apply(Root,V);
	TestTrue(TEXT("Vehicle selection routing is unchanged after return"),Selection->bActiveNow);
	V.Phase=EHALRoomPhase::Playing; V.RoundId=11; V.MatchRemaining=30; FHALRoomScreenTestAccess::Apply(Root,V);
	TestEqual(TEXT("Next round selects HUD again"),Switcher->GetActiveWidget(),static_cast<UWidget*>(HUD));
	TestEqual(TEXT("Next round cannot reuse old HUD data"),HUD->GetMatchView().RoundId,int64(11));
	TestEqual(TEXT("Next round receives the new time"),HUD->GetCountdownText().ToString(),FString(TEXT("00:30")));
	TestTrue(TEXT("Null clears the flow's selected page"),Flow->ShowFlowScreen(nullptr));
	TestNull(TEXT("Cleared flow has no stale focus"),IHALRoomScreenInterface::Execute_GetDefaultFocusTarget(Flow));
	FHALRoomScreenTestAccess::Clear(Root);
	TestFalse(TEXT("Cleanup releases flow ownership"),Root->ManagesMatchHUD());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoomResultsOnlyTest,"HAL.FutureCreation.GameFlow.UIIntegration.ResultsHideLegacyRoomContent",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALRoomResultsOnlyTest::RunTest(const FString&)
{
	auto* Root=NewObject<UHALContractRoomWidget>(); Root->WidgetTree=NewObject<UWidgetTree>(Root); Root->NativeOnInitialized();
	auto* OuterPanel=Root->WidgetTree->ConstructWidget<UVerticalBox>(); OuterPanel->AddChild(Root->Panel); Root->WidgetTree->RootWidget=OuterPanel;
	auto* Chrome=Root->WidgetTree->ConstructWidget<UTextBlock>(); OuterPanel->AddChild(Chrome); Chrome->SetVisibility(ESlateVisibility::Visible);
	auto* HiddenChrome=Root->WidgetTree->ConstructWidget<UTextBlock>(); OuterPanel->AddChild(HiddenChrome); HiddenChrome->SetVisibility(ESlateVisibility::Hidden);
	auto* SharedPanel=Root->WidgetTree->ConstructWidget<UVerticalBox>(); OuterPanel->AddChild(SharedPanel); SharedPanel->SetVisibility(ESlateVisibility::Visible);
	auto* SharedChrome=Root->WidgetTree->ConstructWidget<UTextBlock>(); SharedPanel->AddChild(SharedChrome); SharedChrome->SetVisibility(ESlateVisibility::Visible);
	auto* Flow=Root->WidgetTree->ConstructWidget<UHALContractMatchFlowWidget>(); SharedPanel->AddChild(Flow); Flow->bMatchFlowContainer=true;
	Flow->WidgetTree=NewObject<UWidgetTree>(Flow);
	auto* Switcher=Flow->WidgetTree->ConstructWidget<UWidgetSwitcher>(); Flow->WidgetTree->RootWidget=Switcher;
	auto* Loading=Flow->WidgetTree->ConstructWidget<UHALGameFlowScreenWidget>(); Switcher->AddChild(Loading); Loading->Panel=EHALGameFlowPanel::Loading;
	auto* Countdown=Flow->WidgetTree->ConstructWidget<UHALGameFlowScreenWidget>(); Switcher->AddChild(Countdown); Countdown->Panel=EHALGameFlowPanel::Countdown;
	auto* HUD=Flow->WidgetTree->ConstructWidget<UHALMatchHUDWidget>(); Switcher->AddChild(HUD);
	auto* Results=Flow->WidgetTree->ConstructWidget<UHALGameFlowScreenWidget>(); Switcher->AddChild(Results); Results->Panel=EHALGameFlowPanel::Results;
	Flow->Pages.Add(EHALRoomPhase::Loading,Loading); Flow->Pages.Add(EHALRoomPhase::Countdown,Countdown);
	Flow->Pages.Add(EHALRoomPhase::Playing,HUD); Flow->Pages.Add(EHALRoomPhase::Results,Results);
	FHALRoomScreenTestAccess::Discover(Root);
	FHALRoomView V; V.bRoomAvailable=true; V.Phase=EHALRoomPhase::Open; V.LocalPhase=EHALRoomPlayerPhase::Selecting; V.bMenuVisible=true;
	FHALRoomScreenTestAccess::Deliver(Root,V);
	TestEqual(TEXT("Legacy selection remains visible before the match"),Root->Panel->GetVisibility(),ESlateVisibility::Visible);
	for (const auto Phase:{EHALRoomPhase::Loading,EHALRoomPhase::Countdown})
	{
		V.Phase=Phase; V.bRequestPending=true; V.LoadedPlayers=1; V.ExpectedPlayers=4; V.CountdownRemaining=3;
		FHALRoomScreenTestAccess::Deliver(Root,V);
		auto* Page=Flow->Pages.FindRef(Phase).Get();
		TestEqual(TEXT("Preparation phase selects its own page"),Switcher->GetActiveWidget(),static_cast<UWidget*>(Page));
		TestEqual(TEXT("Current preparation page stays visible"),Page->GetVisibility(),ESlateVisibility::SelfHitTestInvisible);
		TestEqual(TEXT("Preparation page receives the current Room phase"),Page->GetScreenView().Phase,Phase);
		TestEqual(TEXT("Loading progress comes from the Room view"),Page->GetScreenView().LoadedPlayers,1);
		TestEqual(TEXT("Countdown comes from the Room view"),Page->GetScreenView().CountdownRemaining,3.f);
		TestEqual(TEXT("Preparation masks the old Blueprint menu despite pending requests"),Root->Panel->GetVisibility(),ESlateVisibility::Collapsed);
		TestEqual(TEXT("Preparation masks Room labels"),Chrome->GetVisibility(),ESlateVisibility::Collapsed);
		TestEqual(TEXT("Preparation masks deep siblings"),SharedChrome->GetVisibility(),ESlateVisibility::Collapsed);
		TestEqual(TEXT("Preparation preserves the layout path to MatchFlow"),SharedPanel->GetVisibility(),ESlateVisibility::Visible);
		TestEqual(TEXT("Preparation preserves MatchFlow"),Flow->GetVisibility(),ESlateVisibility::SelfHitTestInvisible);
		FHALRoomScreenTestAccess::Deliver(Root,V);
		TestEqual(TEXT("Repeated preparation views cannot reveal legacy content"),Root->Panel->GetVisibility(),ESlateVisibility::Collapsed);
	}
	TestEqual(TEXT("Countdown deactivates Loading"),Loading->GetVisibility(),ESlateVisibility::Collapsed);
	V.Phase=EHALRoomPhase::Open; V.bRequestPending=false; FHALRoomScreenTestAccess::Deliver(Root,V);
	TestEqual(TEXT("Returning to selection from preparation restores the legacy menu"),Root->Panel->GetVisibility(),ESlateVisibility::Visible);
	TestEqual(TEXT("Leaving preparation restores Room labels"),Chrome->GetVisibility(),ESlateVisibility::Visible);
	TestEqual(TEXT("Leaving preparation preserves originally Hidden labels"),HiddenChrome->GetVisibility(),ESlateVisibility::Hidden);
	TestEqual(TEXT("Leaving preparation deactivates its current page"),Countdown->GetVisibility(),ESlateVisibility::Collapsed);
	V.Phase=EHALRoomPhase::Countdown; FHALRoomScreenTestAccess::Deliver(Root,V);
	V.Phase=EHALRoomPhase::Playing; V.bMenuVisible=false; V.Match.bActive=true; V.MatchRemaining=30;
	FHALRoomScreenTestAccess::Deliver(Root,V);
	TestEqual(TEXT("Playing retains the existing Blueprint hide rule"),Root->Panel->GetVisibility(),ESlateVisibility::Collapsed);
	TestEqual(TEXT("Playing restores nonexclusive Room content"),Chrome->GetVisibility(),ESlateVisibility::Visible);
	TestEqual(TEXT("Playing restores originally Hidden content"),HiddenChrome->GetVisibility(),ESlateVisibility::Hidden);
	TestEqual(TEXT("Playing restores deep siblings"),SharedChrome->GetVisibility(),ESlateVisibility::Visible);
	TestEqual(TEXT("Playing deactivates Countdown"),Countdown->GetVisibility(),ESlateVisibility::Collapsed);
	V.bMenuVisible=true; FHALRoomScreenTestAccess::Deliver(Root,V);
	TestEqual(TEXT("Playing still permits the existing local menu"),Root->Panel->GetVisibility(),ESlateVisibility::Visible);
	V.Phase=EHALRoomPhase::Results; V.bShowResults=true; V.bMenuVisible=true; V.ResultsRemaining=8;
	V.ResultSummary=FText::FromString(TEXT("RESULTS SNAPSHOT")); FHALRoomScreenTestAccess::Deliver(Root,V);
	TestTrue(TEXT("Legacy Blueprint still receives the Results snapshot"),Root->LastWasResults);
	TestTrue(TEXT("Legacy result text is updated while its branch is hidden"),Root->StateText->GetText().ToString().Contains(TEXT("RESULTS SNAPSHOT")));
	TestEqual(TEXT("Legacy selection/results panel cannot overlap the new Results"),Root->Panel->GetVisibility(),ESlateVisibility::Collapsed);
	TestEqual(TEXT("Room-owned labels are hidden too"),Chrome->GetVisibility(),ESlateVisibility::Collapsed);
	TestEqual(TEXT("Siblings deeper along the MatchFlow path are hidden"),SharedChrome->GetVisibility(),ESlateVisibility::Collapsed);
	TestEqual(TEXT("Shared layout ancestor stays visible"),SharedPanel->GetVisibility(),ESlateVisibility::Visible);
	TestEqual(TEXT("MatchFlow stays visible"),Flow->GetVisibility(),ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("Results remains selected"),Switcher->GetActiveWidget(),static_cast<UWidget*>(Results));
	TestEqual(TEXT("Results is the visible child"),Results->GetVisibility(),ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("HUD is inactive during Results"),HUD->GetVisibility(),ESlateVisibility::Collapsed);
	V.ResultsRemaining=7; FHALRoomScreenTestAccess::Deliver(Root,V);
	TestEqual(TEXT("Repeated old Blueprint show requests cannot reveal its Results panel"),Root->Panel->GetVisibility(),ESlateVisibility::Collapsed);
	V.Phase=EHALRoomPhase::Returning; V.bShowResults=false; V.bMenuVisible=false;
	FHALRoomScreenTestAccess::Deliver(Root,V);
	TestEqual(TEXT("Restore happens before Blueprint decides the returning visibility"),Root->Panel->GetVisibility(),ESlateVisibility::Collapsed);
	TestEqual(TEXT("Leaving Results restores visible Room content"),Chrome->GetVisibility(),ESlateVisibility::Visible);
	TestEqual(TEXT("Originally hidden content is not made visible"),HiddenChrome->GetVisibility(),ESlateVisibility::Hidden);
	TestEqual(TEXT("Shared sibling is restored"),SharedChrome->GetVisibility(),ESlateVisibility::Visible);
	V.Phase=EHALRoomPhase::Open; V.bMenuVisible=true; FHALRoomScreenTestAccess::Deliver(Root,V);
	TestEqual(TEXT("Returning to selection preserves the old UI"),Root->Panel->GetVisibility(),ESlateVisibility::Visible);
	V.Phase=EHALRoomPhase::Results; V.bShowResults=true; FHALRoomScreenTestAccess::Deliver(Root,V);
	FHALRoomScreenTestAccess::Clear(Root);
	TestEqual(TEXT("Cleanup releases the temporary visibility mask"),Chrome->GetVisibility(),ESlateVisibility::Visible);
	TestEqual(TEXT("Cleanup preserves Hidden rather than forcing Visible"),HiddenChrome->GetVisibility(),ESlateVisibility::Hidden);
	FHALRoomScreenTestAccess::Discover(Root); V.Phase=EHALRoomPhase::Loading; V.bShowResults=false;
	FHALRoomScreenTestAccess::Deliver(Root,V); FHALRoomScreenTestAccess::Clear(Root);
	TestEqual(TEXT("Cleanup during Loading restores Room content"),Chrome->GetVisibility(),ESlateVisibility::Visible);
	TestEqual(TEXT("Cleanup during Loading preserves Hidden content"),HiddenChrome->GetVisibility(),ESlateVisibility::Hidden);
	Flow->bMatchFlowContainer=false; FHALRoomScreenTestAccess::Discover(Root);
	for (const auto Phase:{EHALRoomPhase::Loading,EHALRoomPhase::Countdown,EHALRoomPhase::Results})
	{
		V.Phase=Phase; V.bShowResults=Phase==EHALRoomPhase::Results; FHALRoomScreenTestAccess::Deliver(Root,V);
		TestEqual(TEXT("Without the opt-in flow, legacy presentation is unchanged"),Root->Panel->GetVisibility(),ESlateVisibility::Visible);
		TestEqual(TEXT("Legacy-only Room content is never masked"),Chrome->GetVisibility(),ESlateVisibility::Visible);
	}
	FHALRoomScreenTestAccess::Clear(Root);
	return true;
}
#endif
