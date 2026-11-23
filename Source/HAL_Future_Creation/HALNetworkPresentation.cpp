#include "HALNetworkGameInstance.h"
#include "HALFrontEndConfig.h"
#include "HALArcadeStyle.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"

const UHALFrontEndConfig* UHALNetworkGameInstance::GetFrontEndConfig() const
{ return FrontEndConfig?FrontEndConfig.Get():GetDefault<UHALFrontEndConfig>(); }
void UHALNetworkGameInstance::FinishConnection()
{
	bConnecting=false;
	FTSTicker::GetCoreTicker().RemoveTicker(ConnectionTicker); ConnectionTicker.Reset();
}
void UHALNetworkGameInstance::CancelConnection()
{
	if (bConnecting) { ReturnToEntry(FText::FromString(TEXT("Connection cancelled. Create or join a room to try again."))); }
}
bool UHALNetworkGameInstance::IsMatchTravel(const FString& SourceMap,const FString& DestinationURL,bool bSeamless) const
{
	if (!bSeamless || !bPracticeRoomActive || PracticeMapPath.IsEmpty()
		|| UWorld::RemovePIEPrefix(SourceMap)!=PracticeMapPath) { return false; }
	const FURL Destination(nullptr,*DestinationURL,TRAVEL_Absolute);
	const FString Target=UWorld::RemovePIEPrefix(Destination.Map);
	return Destination.Valid && !DestinationURL.IsEmpty() && Target.StartsWith(TEXT("/"))
		&& Target!=PracticeMapPath && Target!=EntryMapPath;
}
void UHALNetworkGameInstance::HideTravelWidgets(const FString& DestinationURL,bool bSeamless)
{
	const bool bMatchTravel=IsMatchTravel(GetWorld()?GetWorld()->GetOutermost()->GetName():FString(),DestinationURL,bSeamless);
	EndTravelPresentation(); ReplaceVisibleWidget(nullptr); BeginTravelPresentation(bMatchTravel);
}
void UHALNetworkGameInstance::BeginTravelPresentation(bool bMatchTravel)
{
	if (TravelOverlay || !GetGameViewportClient()) { return; }
	UClass* LoadingClass=MatchTravelLoadingClass.Get();
	if (bMatchTravel && LoadingClass && !LoadingClass->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists))
	{
		// A fresh static placeholder belongs to the GameInstance, never to the departing Room/PlayerController.
		TravelLoadingWidget=CreateWidget<UUserWidget>(this,LoadingClass);
		if (TravelLoadingWidget)
		{
			TravelLoadingWidget->SetPlayerContext(FLocalPlayerContext());
			TravelLoadingWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			TravelOverlay=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.Padding(0).BorderBackgroundColor(HALArcade::Ink())[TravelLoadingWidget->TakeWidget()];
			GetGameViewportClient()->AddViewportWidgetContent(TravelOverlay.ToSharedRef(),1000);
			return;
		}
	}
	// Keep the legacy overlay for Create/Join/return and when no match placeholder is configured.
	TravelOverlay=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(HALArcade::Ink()).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight().Padding(20)[HALArcade::Text(GetFrontEndConfig()->Title,34,GetFrontEndConfig()->Accent)]
		+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(20)[HALArcade::Text(TEXT("TRAVELLING TO THE ARENA"),24)]
		+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[HALArcade::Text(TEXT("Your vehicle will be ready shortly."),17,HALArcade::Muted())]];
	GetGameViewportClient()->AddViewportWidgetContent(TravelOverlay.ToSharedRef(),1000);
}
void UHALNetworkGameInstance::EndTravelPresentation()
{
	if (TravelOverlay && GetGameViewportClient()) { GetGameViewportClient()->RemoveViewportWidgetContent(TravelOverlay.ToSharedRef()); }
	TravelOverlay.Reset();
	TravelLoadingWidget=nullptr;
}
