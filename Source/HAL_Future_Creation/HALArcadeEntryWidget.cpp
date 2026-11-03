#include "HALArcadeEntryWidget.h"
#include "HALArcadeStyle.h"
#include "HALFrontEndConfig.h"
#include "HALNetworkGameInstance.h"
#include "HALMenuFeedback.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"

FHALEntryView UHALArcadeEntryWidget::GetEntryView() const
{
	FHALEntryView View; View.bAttract=bAttract;
	if (const auto* GI=GetGameInstance<UHALNetworkGameInstance>())
	{ View.bConnecting=GI->IsConnecting(); View.Message=HALMenuFeedback::Connection(GI->GetConnectionMessage()); View.LastHostAddress=GI->GetLastHostAddress(); }
	return View;
}
void UHALArcadeEntryWidget::PublishEntryView()
{
	const auto View=GetEntryView();
	if (!bHasPublishedView || !FHALEntryView::StaticStruct()->CompareScriptStruct(&View,&PublishedView,0))
	{ PublishedView=View; bHasPublishedView=true; ReceiveEntryView(View); }
}
void UHALArcadeEntryWidget::OpenConnections() { bAttract=false; PublishEntryView(); FocusMenu(); }
bool UHALArcadeEntryWidget::HostRoom()
{ auto* GI=GetGameInstance<UHALNetworkGameInstance>(); const bool Result=GI && GI->HostMatch(); PublishEntryView(); return Result; }
bool UHALArcadeEntryWidget::JoinRoom(const FString& HostIPv4)
{ auto* GI=GetGameInstance<UHALNetworkGameInstance>(); const bool Result=GI && GI->JoinMatch(HostIPv4); PublishEntryView(); return Result; }
void UHALArcadeEntryWidget::CancelConnection()
{ if (auto* GI=GetGameInstance<UHALNetworkGameInstance>()) { GI->CancelConnection(); } }
void UHALArcadeEntryWidget::Join()
{ if (Address) { JoinRoom(Address->GetText().ToString()); } }
void UHALArcadeEntryWidget::BackMenu()
{
	if (auto* GI=GetGameInstance<UHALNetworkGameInstance>(); GI && GI->IsConnecting()) { GI->CancelConnection(); return; }
	bAttract=!bAttract; PublishEntryView(); FocusMenu();
}
void UHALArcadeEntryWidget::RefreshEntry()
{
	const auto* GI=GetGameInstance<UHALNetworkGameInstance>(); if (!GI) { return; }
	const bool Busy=GI->IsConnecting();
	PublishEntryView();
	if (bLastBusy!=Busy || !IsMenuControlFocused()) { bLastBusy=Busy; FocusMenu(); }
}
void UHALArcadeEntryWidget::NativeConstruct()
{
	if (const auto* GI=GetGameInstance<UHALNetworkGameInstance>()) { bAttract=GI->GetConnectionMessage().IsEmpty(); }
	Super::NativeConstruct(); PublishEntryView();
	GetWorld()->GetTimerManager().SetTimer(RefreshTimer,this,&ThisClass::RefreshEntry,.1f,true);
}
void UHALArcadeEntryWidget::NativeDestruct()
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(RefreshTimer); }
	bHasPublishedView=false; PublishedView={}; ResetMenuControls(); Address.Reset(); Super::NativeDestruct();
}
TSharedRef<SWidget> UHALArcadeEntryWidget::RebuildWidget()
{
	SetIsFocusable(true); ResetMenuControls();
	if (UsesDesignerLayout()) { return Super::RebuildWidget(); }
	const auto* GI=GetGameInstance<UHALNetworkGameInstance>();
	const auto* Config=GI?GI->GetFrontEndConfig():GetDefault<UHALFrontEndConfig>();
	BackgroundBrush.SetResourceObject(Config->Background); LogoBrush.SetResourceObject(Config->Logo);
	const auto Busy=[this]{ const auto* N=GetGameInstance<UHALNetworkGameInstance>(); return N && N->IsConnecting(); };
	const auto ConnectionVisibility=[this]{ return !bAttract?EVisibility::Visible:EVisibility::Collapsed; };
	const auto IdleVisibility=[this,Busy]{ return !bAttract && !Busy()?EVisibility::Visible:EVisibility::Collapsed; };
	auto Continue=HALArcade::Button(TEXT("PRESS START / ENTER"),FSimpleDelegate::CreateUObject(this,&ThisClass::OpenConnections));
	Continue->SetVisibility(TAttribute<EVisibility>::CreateLambda([this]{ return bAttract?EVisibility::Visible:EVisibility::Collapsed; }));
	RegisterMenuControl(Continue,FSimpleDelegate::CreateUObject(this,&ThisClass::OpenConnections));
	auto HostAction=FSimpleDelegate::CreateWeakLambda(this,[this]{ HostRoom(); });
	auto Host=HALArcade::Button(TEXT("CREATE ROOM"),HostAction); Host->SetVisibility(TAttribute<EVisibility>::CreateLambda(IdleVisibility)); RegisterMenuControl(Host,HostAction);
	SAssignNew(Address,SEditableTextBox).HintText(FText::FromString(TEXT("HOST IPv4 ADDRESS"))).Text(FText::FromString(GI?GI->GetLastHostAddress():TEXT("127.0.0.1")))
		.Font(FCoreStyle::GetDefaultFontStyle("Regular",21)).Padding(FMargin(12)).SelectAllTextWhenFocused(true)
		.Visibility_Lambda(IdleVisibility);
	RegisterMenuControl(Address.ToSharedRef(),FSimpleDelegate::CreateUObject(this,&ThisClass::Join));
	auto JoinAction=FSimpleDelegate::CreateUObject(this,&ThisClass::Join);
	auto JoinButton=HALArcade::Button(TEXT("JOIN ROOM"),JoinAction); JoinButton->SetVisibility(TAttribute<EVisibility>::CreateLambda(IdleVisibility)); RegisterMenuControl(JoinButton,JoinAction);
	auto BackAction=FSimpleDelegate::CreateUObject(this,&ThisClass::BackMenu);
	auto Back=HALArcade::Button(TEXT("BACK"),BackAction); Back->SetVisibility(TAttribute<EVisibility>::CreateLambda(IdleVisibility)); RegisterMenuControl(Back,BackAction);
	auto CancelAction=FSimpleDelegate::CreateUObject(this,&ThisClass::CancelConnection);
	auto Cancel=HALArcade::Button(TEXT("CANCEL CONNECTION"),CancelAction);
	Cancel->SetVisibility(TAttribute<EVisibility>::CreateLambda([this,Busy]{ return !bAttract && Busy()?EVisibility::Visible:EVisibility::Collapsed; })); RegisterMenuControl(Cancel,CancelAction);
	return SNew(SOverlay)
		+SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(HALArcade::Ink())]
		+SOverlay::Slot()[SNew(SImage).Image(&BackgroundBrush).ColorAndOpacity(FLinearColor(1,1,1,.18f)).Visibility(Config->Background?EVisibility::HitTestInvisible:EVisibility::Collapsed)]
		+SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1280).HeightOverride(720)
		[SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(.56f).Padding(64,74)
		[SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight()[HALArcade::Text(TEXT("INTERPLANETARY SPORTS BROADCAST"),14,Config->Accent)]
		+SVerticalBox::Slot().AutoHeight().Padding(0,36)[SNew(SBox).HeightOverride(90).Visibility(Config->Logo?EVisibility::HitTestInvisible:EVisibility::Collapsed)[SNew(SImage).Image(&LogoBrush)]]
		+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Config->Title).Font(FCoreStyle::GetDefaultFontStyle("Bold",44)).AutoWrapText(true)]
		+SVerticalBox::Slot().AutoHeight().Padding(0,24)[SNew(STextBlock).Text(Config->Subtitle).Font(FCoreStyle::GetDefaultFontStyle("Regular",17)).ColorAndOpacity(HALArcade::Muted()).AutoWrapText(true)]
		+SVerticalBox::Slot().FillHeight(1)
		+SVerticalBox::Slot().AutoHeight()[HALArcade::Text(TEXT("4 DRIVERS. ONE ARENA."),21,Config->Accent)]
		+SVerticalBox::Slot().AutoHeight().Padding(0,12)[HALArcade::Text(TEXT("Take the ball. Make an impact."),17,HALArcade::Muted())]]
		+SHorizontalBox::Slot().FillWidth(.44f).Padding(20,100,64,72)
		[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(HALArcade::Panel()).Padding(30)
		[SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,24)[HALArcade::Text(TEXT("ENTER THE ARENA"),25)]
		+SVerticalBox::Slot().AutoHeight().Padding(0,10)[Continue]
		+SVerticalBox::Slot().AutoHeight().Padding(0,8)[Host]
		+SVerticalBox::Slot().AutoHeight().Padding(0,8)[Address.ToSharedRef()]
		+SVerticalBox::Slot().AutoHeight().Padding(0,8)[JoinButton]
		+SVerticalBox::Slot().AutoHeight().Padding(0,8)[Back]
		+SVerticalBox::Slot().AutoHeight().Padding(0,8)[Cancel]
		+SVerticalBox::Slot().AutoHeight().Padding(0,20)[SNew(STextBlock).Visibility_Lambda(ConnectionVisibility).Text_Lambda([this]{ const auto* N=GetGameInstance<UHALNetworkGameInstance>(); return HALMenuFeedback::Connection(N?N->GetConnectionMessage():FText()); }).AutoWrapText(true).ColorAndOpacity(HALArcade::Muted()).Font(FCoreStyle::GetDefaultFontStyle("Regular",16))]
		+SVerticalBox::Slot().FillHeight(1)
		+SVerticalBox::Slot().AutoHeight()[HALArcade::Text(TEXT("D-PAD / ARROWS : MOVE\nA / ENTER : SELECT   B / ESC : BACK"),12,HALArcade::Muted())]]]]]];
}
