#include "HALPracticeRoomWidget.h"
#include "HALArcadeStyle.h"
#include "HALPlayerController.h"
#include "HALPlayerState.h"
#include "HALMatchGameState.h"
#include "HALRoomComponent.h"
#include "HALVehicleCatalog.h"
#include "HALNetworkGameInstance.h"
#include "HALFrontEndConfig.h"
#include "HALAbilitySystemComponent.h"
#include "ArcadeVehicleMovementComponent.h"
#include "TestVehiclePawn.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerInput.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"

namespace
{
	const UHALRoomComponent* PresentationRoom(const UUserWidget* W)
	{ const auto* S=W->GetWorld()?W->GetWorld()->GetGameState<AHALMatchGameState>():nullptr; return S?S->GetRoom():nullptr; }
	bool AwaitingStart(const UUserWidget* W)
	{ const auto* R=PresentationRoom(W); return R && (R->GetData().Phase==EHALRoomPhase::Loading || R->GetData().Phase==EHALRoomPhase::Countdown); }
}
FText UHALPracticeRoomWidget::DescriptionText() const
{
	if (AwaitingStart(this)) { return FText::FromString(TEXT("The arena opens when every driver is ready. Get ready for the countdown.")); }
	const auto* PS=GetOwningPlayerState<AHALPlayerState>();
	const auto* E=PS && LoadedCatalog ? LoadedCatalog->Vehicles.FindByPredicate([PS](const FHALVehicleCatalogEntry& V){ return V.VehicleId==PS->GetRoomData().VehicleId; }):nullptr;
	return E && !E->Description.IsEmpty()?E->Description:FText::FromString(TEXT("Choose your ride. Confirm to practice and join the next match."));
}
void UHALPracticeRoomWidget::RefreshPresentation()
{
	const auto* R=PresentationRoom(this); if (!R) { return; }
	const auto Path=R->GetData().Catalog.ToSoftObjectPath();
	if (Path!=CatalogPath)
	{
		if (CatalogLoad) { CatalogLoad->CancelHandle(); }
		CatalogPath=Path; LoadedCatalog=nullptr;
		if (!Path.IsNull() && FPackageName::DoesPackageExist(Path.GetLongPackageName()))
		{ CatalogLoad=UAssetManager::GetStreamableManager().RequestAsyncLoad(Path,FStreamableDelegate::CreateWeakLambda(this,[this,Path]
			{ if (CatalogPath==Path) { LoadedCatalog=Cast<UHALVehicleCatalog>(Path.ResolveObject()); } })); }
	}
	const auto* PS=GetOwningPlayerState<AHALPlayerState>();
	const auto* Entry=PS && LoadedCatalog?LoadedCatalog->Vehicles.FindByPredicate([PS](const FHALVehicleCatalogEntry& E){ return E.VehicleId==PS->GetRoomData().VehicleId; }):nullptr;
	FSoftObjectPath Wanted;
	if (Entry)
	{
		Wanted=Entry->PreviewImage.ToSoftObjectPath();
		if (Wanted.IsNull() || !FPackageName::DoesPackageExist(Wanted.GetLongPackageName())) { Wanted=Entry->Icon.ToSoftObjectPath(); }
		if (!Wanted.IsNull() && !FPackageName::DoesPackageExist(Wanted.GetLongPackageName())) { Wanted.Reset(); }
	}
	if (Wanted==ImagePath) { return; }
	if (ImageLoad) { ImageLoad->CancelHandle(); }
	ImagePath=Wanted; LoadedPreview=nullptr; PreviewBrush.SetResourceObject(nullptr);
	if (!Wanted.IsNull())
	{ ImageLoad=UAssetManager::GetStreamableManager().RequestAsyncLoad(Wanted,FStreamableDelegate::CreateWeakLambda(this,[this,Wanted]
		{ if (ImagePath==Wanted) { LoadedPreview=Cast<UTexture2D>(Wanted.ResolveObject()); PreviewBrush.SetResourceObject(LoadedPreview); if (LoadedPreview) { PreviewBrush.ImageSize=FVector2D(LoadedPreview->GetSizeX(),LoadedPreview->GetSizeY()); } } })); }
}
bool UHALPracticeRoomWidget::HorizontalMenu(int32 Direction) { CycleVehicle(Direction); return true; }
void UHALPracticeRoomWidget::BackMenu()
{
	if (auto* PC=GetOwningPlayer<AHALPlayerController>()) { PC->ToggleRoomMenu(); }
}
void UHALPracticeRoomWidget::NativeConstruct()
{
	Super::NativeConstruct(); RefreshPresentation(); RefreshInput();
	GetWorld()->GetTimerManager().SetTimer(InputRefreshTimer,this,&ThisClass::RefreshInput,.1f,true);
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnApplicationActivationStateChanged().AddWeakLambda(this,[this](bool Active)
		{
			if (!Active)
			{
				if (auto* PC=GetOwningPlayer<AHALPlayerController>())
				{ if (!PC->WantsRoomMenu()) { PC->ToggleRoomMenu(); } if (auto* V=Cast<ATestVehiclePawn>(PC->GetPawn())) { V->SetMenuInputActive(true); } }
				bInputModeKnown=false;
			}
		});
	}
}
void UHALPracticeRoomWidget::NativeDestruct()
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(InputRefreshTimer); }
	if (FSlateApplication::IsInitialized()) { FSlateApplication::Get().OnApplicationActivationStateChanged().RemoveAll(this); }
	if (CatalogLoad) { CatalogLoad->CancelHandle(); } if (ImageLoad) { ImageLoad->CancelHandle(); }
	CatalogLoad.Reset(); ImageLoad.Reset(); LoadedCatalog=nullptr; LoadedPreview=nullptr;
	bHasPublishedView=false; PublishedView={};
	PreviewBrush.SetResourceObject(nullptr); ResetMenuControls(); Super::NativeDestruct();
}
void UHALPracticeRoomWidget::RefreshFromRoomState()
{
	if (IsInViewport() && GetCachedWidget().IsValid() && GetWorld() && !GetWorld()->bIsTearingDown) { RefreshInput(); }
}
void UHALPracticeRoomWidget::RefreshInput()
{
	if (bRefreshingView) { return; }
	TGuardValue<bool> RefreshGuard(bRefreshingView,true);
	RefreshPresentation();
	auto* PC=GetOwningPlayer<AHALPlayerController>(); if (!PC || !PC->GetLocalPlayer()) { return; }
	const auto* PS=PC->GetPlayerState<AHALPlayerState>(); auto* Vehicle=Cast<ATestVehiclePawn>(PC->GetPawn());
	const auto* ASC=PS?PS->GetHALAbilitySystemComponent():nullptr;
	const auto* M=Vehicle?Vehicle->FindComponentByClass<UArcadeVehicleMovementComponent>():nullptr;
	const bool Ready=PS && Vehicle && PS->GetRoomData().ReadyAvatar==Vehicle && ASC && ASC->GetAvatarActor()==Vehicle && Vehicle->GetPlayerState()==PS && M && M->GetMovementEffectStatus().bReady;
	const bool Menu=MenuVisible() || !Ready;
	if (bInputModeKnown && bUsingMenu==Menu && InputAvatar.Get()==Vehicle)
	{ PublishRoomView(); if (Menu && !IsMenuControlFocused()) { FocusMenu(); } return; }
	bInputModeKnown=true; bUsingMenu=Menu; InputAvatar=Vehicle; PC->bShowMouseCursor=Menu;
	if (Vehicle) { Vehicle->SetMenuInputActive(Menu); }
	PublishRoomView();
	// Enhanced Input's mapping rebuild handles held keys. Do not flush physical key state before that rebuild.
	if (Menu)
	{ FInputModeUIOnly Mode; Mode.SetWidgetToFocus(TakeWidget()); PC->SetInputMode(Mode); FocusMenu(); }
	else { PC->SetInputMode(FInputModeGameOnly()); }
}
TSharedRef<SWidget> UHALPracticeRoomWidget::RebuildWidget()
{
	SetIsFocusable(true); ResetMenuControls();
	if (UsesDesignerLayout()) { return Super::RebuildWidget(); }
	const auto* GI=GetGameInstance<UHALNetworkGameInstance>(); const auto* Config=GI?GI->GetFrontEndConfig():GetDefault<UHALFrontEndConfig>();
	auto Actions=SNew(SVerticalBox);
	auto AddAction=[this,Actions](const TCHAR* Label,FSimpleDelegate Action,TAttribute<bool> Enabled,TAttribute<EVisibility> ControlVisibility=TAttribute<EVisibility>(EVisibility::Visible))
	{
		auto B=HALArcade::Button(Label,Action); B->SetEnabled(Enabled); B->SetVisibility(ControlVisibility); RegisterMenuControl(B,Action);
		Actions->AddSlot().AutoHeight().Padding(0,4)[B];
	};
	auto Previous=FSimpleDelegate::CreateWeakLambda(this,[this]{ CycleVehicle(-1); });
	auto Next=FSimpleDelegate::CreateWeakLambda(this,[this]{ CycleVehicle(1); });
	auto PrevButton=HALArcade::Button(TEXT("< PREVIOUS"),Previous); PrevButton->SetEnabled(TAttribute<bool>::CreateLambda([this]{ return CanSelect(); })); RegisterMenuControl(PrevButton,Previous);
	auto NextButton=HALArcade::Button(TEXT("NEXT >"),Next); NextButton->SetEnabled(TAttribute<bool>::CreateLambda([this]{ return CanSelect(); })); RegisterMenuControl(NextButton,Next);
	Actions->AddSlot().AutoHeight().Padding(0,4)[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,4,0)[PrevButton]+SHorizontalBox::Slot().FillWidth(1)[NextButton]];
	AddAction(TEXT("CONFIRM & PRACTICE"),FSimpleDelegate::CreateUObject(this,&ThisClass::ConfirmSelection),TAttribute<bool>::CreateLambda([this]{ return CanConfirm(); }));
	AddAction(TEXT("CHANGE VEHICLE"),FSimpleDelegate::CreateUObject(this,&ThisClass::CancelSelection),TAttribute<bool>::CreateLambda([this]{ return CanCancel(); }));
	AddAction(TEXT("HOST: START MATCH"),FSimpleDelegate::CreateUObject(this,&ThisClass::StartRoomMatch),TAttribute<bool>::CreateLambda([this]{ return CanStart(); }));
	AddAction(TEXT("RESUME DRIVING"),FSimpleDelegate::CreateUObject(this,&ThisClass::ResumeDriving),TAttribute<bool>::CreateLambda([this]{ return CanResume(); }));
	AddAction(TEXT("RETURN TO ROOM"),FSimpleDelegate::CreateUObject(this,&ThisClass::ReturnToRoom),TAttribute<bool>::CreateLambda([this]{ return CanReturnToRoom(); }),TAttribute<EVisibility>::CreateLambda([this]
	{ const auto* R=PresentationRoom(this); return R && R->GetData().Phase==EHALRoomPhase::Playing && R->GetData().Rules.Profile==EHALRoomProfile::Development?EVisibility::Visible:EVisibility::Collapsed; }));
	AddAction(TEXT("LEAVE ROOM"),FSimpleDelegate::CreateUObject(this,&ThisClass::LeaveCurrentRoom),true);
	auto Seats=SNew(SHorizontalBox);
	for (int32 I=0;I<4;++I)
	{
		Seats->AddSlot().FillWidth(1).Padding(5,0)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(HALArcade::Panel()).Padding(12)
		[SNew(STextBlock).Text_Lambda([this,I]{ return SeatText(I); }).ColorAndOpacity_Lambda([this,I]{ return SeatColor(I); }).Font(FCoreStyle::GetDefaultFontStyle("Bold",14)).AutoWrapText(true)]];
	}
	return SNew(SOverlay)
		+SOverlay::Slot()[SNew(SBox).Visibility_Lambda([this]{ return MenuVisible()?EVisibility::Visible:EVisibility::Collapsed; })
		[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.012f,.022f,.045f,.94f))
		[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1280).HeightOverride(720)
		[SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight().Padding(38,20,38,14)[SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",28)).ColorAndOpacity(Config->Accent).Text_Lambda([this]
		{ const auto* R=PresentationRoom(this); return FText::FromString(!R?TEXT("CONNECTING"):AwaitingStart(this)?TEXT("GET READY"):R->GetData().Phase==EHALRoomPhase::Playing?TEXT("ARENA MENU"):TEXT("DRIVER SELECTION")); })]
		+SHorizontalBox::Slot().AutoWidth()[HALArcade::Text(TEXT("YOUR COLOUR. YOUR NUMBER. YOUR RIDE."),12,HALArcade::Muted())]]
		+SVerticalBox::Slot().AutoHeight().Padding(32,0)[SNew(SBox).HeightOverride(96)[Seats]]
		+SVerticalBox::Slot().FillHeight(1).Padding(38,18)[SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(.61f).Padding(0,0,24,0)
		[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(HALArcade::Panel()).Padding(24)
		[SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_UObject(this,&ThisClass::VehicleText).Font(FCoreStyle::GetDefaultFontStyle("Bold",27))]
		+SVerticalBox::Slot().FillHeight(1).Padding(0,14)[SNew(SOverlay)
		+SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(&PreviewBrush).Visibility_Lambda([this]{ return LoadedPreview && !AwaitingStart(this)?EVisibility::HitTestInvisible:EVisibility::Collapsed; })]]
		+SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SVerticalBox).Visibility_Lambda([this]{ return LoadedPreview && !AwaitingStart(this)?EVisibility::Collapsed:EVisibility::HitTestInvisible; })
		+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",32)).ColorAndOpacity(Config->Accent).Text_Lambda([this]{ return AwaitingStart(this)?StatusText():FText::FromString(TEXT("[  VEHICLE  ]")); })]
		+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,10)[HALArcade::Text(TEXT("READY TO MAKE AN IMPACT"),13,HALArcade::Muted())]]]
		+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_UObject(this,&ThisClass::DescriptionText).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle("Regular",17)).ColorAndOpacity(HALArcade::Muted())]]]
		+SHorizontalBox::Slot().FillWidth(.39f)[Actions]]
		+SVerticalBox::Slot().AutoHeight().Padding(38,0,38,10)[SNew(STextBlock).Text_UObject(this,&ThisClass::StatusText).AutoWrapText(true).ColorAndOpacity(Config->Accent).Font(FCoreStyle::GetDefaultFontStyle("Bold",17))]
		+SVerticalBox::Slot().AutoHeight().Padding(38,0,38,18)[HALArcade::Text(TEXT("D-PAD / ARROWS : NAVIGATE    LEFT / RIGHT : VEHICLE    A / ENTER : SELECT    B / ESC / START : RESUME"),12,HALArcade::Muted())]]]]]]
		+SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(12)
		[SNew(SBorder).Visibility_Lambda([this]{ return MenuVisible()?EVisibility::Collapsed:EVisibility::HitTestInvisible; }).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(HALArcade::Panel()).Padding(FMargin(20,8))
		[SNew(STextBlock).Text_Lambda([this]
		{ const auto* R=PresentationRoom(this); const auto* PS=GetOwningPlayerState<AHALPlayerState>(); return FText::FromString(FString::Printf(TEXT("P%d  /  %s       TAB / ESC / START : MENU"),PS?PS->GetRoomData().Number:0,R && R->GetData().Phase==EHALRoomPhase::Playing?TEXT("MATCH"):TEXT("PRACTICE"))); }).ColorAndOpacity_Lambda([this]{ const auto* PS=GetOwningPlayerState<AHALPlayerState>(); return PS?PS->GetRoomData().Color:FLinearColor::White; }).Font(FCoreStyle::GetDefaultFontStyle("Bold",15))]];
}
