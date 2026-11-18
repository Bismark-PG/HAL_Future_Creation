#include "HALMatchHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/World.h"
#include "HALMatchGameState.h"
#include "HALRoomComponent.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UHALMatchHUDWidget::RebuildWidget()
{
	// A future Designer subclass may replace the layout without changing the data source.
	if (WidgetTree && WidgetTree->RootWidget) { return Super::RebuildWidget(); }
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
}
FText UHALMatchHUDWidget::GetCountdownText() const
{
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
	if (auto* World=GetWorld()) { World->GetTimerManager().SetTimer(RefreshTimer,this,&ThisClass::RefreshCountdown,.1f,true); }
}
void UHALMatchHUDWidget::NativeDestruct()
{
	if (auto* World=GetWorld()) { World->GetTimerManager().ClearTimer(RefreshTimer); }
	Super::NativeDestruct();
}
void UHALMatchHUDWidget::RefreshCountdown()
{
	const FText Text=GetCountdownText();
	SetVisibility(Text.IsEmpty()?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible);
	if (CountdownText) { CountdownText->SetText(Text); }
}
