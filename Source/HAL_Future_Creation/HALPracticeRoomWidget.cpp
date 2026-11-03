#include "HALPracticeRoomWidget.h"
#include "HALArcadeStyle.h"
#include "HALNetworkGameInstance.h"
#include "HALFrontEndConfig.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScaleBox.h"
#include "HALPlayerController.h"
#include "HALPlayerState.h"
#include "HALMatchGameState.h"
#include "HALRoomComponent.h"
#include "HALVehicleCatalog.h"
#include "HALAbilitySystemComponent.h"
#include "TestVehiclePawn.h"
#include "ArcadeVehicleMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	const UHALRoomComponent* Room(const UUserWidget* W)
	{ const auto* S = W->GetWorld() ? W->GetWorld()->GetGameState<AHALMatchGameState>() : nullptr; return S ? S->GetRoom() : nullptr; }
	FString ResultText(EHALRoomResult R)
	{
		switch (R)
		{
		case EHALRoomResult::Accepted: return TEXT("");
		case EHALRoomResult::SpawnBlocked: return TEXT("Spawn point blocked. Clear it and confirm again.");
		case EHALRoomResult::MissingSpawn: return TEXT("Your spawn point is unavailable. Ask the host to check the room.");
		case EHALRoomResult::InvalidVehicle: return TEXT("Vehicle unavailable. Select another vehicle.");
		case EHALRoomResult::InvalidConfiguration: return TEXT("Match destination unavailable. Ask the host to check the room settings.");
		case EHALRoomResult::SpawnFailed: return TEXT("Could not prepare the vehicle. Confirm to retry.");
		case EHALRoomResult::NotHost: return TEXT("Only the host can start.");
		case EHALRoomResult::StaleSelection: return TEXT("Selection updated. Please retry.");
		case EHALRoomResult::NotReady: return TEXT("Waiting for all players to confirm and prepare their vehicles.");
		default: return TEXT("Request unavailable in the current room state.");
		}
	}
}
bool UHALPracticeRoomWidget::MenuVisible() const
{
	const auto* PC = GetOwningPlayer<AHALPlayerController>(); const auto* R = Room(this);
	return !PC || PC->WantsRoomMenu() || !R || (R->GetData().Phase != EHALRoomPhase::Open && R->GetData().Phase != EHALRoomPhase::Playing);
}
bool UHALPracticeRoomWidget::CanSelect() const
{
	const auto* PC = GetOwningPlayer<AHALPlayerController>(); const auto* PS = PC ? PC->GetPlayerState<AHALPlayerState>() : nullptr;
	const auto* R = Room(this);
	return PC && PS && R && !PC->IsRoomRequestPending() && R->GetData().Phase == EHALRoomPhase::Open
		&& PS->GetRoomData().Phase == EHALRoomPlayerPhase::Selecting;
}
bool UHALPracticeRoomWidget::CanConfirm() const
{ const auto* PS = GetOwningPlayerState<AHALPlayerState>(); return CanSelect() && PS && !PS->GetRoomData().VehicleId.IsNone(); }
bool UHALPracticeRoomWidget::CanStart() const
{
	const auto* R = Room(this); const auto* PC = GetOwningPlayer<AHALPlayerController>();
	const auto* PS = PC ? PC->GetPlayerState<AHALPlayerState>() : nullptr;
	if (!PC || !PS || !R || PC->IsRoomRequestPending() || R->GetData().Host != PS || R->GetData().Phase != EHALRoomPhase::Open
		|| R->GetData().Members.Num() < R->GetData().Rules.MinimumPlayers()) { return false; }
	for (const auto& M : R->GetData().Members)
	{ if (!M || M->GetRoomData().Phase != EHALRoomPlayerPhase::Ready || !M->GetRoomData().ReadyAvatar) { return false; } }
	return true;
}
void UHALPracticeRoomWidget::CycleVehicle(int32 Direction)
{
	if (!CanSelect()) { return; }
	auto* PC = GetOwningPlayer<AHALPlayerController>(); const auto* R = Room(this);
	const auto* C = R ? LoadedCatalog.Get() : nullptr;
	if (!C || C->Vehicles.IsEmpty()) { return; }
	const FName Current = PC->GetPlayerState<AHALPlayerState>()->GetRoomData().VehicleId;
	const int32 Index = C->Vehicles.IndexOfByPredicate([Current](const FHALVehicleCatalogEntry& E) { return E.VehicleId == Current; });
	const int32 Next = Index == INDEX_NONE ? 0 : (Index+Direction+C->Vehicles.Num())%C->Vehicles.Num();
	PC->SelectRoomVehicle(C->Vehicles[Next].VehicleId);
}
FText UHALPracticeRoomWidget::VehicleText() const
{
	const auto* PS = GetOwningPlayerState<AHALPlayerState>(); const auto* R = Room(this);
	const FName Id = PS ? PS->GetRoomData().VehicleId : NAME_None;
	const auto* C = R ? LoadedCatalog.Get() : nullptr;
	const auto* E = C ? C->Vehicles.FindByPredicate([Id](const FHALVehicleCatalogEntry& V) { return V.VehicleId == Id; }) : nullptr;
	return E ? (E->DisplayName.IsEmpty() ? FText::FromName(Id) : E->DisplayName) : FText::FromString(TEXT("Choose a vehicle"));
}
FText UHALPracticeRoomWidget::SeatText(int32 Seat) const
{
	const auto* R = Room(this);
	if (R)
	{
		for (const auto& P : R->GetData().Members)
		{
			if (!P || P->GetRoomData().Seat != Seat) { continue; }
			const auto& D = P->GetRoomData();
			const TCHAR* Phase = D.Phase == EHALRoomPlayerPhase::Ready ? (R->GetData().Phase==EHALRoomPhase::Open ? TEXT("Practicing") : R->GetData().Phase==EHALRoomPhase::Playing ? TEXT("Playing") : D.bClientLoaded ? TEXT("Ready") : TEXT("Loading"))
				: D.Phase == EHALRoomPlayerPhase::PreparingAvatar ? TEXT("Preparing") : TEXT("Selecting");
			const FString Name=P->GetPlayerName().Len()>18?P->GetPlayerName().Left(17)+TEXT("..."):P->GetPlayerName();
			const auto* Entry=LoadedCatalog?LoadedCatalog->Vehicles.FindByPredicate([&D](const FHALVehicleCatalogEntry& V){ return V.VehicleId==D.VehicleId; }):nullptr;
			FString Model=Entry && !Entry->DisplayName.IsEmpty()?Entry->DisplayName.ToString():D.VehicleId.IsNone()?TEXT("Choose a vehicle"):D.VehicleId.ToString();
			if (Model.Len()>20) { Model=Model.Left(17)+TEXT("..."); }
			return FText::FromString(FString::Printf(TEXT("P%d  %s\n%s%s\n%s"),D.Number,*Name,*Model,P==R->GetData().Host?TEXT(" / Host"):TEXT(""),Phase));
		}
	}
	return FText::FromString(FString::Printf(TEXT("P%d - Empty"),Seat+1));
}
FSlateColor UHALPracticeRoomWidget::SeatColor(int32 Seat) const
{
	if (const auto* R = Room(this))
	{ for (const auto& P : R->GetData().Members) { if (P && P->GetRoomData().Seat == Seat) { return FSlateColor(P->GetRoomData().Color); } } }
	return FSlateColor(FLinearColor(.5f,.5f,.5f));
}
FText UHALPracticeRoomWidget::StatusText() const
{
	const auto* R = Room(this); const auto* PC = GetOwningPlayer<AHALPlayerController>();
	const auto* PS = PC ? PC->GetPlayerState<AHALPlayerState>() : nullptr;
	if (!R || !PS) { return FText::FromString(TEXT("Connecting to room...")); }
	if (R->GetData().Phase==EHALRoomPhase::Loading)
	{
		int32 Loaded=0; for (const auto& P:R->GetData().Members) { if (P && P->GetRoomData().bClientLoaded) { ++Loaded; } }
		return FText::FromString(FString::Printf(TEXT("Loading vehicles: %d / %d"),Loaded,R->GetData().ExpectedPlayers));
	}
	if (R->GetData().Phase == EHALRoomPhase::Countdown)
	{
		const auto* S=GetWorld()->GetGameState<AHALMatchGameState>();
		return FText::FromString(FString::Printf(TEXT("Match starts in %d"),FMath::CeilToInt(FMath::Max(0.0,R->GetData().CountdownEndServerTime-S->GetServerWorldTimeSeconds()))));
	}
	if (R->GetData().Phase == EHALRoomPhase::Playing) { return FText::FromString(TEXT("Match in progress.")); }
	if (R->GetData().Phase == EHALRoomPhase::Returning) { return FText::FromString(TEXT("Leaving this arena...")); }
	if (R->GetData().Phase != EHALRoomPhase::Open) { return FText::FromString(TEXT("Preparing the match. New players and vehicle changes are closed.")); }
	if (PC->IsRoomRequestPending()) { return FText::FromString(TEXT("Updating selection...")); }
	const auto Last = PC->GetLastRoomResult();
	const auto Result = PS->GetRoomData().AvatarResult != EHALRoomResult::Accepted ? PS->GetRoomData().AvatarResult
		: Last == EHALRoomResult::Inactive ? EHALRoomResult::Accepted : Last;
	if (Result != EHALRoomResult::Accepted) { return FText::FromString(ResultText(Result)); }
	if (!R->GetData().FlowMessage.IsEmpty()) { return FText::FromString(R->GetData().FlowMessage); }
	return FText::FromString(FString::Printf(TEXT("Practice: no HP damage. Start needs %d confirmed player(s)."),R->GetData().Rules.MinimumPlayers()));
}
int32 UHALPracticeRoomWidget::NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Culling,
	FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const int32 Base = Super::NativePaint(Args,G,Culling,Elements,Layer,Style,bParentEnabled);
	const auto* R=Room(this); auto* PC=GetOwningPlayer();
	if (!R || !PC || MenuVisible()) { return Base; }
	for (const auto& PS : R->GetData().Members)
	{
		const auto* Pawn=PS ? PS->GetPawn() : nullptr; if (!Pawn) { continue; }
		FVector Center,Extent; Pawn->GetActorBounds(true,Center,Extent); FVector2D Screen;
		if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC,Center+FVector(0,0,Extent.Z+30),Screen,true))
		{
			FSlateDrawElement::MakeText(Elements,Base+1,G.ToPaintGeometry(FVector2D(1,1),FSlateLayoutTransform(Screen)),
				FString::Printf(TEXT("P%d"),PS->GetRoomData().Number),FCoreStyle::GetDefaultFontStyle("Bold",20),ESlateDrawEffect::None,PS->GetRoomData().Color);
		}
	}
	return Base+1;
}
