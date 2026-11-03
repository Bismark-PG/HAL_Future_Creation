#include "HALPracticeRoomWidget.h"
#include "HALPlayerController.h"
#include "HALPlayerState.h"
#include "HALMatchGameState.h"
#include "HALRoomComponent.h"
#include "HALVehicleCatalog.h"
#include "Engine/World.h"

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
	return R && PC && !PC->IsRoomRequestPending() && R->GetData().Phase==EHALRoomPhase::Playing
		&& R->GetData().Rules.Profile==EHALRoomProfile::Development && R->GetData().Host==GetOwningPlayerState<AHALPlayerState>();
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
FHALRoomView UHALPracticeRoomWidget::GetRoomView() const
{
	FHALRoomView View; const auto* R=ScreenRoom(this); const auto* PC=GetOwningPlayer<AHALPlayerController>();
	const auto* PS=GetOwningPlayerState<AHALPlayerState>();
	View.bRoomAvailable=R && R->IsEnabled() && PS; View.bCatalogLoaded=LoadedCatalog!=nullptr;
	View.bMenuVisible=bInputModeKnown?bUsingMenu:MenuVisible();
	View.bRequestPending=PC && PC->IsRoomRequestPending();
	View.bCanSelect=CanSelect(); View.bCanConfirm=CanConfirm(); View.bCanCancel=CanCancel(); View.bCanStart=CanStart();
	View.bCanResume=CanResume(); View.bCanReturnToRoom=CanReturnToRoom();
	View.bCanLeave=PC && GetWorld() && !GetWorld()->bIsTearingDown && !PC->IsActorBeingDestroyed();
	View.SelectedVehicleName=VehicleText(); View.Description=DescriptionText(); View.LoadedPreview=LoadedPreview; View.Status=StatusText();
	if (PS)
	{ const auto& P=PS->GetRoomData(); View.SelectedVehicleId=P.VehicleId; View.LocalPhase=P.Phase; View.LocalNumber=P.Number; View.LocalColor=P.Color; }
	if (R)
	{
		const auto& D=R->GetData(); View.Phase=D.Phase; View.RoundId=D.RoundId; View.MinimumPlayers=D.Rules.MinimumPlayers();
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
	{ PublishedView=View; bHasPublishedView=true; ReceiveRoomView(View); }
}
