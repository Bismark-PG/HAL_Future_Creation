#include "HALNetworkGameInstance.h"
#include "HALFrontEndConfig.h"
#include "HALArcadeStyle.h"
#include "Engine/GameViewportClient.h"
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
void UHALNetworkGameInstance::HideTravelWidgets()
{ ReplaceVisibleWidget(nullptr); BeginTravelPresentation(); }
void UHALNetworkGameInstance::BeginTravelPresentation()
{
	if (TravelOverlay || !GetGameViewportClient()) { return; }
	// Viewport Slate survives a blocking World load. It captures only copied text/colors, no old actors.
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
}
