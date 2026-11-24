#include "HALMatchHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/World.h"
#include "HALMatchGameState.h"
#include "HALRoomComponent.h"
#include "HALAbilitySystemComponent.h"
#include "HALBatarang.h"
#include "HALGameFlowPermissions.h"
#include "TestVehiclePawn.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UHALMatchHUDWidget::RebuildWidget()
{
	// A future Designer subclass may replace the layout without changing the data source.
	TSharedRef<SWidget> Clock = WidgetTree && WidgetTree->RootWidget ? Super::RebuildWidget() : MakeNativeClock();
	auto Overlay = SNew(SOverlay);
	ClockPanel = SNew(SBox)[Clock];
	Overlay->AddSlot()[ClockPanel.ToSharedRef()];
	EnergyPanel = SNew(SBox).WidthOverride(292)
	[
		SNew(SBorder).Padding(FMargin(12,7)).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(.015f,.022f,.04f,.86f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[SAssignNew(EnergyText, STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
				.ColorAndOpacity(FLinearColor::White).Justification(ETextJustify::Center)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)
			[SNew(SBox).HeightOverride(9)[SAssignNew(EnergyBar, SProgressBar).Percent(0.f)]]
		]
	];
	Overlay->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,36)[EnergyPanel.ToSharedRef()];
	return Overlay;
}
TSharedRef<SWidget> UHALMatchHUDWidget::MakeNativeClock()
{
	return SNew(SOverlay)
	+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0,24,0,0))
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(1.f,1.f,1.f,.5f))
		.Padding(FMargin(24,8))
		[
			SAssignNew(CountdownText,STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular",32))
			.ColorAndOpacity(FLinearColor::Black)
			.Justification(ETextJustify::Center)
		]
	];
}
void UHALMatchHUDWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	CountdownText.Reset();
	ClockPanel.Reset(); EnergyPanel.Reset(); EnergyText.Reset(); EnergyBar.Reset();
}
FHALUltimateEnergyView UHALMatchHUDWidget::GetLocalUltimateEnergy() const
{
	if (UsesRoomView()) { return GetCachedRoomView().LocalUltimateEnergy; }
	const auto* Pawn = Cast<ATestVehiclePawn>(GetOwningPlayerPawn());
	const auto* ASC = Pawn ? Cast<UHALAbilitySystemComponent>(Pawn->GetAbilitySystemComponent()) : nullptr;
	return ASC && ASC->GetAvatarActor() == Pawn ? ASC->GetUltimateEnergyView() : FHALUltimateEnergyView();
}
FText UHALMatchHUDWidget::GetCountdownText() const
{
	if (UsesRoomView())
	{
		const auto& RoomView=GetCachedRoomView();
		if (!RoomView.bRoomAvailable || RoomView.bShowResults || RoomView.Phase!=EHALRoomPhase::Playing
			|| !RoomView.Match.bActive || RoomView.Match.bComplete || !FMath::IsFinite(RoomView.MatchRemaining)) { return FText::GetEmpty(); }
		const int32 Seconds=FMath::CeilToInt(FMath::Clamp(RoomView.MatchRemaining,0.f,3600.f));
		return FText::FromString(FString::Printf(TEXT("%02d:%02d"),Seconds/60,Seconds%60));
	}
	const auto* State=GetWorld()?GetWorld()->GetGameState<AHALMatchGameState>():nullptr;
	if (!State) { return FText::GetEmpty(); }
	const auto& Data=State->GetRoom()->GetData();
	if (Data.Phase!=EHALRoomPhase::Playing || !Data.Match.bActive || Data.Match.bComplete) { return FText::GetEmpty(); }
	const double Remaining=Data.Match.EndServerTime-State->GetServerWorldTimeSeconds();
	if (!FMath::IsFinite(Remaining)) { return FText::GetEmpty(); }
	const int32 Seconds=FMath::CeilToInt(FMath::Clamp(Remaining,0.0,3600.0));
	return FText::FromString(FString::Printf(TEXT("%02d:%02d"),Seconds/60,Seconds%60));
}
void UHALMatchHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(false);
	RefreshCountdown();
	if (auto* World=GetWorld(); World && !UsesRoomView()) { World->GetTimerManager().SetTimer(RefreshTimer,this,&ThisClass::RefreshCountdown,.1f,true); }
}
void UHALMatchHUDWidget::NativeDestruct()
{
	if (auto* World=GetWorld()) { World->GetTimerManager().ClearTimer(RefreshTimer); }
	Super::NativeDestruct();
}
void UHALMatchHUDWidget::OnRoomViewApplied(const FHALRoomView& View)
{
	if (auto* World=GetWorld()) { World->GetTimerManager().ClearTimer(RefreshTimer); }
	ReceiveMatchView(View); RefreshCountdown();
}
void UHALMatchHUDWidget::OnScreenActiveChanged(bool bActive)
{
	if (auto* World=GetWorld()) { World->GetTimerManager().ClearTimer(RefreshTimer); }
	RefreshCountdown();
}
void UHALMatchHUDWidget::RefreshCountdown()
{
	const FText Text=GetCountdownText();
	const auto Energy = GetLocalUltimateEnergy();
	const bool bRoomManaged = UsesRoomView();
	const auto& RoomView = GetCachedRoomView();
	const bool ScreenActive = !bRoomManaged || IsRoomScreenActive();
	const bool Driving = ScreenActive && (bRoomManaged ? RoomView.bShowUltimateEnergy
		: Energy.Cost > 0 && HALGameFlow::Permissions(GetWorld(), GetOwningPlayerPawn()).bAbilities);
	const bool ShowEnergy = Driving && bShowNativeUltimateMeter;
	SetVisibility(ScreenActive && (!Text.IsEmpty() || Driving) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (ClockPanel) { ClockPanel->SetVisibility(ScreenActive && (!Text.IsEmpty() || (Driving && !bShowNativeUltimateMeter)) ? EVisibility::HitTestInvisible : EVisibility::Collapsed); }
	if (EnergyPanel) { EnergyPanel->SetVisibility(ShowEnergy ? EVisibility::HitTestInvisible : EVisibility::Collapsed); }
	if (ShowEnergy && EnergyText && EnergyBar)
	{
		const FString Cells = Energy.Cost == HALUltimate::SideShiftCost
			? FString::Printf(TEXT("   %d/4"), FMath::FloorToInt(Energy.Energy / Energy.Cost)) : FString();
		const TCHAR* State = Energy.bCanActivate ? TEXT("   READY") : Energy.bHasEnergy ? TEXT("   BLOCKED") : TEXT("   CHARGING");
		const auto* Car = Cast<ATestVehiclePawn>(GetOwningPlayerPawn());
		const auto* Dart = Car ? Car->GetActiveBatarang() : nullptr;
		if (IsValid(Dart) && Dart->GetState().Phase == EHALDartPhase::Pinned)
		{
			State = TEXT(""); // The pin's wing pulse carries its fuse cue.
		}
		EnergyText->SetText(FText::FromString(FString::Printf(TEXT("ULT   %d/100%s%s"), FMath::FloorToInt(Energy.Energy), *Cells, State)));
		float DisplayEnergy = Energy.Energy;
		if (Energy.bRecharging && Energy.FullRechargeSeconds > 0.f)
		{
			const auto* Match = !bRoomManaged && GetWorld() ? GetWorld()->GetGameState<AHALMatchGameState>() : nullptr;
			if (bRoomManaged || Match)
			{
				const double Now = bRoomManaged ? RoomView.ServerWorldTimeSeconds : Match->GetServerWorldTimeSeconds();
				const double Elapsed = FMath::Max(0., Now - Energy.SampleServerTime);
				DisplayEnergy += float(Elapsed * HALUltimate::Capacity / Energy.FullRechargeSeconds);
			}
		}
		EnergyBar->SetPercent(FMath::Clamp(DisplayEnergy / HALUltimate::Capacity, 0.f, 1.f));
	}
	if (CountdownText) { CountdownText->SetText(Text); }
}
