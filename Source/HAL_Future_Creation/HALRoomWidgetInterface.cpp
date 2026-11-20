#include "HALPracticeRoomWidget.h"
#include "HALGameFlowWidgets.h"
#include "HALMatchHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "HALPlayerController.h"
#include "HALPlayerState.h"
#include "HALMatchGameState.h"
#include "HALRoomComponent.h"
#include "HALVehicleCatalog.h"
#include "HALPracticeRoomGameMode.h"
#include "HALAbilitySystemComponent.h"
#include "HALGameFlowPermissions.h"
#include "TestVehiclePawn.h"
#include "Engine/World.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetSwitcher.h"

namespace
{
	const UHALRoomComponent* ScreenRoom(const UUserWidget* W)
	{ const auto* S=W->GetWorld()?W->GetWorld()->GetGameState<AHALMatchGameState>():nullptr; return S?S->GetRoom():nullptr; }
}
bool UHALPracticeRoomWidget::CanCancel() const
{
	const auto* PC=GetOwningPlayer<AHALPlayerController>(); const auto* PS=GetOwningPlayerState<AHALPlayerState>(); const auto* R=ScreenRoom(this);
	return PC && PS && R && !PC->IsRoomRequestPending() && R->GetData().Phase==EHALRoomPhase::Open
		&& PS->GetRoomData().Phase!=EHALRoomPlayerPhase::Selecting;
}
bool UHALPracticeRoomWidget::CanResume() const
{
	const auto* PS=GetOwningPlayerState<AHALPlayerState>(); const auto* R=ScreenRoom(this);
	return PS && R && (R->GetData().Phase==EHALRoomPhase::Open || R->GetData().Phase==EHALRoomPhase::Playing)
		&& PS->GetRoomData().Phase==EHALRoomPlayerPhase::Ready;
}
bool UHALPracticeRoomWidget::CanReturnToRoom() const
{
	const auto* R=ScreenRoom(this); const auto* PC=GetOwningPlayer<AHALPlayerController>();
	const auto* Mode=GetWorld()?GetWorld()->GetAuthGameMode<AHALPracticeRoomGameMode>():nullptr;
	return R && PC && PC->IsLocalController() && !PC->IsRoomRequestPending() && Mode && Mode->CanRequestRoundReturn()
		&& R->GetData().Host==GetOwningPlayerState<AHALPlayerState>();
}
void UHALPracticeRoomWidget::SelectVehicleById(FName Id)
{ if (CanSelect()) { GetOwningPlayer<AHALPlayerController>()->SelectRoomVehicle(Id); } }
void UHALPracticeRoomWidget::ConfirmSelection()
{ if (CanConfirm()) { GetOwningPlayer<AHALPlayerController>()->ConfirmRoomVehicle(); } }
void UHALPracticeRoomWidget::CancelSelection()
{ if (CanCancel()) { GetOwningPlayer<AHALPlayerController>()->CancelRoomConfirmation(); } }
void UHALPracticeRoomWidget::StartRoomMatch()
{ if (CanStart()) { GetOwningPlayer<AHALPlayerController>()->RequestStartMatch(); } }
void UHALPracticeRoomWidget::ResumeDriving()
{ if (auto* PC=GetOwningPlayer<AHALPlayerController>(); CanResume() && PC && PC->WantsRoomMenu()) { PC->ToggleRoomMenu(); } }
void UHALPracticeRoomWidget::LeaveCurrentRoom()
{ if (auto* PC=GetOwningPlayer<AHALPlayerController>()) { PC->LeaveRoom(); } }
void UHALPracticeRoomWidget::ReturnToRoom()
{ if (CanReturnToRoom()) { GetOwningPlayer<AHALPlayerController>()->HALReturnToRoom(); } }
void UHALPracticeRoomWidget::CycleSpectator(int32 Direction)
{ if (auto* PC=GetOwningPlayer<AHALPlayerController>()) { PC->CycleSpectatedPlayer(Direction); PublishRoomView(); } }
FHALRoomView UHALPracticeRoomWidget::GetRoomView() const
{
	FHALRoomView View; const auto* R=ScreenRoom(this); const auto* PC=GetOwningPlayer<AHALPlayerController>();
	const auto* PS=GetOwningPlayerState<AHALPlayerState>();
	View.bRoomAvailable=R && R->IsEnabled() && PS; View.bCatalogLoaded=LoadedCatalog!=nullptr;
	View.bMenuVisible=bInputModeKnown?bUsingMenu:MenuVisible();
	View.bRequestPending=PC && PC->IsRoomRequestPending();
	View.bCanSelect=CanSelect(); View.bCanConfirm=CanConfirm(); View.bCanCancel=CanCancel(); View.bCanStart=CanStart();
	View.bCanResume=CanResume(); View.bCanReturnToRoom=CanReturnToRoom();
	View.bSpectating=PC && PC->IsRoundSpectating(); View.SpectatedNumber=PC?PC->GetSpectatedPlayerNumber():0;
	if (View.bSpectating) { View.bMenuVisible=MenuVisible(); }
	View.bCanLeave=PC && GetWorld() && !GetWorld()->bIsTearingDown && !PC->IsActorBeingDestroyed();
	View.SelectedVehicleName=VehicleText(); View.Description=DescriptionText(); View.LoadedPreview=LoadedPreview; View.Status=StatusText();
	if (PS)
	{ const auto& P=PS->GetRoomData(); View.SelectedVehicleId=P.VehicleId; View.LocalPhase=P.Phase; View.LocalNumber=P.Number; View.LocalColor=P.Color; }
	if (R)
	{
		const auto& D=R->GetData(); View.Phase=D.Phase; View.RoundId=D.RoundId; View.MinimumPlayers=D.Rules.MinimumPlayers();
		View.Match=D.Match; View.bShowResults=D.Phase==EHALRoomPhase::Results;
		View.bCanCycleSpectator=View.bSpectating && D.Phase==EHALRoomPhase::Playing;
		const double Now=GetWorld()->GetGameState<AHALMatchGameState>()->GetServerWorldTimeSeconds();
		if (D.Phase==EHALRoomPhase::Playing)
		{
			View.ServerWorldTimeSeconds=Now;
			const auto* Pawn=PC?Cast<ATestVehiclePawn>(PC->GetPawn()):nullptr;
			const auto* ASC=Pawn?Cast<UHALAbilitySystemComponent>(Pawn->GetAbilitySystemComponent()):nullptr;
			if (ASC && ASC->GetAvatarActor()==Pawn) { View.LocalUltimateEnergy=ASC->GetUltimateEnergyView(); }
			View.bShowUltimateEnergy=View.LocalUltimateEnergy.Cost>0 && HALGameFlow::Permissions(GetWorld(),Pawn).bAbilities;
		}
		if (D.Match.bActive) { View.MatchRemaining=D.Match.bComplete?0:float(FMath::Max(0.0,D.Match.EndServerTime-Now)); }
		if (View.bShowResults) { View.ResultsRemaining=float(FMath::Max(0.0,D.Match.ReturnServerTime-Now)); }
		View.ResultSummary=MatchResultText();
		View.ExpectedPlayers=D.ExpectedPlayers; View.bHost=PS && D.Host==PS;
		if (D.Phase==EHALRoomPhase::Countdown)
		{ View.CountdownRemaining=FMath::Max(0.0,D.CountdownEndServerTime-GetWorld()->GetGameState<AHALMatchGameState>()->GetServerWorldTimeSeconds()); }
		for (int32 I=0;I<4;++I)
		{
			FHALRoomSeatView Seat; Seat.Seat=I; Seat.Number=I+1;
			for (const auto& Member:D.Members)
			{
				if (!Member || Member->GetRoomData().Seat!=I) { continue; }
				const auto& P=Member->GetRoomData(); Seat.bOccupied=true; Seat.bLocal=Member==PS; Seat.bHost=Member==D.Host;
				Seat.Color=P.Color; Seat.Number=P.Number; Seat.PlayerName=Member->GetPlayerName(); Seat.VehicleId=P.VehicleId;
				Seat.Phase=P.Phase; Seat.bClientLoaded=P.bClientLoaded; if (P.bClientLoaded) { ++View.LoadedPlayers; } break;
			}
			View.Seats.Add(Seat);
		}
	}
	View.LoadedCatalog=LoadedCatalog;
	if (LoadedCatalog)
	{
		for (const auto& E:LoadedCatalog->Vehicles)
		{ FHALVehicleDisplayView V; V.VehicleId=E.VehicleId; V.DisplayName=E.DisplayName.IsEmpty()?FText::FromName(E.VehicleId):E.DisplayName;
			V.Description=E.Description; V.Icon=E.Icon; V.PreviewImage=E.PreviewImage; View.Vehicles.Add(V); }
	}
	return View;
}
void UHALPracticeRoomWidget::PublishRoomView()
{
	const auto View=GetRoomView();
	if (!bHasPublishedView || !FHALRoomView::StaticStruct()->CompareScriptStruct(&View,&PublishedView,0))
	{ PublishedView=View; bHasPublishedView=true; ReceiveRoomView(View); ApplyRoomPresentation(View); }
}

FHALRoomPresentation UHALPracticeRoomWidget::ResolvePresentation(const FHALRoomView& View)
{
	FHALRoomPresentation Result;
	if (!View.bRoomAvailable) { return Result; }
	if (View.bShowResults || View.Phase==EHALRoomPhase::Results) { Result.Screen=EHALRoomScreen::Results; return Result; }
	switch (View.Phase)
	{
	case EHALRoomPhase::Loading: Result.Screen=EHALRoomScreen::Loading; break;
	case EHALRoomPhase::Countdown: Result.Screen=EHALRoomScreen::Countdown; break;
	case EHALRoomPhase::Playing:
		Result.Screen=View.bSpectating?EHALRoomScreen::Spectator:EHALRoomScreen::Match;
		Result.bShowLocalMenu=View.bMenuVisible; break;
	case EHALRoomPhase::Open:
		if (View.LocalPhase==EHALRoomPlayerPhase::Ready)
		{ Result.Screen=EHALRoomScreen::Practice; Result.bShowLocalMenu=View.bMenuVisible; }
		else if (View.bCatalogLoaded && (View.LocalPhase==EHALRoomPlayerPhase::Selecting || View.LocalPhase==EHALRoomPlayerPhase::PreparingAvatar))
		{ Result.Screen=EHALRoomScreen::Selection; }
		break;
	default: break;
	}
	return Result;
}
bool UHALPracticeRoomWidget::IsRoomChild(const UUserWidget* Child) const
{
	if (!IsValid(Child) || Child==this || !Child->GetClass()->ImplementsInterface(UHALRoomScreenInterface::StaticClass())) { return false; }
	for (const UWidget* Item=Child;Item;)
	{
		if (Item==this) { return true; }
		Item=Item->GetParent()?static_cast<const UWidget*>(Item->GetParent()):Item->GetTypedOuter<UUserWidget>();
	}
	return false;
}
bool UHALPracticeRoomWidget::RegisterRoomScreen(EHALRoomScreen Screen, UUserWidget* Child)
{
	if (uint8(Screen)>uint8(EHALRoomScreen::Transition) || !IsRoomChild(Child) || Child==LocalMenuScreen) { return false; }
	for (const auto& Pair:RoomScreens) { if (Pair.Key!=Screen && Pair.Value==Child) { return false; } }
	if (RoomScreens.FindRef(Screen)==Child) { return true; }
	if (auto* Previous=RoomScreens.FindRef(Screen).Get())
	{
		IHALRoomScreenInterface::Execute_SetScreenActive(Previous,false); Previous->SetVisibility(ESlateVisibility::Collapsed);
		if (Previous==ActiveRoomScreen) { ActiveRoomScreen=nullptr; }
	}
	RoomScreens.Add(Screen,Child);
	if (auto* Native=Cast<UHALGameFlowScreenWidget>(Child)) { Native->SetRoomRoot(this); }
	IHALRoomScreenInterface::Execute_SetScreenActive(Child,false); Child->SetVisibility(ESlateVisibility::Collapsed);
	ApplyRoomPresentation(bHasPublishedView?PublishedView:GetRoomView());
	return true;
}
bool UHALPracticeRoomWidget::RegisterLocalMenu(UUserWidget* Child)
{
	if (!IsRoomChild(Child)) { return false; }
	for (const auto& Pair:RoomScreens) { if (Pair.Value==Child) { return false; } }
	if (LocalMenuScreen==Child) { return true; }
	if (LocalMenuScreen)
	{ IHALRoomScreenInterface::Execute_SetScreenActive(LocalMenuScreen,false); LocalMenuScreen->SetVisibility(ESlateVisibility::Collapsed); }
	LocalMenuScreen=Child;
	if (auto* Native=Cast<UHALGameFlowScreenWidget>(Child)) { Native->SetRoomRoot(this); }
	IHALRoomScreenInterface::Execute_SetScreenActive(Child,false); Child->SetVisibility(ESlateVisibility::Collapsed);
	// Force activation of the newly registered menu if it is already requested.
	Presentation.bShowLocalMenu=false;
	ApplyRoomPresentation(bHasPublishedView?PublishedView:GetRoomView());
	return true;
}
bool UHALPracticeRoomWidget::ManagesMatchHUD() const
{ return IsValid(RoomScreens.FindRef(EHALRoomScreen::Match)); }
void UHALPracticeRoomWidget::ApplyRoomPresentation(const FHALRoomView& View)
{
	if (RoomScreens.IsEmpty() && !LocalMenuScreen) { return; }
	const auto Next=ResolvePresentation(View);
	auto* NextScreen=RoomScreens.FindRef(Next.Screen).Get();
	const bool Changed=ActiveRoomScreen!=NextScreen;
	const bool MenuChanged=!bPresentationApplied || Presentation.bShowLocalMenu!=Next.bShowLocalMenu;
	if (Changed && ActiveRoomScreen)
	{ if (auto* Native=Cast<UHALGameFlowScreenWidget>(ActiveRoomScreen)) { Native->UpdateActive(false); }
	  IHALRoomScreenInterface::Execute_SetScreenActive(ActiveRoomScreen,false); ActiveRoomScreen->SetVisibility(ESlateVisibility::Collapsed); }
	if (LocalMenuScreen && MenuChanged && !Next.bShowLocalMenu)
	{ IHALRoomScreenInterface::Execute_SetScreenActive(LocalMenuScreen,false); LocalMenuScreen->SetVisibility(ESlateVisibility::Collapsed); }
	Presentation=Next; bPresentationApplied=true; ActiveRoomScreen=NextScreen;
	if (NextScreen)
	{
		if (auto* Switcher=Cast<UWidgetSwitcher>(NextScreen->GetParent())) { Switcher->SetActiveWidget(NextScreen); }
		if (auto* Native=Cast<UHALGameFlowScreenWidget>(NextScreen)) { Native->PrepareView(View); }
		IHALRoomScreenInterface::Execute_ApplyView(NextScreen,View);
		if (auto* Native=Cast<UHALGameFlowScreenWidget>(NextScreen)) { Native->FinishView(); }
		if (Changed)
		{ NextScreen->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		  if (auto* Native=Cast<UHALGameFlowScreenWidget>(NextScreen)) { Native->UpdateActive(true); }
		  IHALRoomScreenInterface::Execute_SetScreenActive(NextScreen,true); }
	}
	if (LocalMenuScreen && Next.bShowLocalMenu)
	{
		if (auto* Native=Cast<UHALGameFlowScreenWidget>(LocalMenuScreen)) { Native->PrepareView(View); }
		IHALRoomScreenInterface::Execute_ApplyView(LocalMenuScreen,View);
		if (auto* Native=Cast<UHALGameFlowScreenWidget>(LocalMenuScreen)) { Native->FinishView(); }
		if (MenuChanged)
		{ LocalMenuScreen->SetVisibility(ESlateVisibility::SelfHitTestInvisible); IHALRoomScreenInterface::Execute_SetScreenActive(LocalMenuScreen,true); }
	}
	if (Changed || MenuChanged)
	{
		auto* FocusScreen=Next.bShowLocalMenu?LocalMenuScreen.Get():NextScreen;
		SetMenuFocusTarget(FocusScreen?IHALRoomScreenInterface::Execute_GetDefaultFocusTarget(FocusScreen):nullptr);
	}
}
void UHALPracticeRoomWidget::ClearRoomScreens()
{
	if (ActiveRoomScreen) { IHALRoomScreenInterface::Execute_SetScreenActive(ActiveRoomScreen,false); }
	if (LocalMenuScreen && Presentation.bShowLocalMenu) { IHALRoomScreenInterface::Execute_SetScreenActive(LocalMenuScreen,false); }
	RoomScreens.Reset(); LocalMenuScreen=nullptr; ActiveRoomScreen=nullptr; Presentation={}; bPresentationApplied=false;
}
void UHALPracticeRoomWidget::RegisterDesignerScreens()
{
	if (!WidgetTree) { return; }
	WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (auto* HUD=Cast<UHALMatchHUDWidget>(Widget)) { RegisterRoomScreen(EHALRoomScreen::Match,HUD); return; }
		auto* Child=Cast<UHALGameFlowScreenWidget>(Widget); if (!Child) { return; }
		if (Child->Panel==EHALGameFlowPanel::LocalMenu) { RegisterLocalMenu(Child); return; }
		EHALRoomScreen Screen=EHALRoomScreen::Transition;
		switch (Child->Panel)
		{
		case EHALGameFlowPanel::Selection: Screen=EHALRoomScreen::Selection; break;
		case EHALGameFlowPanel::Practice: Screen=EHALRoomScreen::Practice; break;
		case EHALGameFlowPanel::Loading: Screen=EHALRoomScreen::Loading; break;
		case EHALGameFlowPanel::Countdown: Screen=EHALRoomScreen::Countdown; break;
		case EHALGameFlowPanel::Spectator: Screen=EHALRoomScreen::Spectator; break;
		case EHALGameFlowPanel::Results: Screen=EHALRoomScreen::Results; break;
		default: break;
		}
		RegisterRoomScreen(Screen,Child);
	});
}
