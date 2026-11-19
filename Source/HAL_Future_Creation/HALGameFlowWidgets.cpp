#include "HALGameFlowWidgets.h"
#include "HALPracticeRoomWidget.h"
#include "HALMatchHUDWidget.h"
#include "HALVehicleCatalog.h"
#include "HALVehicleHUDStyleLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace HALFlowDisplay
{
	void Text(UUserWidget* Owner,FName Name,const FString& Value)
	{ if (auto* W=Cast<UTextBlock>(Owner->GetWidgetFromName(Name))) { W->SetText(FText::FromString(Value)); } }
	bool Allowed(EHALGameFlowUIAction Action,const FHALRoomView& V)
	{
		switch (Action)
		{
		case EHALGameFlowUIAction::Previous: case EHALGameFlowUIAction::Next: return V.bCanSelect;
		case EHALGameFlowUIAction::Confirm: return V.bCanConfirm;
		case EHALGameFlowUIAction::CancelSelection: return V.bCanCancel;
		case EHALGameFlowUIAction::Start: return V.bCanStart;
		case EHALGameFlowUIAction::Resume: return V.bCanResume;
		case EHALGameFlowUIAction::Leave: return V.bCanLeave;
		case EHALGameFlowUIAction::Return: return V.bCanReturnToRoom;
		case EHALGameFlowUIAction::PreviousSpectator: case EHALGameFlowUIAction::NextSpectator: return V.bCanCycleSpectator;
		default: return true;
		}
	}
	// Narrow compatibility with the existing presentation-only Blueprint preview actor.
	void PreviewValue(AActor* Actor,FName Function,bool bActive,FName Vehicle=NAME_None)
	{
		UFunction* F=Actor?Actor->FindFunction(Function):nullptr; if (!F || F->NumParms!=1) { return; }
		FStructOnScope Args(F);
		for (TFieldIterator<FProperty> It(F);It;++It)
		{
			if (!It->HasAnyPropertyFlags(CPF_Parm) || It->HasAnyPropertyFlags(CPF_ReturnParm)) { continue; }
			if (auto* P=CastField<FBoolProperty>(*It)) { P->SetPropertyValue_InContainer(Args.GetStructMemory(),bActive); }
			else if (auto* NameProperty=CastField<FNameProperty>(*It)) { NameProperty->SetPropertyValue_InContainer(Args.GetStructMemory(),Vehicle); }
			else { return; }
		}
		Actor->ProcessEvent(F,Args.GetStructMemory());
	}
}
void UHALGameFlowScreenWidget::SetRoomRoot(UHALPracticeRoomWidget* Root) { RoomRoot=Root; BindActions(); }
void UHALGameFlowScreenWidget::NativeConstruct() { Super::NativeConstruct(); BindActions(); }
void UHALGameFlowScreenWidget::BindActions()
{
	static const FName Names[]={TEXT("Previous"),TEXT("Next"),TEXT("Confirm"),TEXT("CancelSelection"),TEXT("Start"),TEXT("Resume"),TEXT("Leave"),TEXT("Return"),TEXT("PreviousSpectator"),TEXT("NextSpectator"),TEXT("Back")};
	for (const auto& Pair:Actions)
	{
		if (auto* Button=Cast<UButton>(GetWidgetFromName(Pair.Key)); Button && uint8(Pair.Value)<UE_ARRAY_COUNT(Names))
		{ FScriptDelegate D; D.BindUFunction(this,Names[uint8(Pair.Value)]); Button->OnClicked.AddUnique(D); }
	}
}
void UHALGameFlowScreenWidget::PrepareView(const FHALRoomView& View)
{
	LatestView=View;
	if (Panel==EHALGameFlowPanel::Selection && !OwnedPreview && PreviewClass && GetWorld() && !GetWorld()->bIsTearingDown)
	{
		FActorSpawnParameters Spawn; Spawn.ObjectFlags|=RF_Transient; Spawn.Owner=GetOwningPlayer();
		OwnedPreview=GetWorld()->SpawnActor<AActor>(PreviewClass,FTransform(FVector(0,0,-100000)),Spawn);
		if (OwnedPreview)
		{
			OwnedPreview->SetReplicates(false); OwnedPreview->SetActorEnableCollision(false);
			HALFlowDisplay::PreviewValue(OwnedPreview,TEXT("SetPreviewActive"),false);
			if (auto* P=FindFProperty<FObjectPropertyBase>(GetClass(),TEXT("PreviewActor")); P && OwnedPreview->IsA(P->PropertyClass))
			{ P->SetObjectPropertyValue_InContainer(this,OwnedPreview); }
		}
	}
	if (OwnedPreview) { HALFlowDisplay::PreviewValue(OwnedPreview,TEXT("SetPreviewVehicle"),false,View.SelectedVehicleId); }
}
void UHALGameFlowScreenWidget::ApplyView_Implementation(const FHALRoomView& View) { LatestView=View; }
void UHALGameFlowScreenWidget::SetScreenActive_Implementation(bool bActive)
{ UpdateActive(bActive); }
void UHALGameFlowScreenWidget::UpdateActive(bool bActive)
{
	if (bScreenActive==bActive) { return; }
	bScreenActive=bActive;
	if (OwnedPreview) { HALFlowDisplay::PreviewValue(OwnedPreview,TEXT("SetPreviewActive"),bActive); }
	if (WidgetTree) { WidgetTree->ForEachWidget([&](UWidget* W){ if (auto* HUD=Cast<UHALMatchHUDWidget>(W)) { IHALRoomScreenInterface::Execute_SetScreenActive(HUD,bActive); } }); }
}
void UHALGameFlowScreenWidget::FinishView()
{
	using namespace HALFlowDisplay;
	for (const auto& Pair:Actions)
	{
		if (auto* Button=Cast<UButton>(GetWidgetFromName(Pair.Key)))
		{
			Button->SetIsEnabled(Allowed(Pair.Value,LatestView));
			const bool HideStart=Pair.Value==EHALGameFlowUIAction::Start && !LatestView.bHost;
			const bool HideReturn=Pair.Value==EHALGameFlowUIAction::Return && !LatestView.bCanReturnToRoom;
			Button->SetVisibility(HideStart || HideReturn?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
		}
	}
	Text(this,TEXT("StatusText"),LatestView.Status.ToString());
	FString Title,Detail;
	switch (Panel)
	{
	case EHALGameFlowPanel::LocalMenu: Title=TEXT("MENU"); Detail=TEXT("The match continues while this menu is open."); break;
	case EHALGameFlowPanel::Loading: Title=TEXT("PREPARING MATCH"); Detail=FString::Printf(TEXT("PLAYERS LOADED  %d / %d"),LatestView.LoadedPlayers,LatestView.ExpectedPlayers); break;
	case EHALGameFlowPanel::Countdown: Title=FString::FromInt(FMath::CeilToInt(FMath::Max(0.f,LatestView.CountdownRemaining))); Detail=TEXT("GET READY"); break;
	case EHALGameFlowPanel::Spectator: Title=TEXT("SPECTATING"); Detail=LatestView.SpectatedNumber>0?FString::Printf(TEXT("PLAYER %d"),LatestView.SpectatedNumber):TEXT("ARENA VIEW"); break;
	case EHALGameFlowPanel::Results:
		Title=TEXT("MATCH RESULTS"); Detail=LatestView.ResultSummary.ToString();
		for (const auto& P:LatestView.Match.Players)
		{ Detail+=FString::Printf(TEXT("\nP%d  %s  %.0f / %.0f HP%s"),P.Number,*P.VehicleId.ToString(),P.HP,P.MaxHP,P.bWinner?TEXT("  WINNER"):P.Life==EHALRoundLife::Forfeited?TEXT("  FORFEITED"):P.Life==EHALRoundLife::Eliminated?TEXT("  ELIMINATED"):TEXT("")); }
		Detail+=FString::Printf(TEXT("\nRETURNING IN %d"),FMath::CeilToInt(FMath::Max(0.f,LatestView.ResultsRemaining))); break;
	case EHALGameFlowPanel::Transition: Title=TEXT("PLEASE WAIT"); Detail=LatestView.Status.ToString(); break;
	default: break;
	}
	if (!Title.IsEmpty()) { Text(this,TEXT("TitleText"),Title); Text(this,TEXT("DetailText"),Detail); }
	if (WidgetTree)
	{ WidgetTree->ForEachWidget([&](UWidget* W)
		{ if (auto* Participants=Cast<UHALGameFlowParticipantsWidget>(W)) { Participants->ApplyRoomView(LatestView); }
		  if (auto* HUD=Cast<UHALMatchHUDWidget>(W)) { IHALRoomScreenInterface::Execute_ApplyView(HUD,LatestView); } }); }
}
UWidget* UHALGameFlowScreenWidget::GetDefaultFocusTarget_Implementation() const
{
	// Designer order is the intended navigation order; map iteration never determines focus.
	UWidget* Result=nullptr;
	if (WidgetTree) { WidgetTree->ForEachWidget([&](UWidget* W){ if (!Result && W->IsVisible() && W->GetIsEnabled() && Cast<UButton>(W)) { Result=W; } }); }
	return Result;
}
void UHALGameFlowScreenWidget::Request(EHALGameFlowUIAction Action)
{
	if (!RoomRoot || !HALFlowDisplay::Allowed(Action,LatestView)) { return; }
	for (const auto& Pair:Actions) { if (Pair.Value==Action) { RoomRoot->SetMenuFocusTarget(GetWidgetFromName(Pair.Key)); break; } }
	switch (Action)
	{
	case EHALGameFlowUIAction::Previous: RoomRoot->CycleVehicle(-1); break;
	case EHALGameFlowUIAction::Next: RoomRoot->CycleVehicle(1); break;
	case EHALGameFlowUIAction::Confirm: RoomRoot->ConfirmSelection(); break;
	case EHALGameFlowUIAction::CancelSelection: RoomRoot->CancelSelection(); break;
	case EHALGameFlowUIAction::Start: RoomRoot->StartRoomMatch(); break;
	case EHALGameFlowUIAction::Resume: RoomRoot->ResumeDriving(); break;
	case EHALGameFlowUIAction::Leave: RoomRoot->LeaveCurrentRoom(); break;
	case EHALGameFlowUIAction::Return: RoomRoot->ReturnToRoom(); break;
	case EHALGameFlowUIAction::PreviousSpectator: RoomRoot->CycleSpectator(-1); break;
	case EHALGameFlowUIAction::NextSpectator: RoomRoot->CycleSpectator(1); break;
	default: RoomRoot->RequestMenuBack(); break;
	}
}
void UHALGameFlowScreenWidget::NativeDestruct()
{
	UpdateActive(false);
	if (OwnedPreview)
	{
		if (auto* P=FindFProperty<FObjectPropertyBase>(GetClass(),TEXT("PreviewActor")); P && P->GetObjectPropertyValue_InContainer(this)==OwnedPreview) { P->SetObjectPropertyValue_InContainer(this,nullptr); }
		OwnedPreview->Destroy(); OwnedPreview=nullptr;
	}
	RoomRoot=nullptr; LatestView={}; Super::NativeDestruct();
}
void UHALGameFlowSeatWidget::ApplySeat(const FHALRoomSeatView& Seat,const TArray<FHALVehicleDisplayView>& Vehicles)
{
	FString Label=FString::Printf(TEXT("P%d"),Seat.Number);
	if (Seat.bHost) { Label+=TEXT("  HOST"); } if (Seat.bLocal) { Label+=TEXT("  YOU"); }
	FString Vehicle=TEXT("NOT JOINED"),Status;
	if (Seat.bOccupied)
	{
		const auto* Entry=Vehicles.FindByPredicate([&](const auto& V){ return V.VehicleId==Seat.VehicleId; });
		Vehicle=Seat.VehicleId.IsNone()?TEXT("NO VEHICLE SELECTED"):Entry?Entry->DisplayName.ToString():Seat.VehicleId.ToString();
		switch (Seat.Phase)
		{ case EHALRoomPlayerPhase::Selecting: Status=TEXT("SELECTING"); break; case EHALRoomPlayerPhase::PreparingAvatar: Status=TEXT("PREPARING"); break; case EHALRoomPlayerPhase::Ready: Status=TEXT("READY"); break; default: Status=TEXT("JOINING"); break; }
	}
	HALFlowDisplay::Text(this,TEXT("NumberText"),Label); HALFlowDisplay::Text(this,TEXT("VehicleText"),Vehicle); HALFlowDisplay::Text(this,TEXT("SeatStateText"),Status);
	if (auto* W=Cast<UTextBlock>(GetWidgetFromName(TEXT("NumberText")))) { W->SetColorAndOpacity(Seat.bOccupied?Seat.Color:FLinearColor::Gray); }
}
void UHALGameFlowParticipantsWidget::ApplyRoomView(const FHALRoomView& View) { Vehicles=View.Vehicles; ApplySeats(View.Seats); }
void UHALGameFlowParticipantsWidget::ApplySeats(const TArray<FHALRoomSeatView>& Seats)
{
	for (int32 I=0;I<4;++I)
	{
		FHALRoomSeatView Seat; Seat.Seat=I; Seat.Number=I+1;
		if (const auto* Found=Seats.FindByPredicate([I](const auto& S){ return S.Seat==I; })) { Seat=*Found; }
		if (auto* W=Cast<UHALGameFlowSeatWidget>(GetWidgetFromName(FName(*FString::Printf(TEXT("Seat%d"),I+1))))) { W->ApplySeat(Seat,Vehicles); }
	}
}
void UHALGameFlowPlayerStatusRow::ApplyParticipant(const FHALRoundParticipant& P,const FHALRoomView& View,const UHALVehicleCatalog* Catalog)
{
	HALFlowDisplay::Text(this,NumberTextName,FString::Printf(TEXT("P%d"),P.Number));
	HALFlowDisplay::Text(this,HealthTextName,FString::Printf(TEXT("%.0f / %.0f"),FMath::Max(0.f,P.HP),FMath::Max(0.f,P.MaxHP)));
	if (auto* Bar=Cast<UProgressBar>(GetWidgetFromName(HealthBarName)))
	{ Bar->SetPercent(P.MaxHP>0?FMath::Clamp(P.HP/P.MaxHP,0.f,1.f):0); Bar->SetFillColorAndOpacity(P.Color); }
	if (auto* Background=Cast<UImage>(GetWidgetFromName(BackgroundName))) { UHALVehicleHUDStyleLibrary::ApplyVehicleOutline(Background,Catalog,P.VehicleId); }
	if (IconVehicle!=P.VehicleId)
	{
		IconVehicle=P.VehicleId;
		if (auto* Icon=Cast<UImage>(GetWidgetFromName(IconName)))
		{
			const auto* V=View.Vehicles.FindByPredicate([&](const auto& Entry){ return Entry.VehicleId==P.VehicleId; });
			Icon->SetBrushFromSoftTexture(V?(V->Icon.IsNull()?V->PreviewImage:V->Icon):TSoftObjectPtr<UTexture2D>());
		}
	}
	SetRenderOpacity(P.Life==EHALRoundLife::Alive?1.f:.5f);
}
