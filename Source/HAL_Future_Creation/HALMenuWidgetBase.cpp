#include "HALMenuWidgetBase.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Layout/WidgetPath.h"
DEFINE_LOG_CATEGORY_STATIC(LogHALMenuFocus,Log,All);

bool UHALMenuWidgetBase::UsesDesignerLayout() const
{ return GetClass()->IsChildOf(UUserWidget::StaticClass()) && Cast<UWidgetBlueprintGeneratedClass>(GetClass())!=nullptr; }
void UHALMenuWidgetBase::NativeDestruct()
{ DesignerFocusTarget=nullptr; ResetMenuControls(); Super::NativeDestruct(); }
void UHALMenuWidgetBase::SetMenuFocusTarget(UWidget* Target)
{
	if (!Target) { DesignerFocusTarget=nullptr; return; }
	// Support nested card widgets, including children created at runtime and added to this screen.
	for (UWidget* Item=Target;Item;)
	{
		if (Item==this) { DesignerFocusTarget=Target; return; }
		Item=Item->GetParent()?static_cast<UWidget*>(Item->GetParent()):Item->GetTypedOuter<UUserWidget>();
	}
}
bool UHALMenuWidgetBase::HasVisibleDesignerFocus() const
{
	if (!FSlateApplication::IsInitialized() || !GetOwningLocalPlayer()) { return false; }
	const auto User=GetOwningLocalPlayer()->GetSlateUser(); if (!User) { return false; }
	const auto Focused=FSlateApplication::Get().GetUserFocusedWidget(User->GetUserIndex());
	const auto Root=GetCachedWidget(); FWidgetPath Path;
	return Root && Focused && Focused!=Root && Focused->IsEnabled()
		&& FSlateApplication::Get().GeneratePathToWidgetUnchecked(Focused.ToSharedRef(),Path)
		&& Path.ContainsWidget(Root.Get());
}

void UHALMenuWidgetBase::ResetMenuControls() { Controls.Reset(); FocusIndex=INDEX_NONE; }
void UHALMenuWidgetBase::RegisterMenuControl(const TSharedRef<SWidget>& Widget,FSimpleDelegate Action)
{ Controls.Add({Widget,Action}); }
bool UHALMenuWidgetBase::IsMenuControlFocused() const
{
	if (UsesDesignerLayout()) { return HasVisibleDesignerFocus(); }
	for (const auto& C:Controls) { if (auto W=C.Widget.Pin(); W && W->IsEnabled() && W->GetVisibility().IsVisible() && (W->HasAnyUserFocus() || W->HasFocusedDescendants())) { return true; } }
	return false;
}
void UHALMenuWidgetBase::FocusMenu(int32 Direction)
{
	if (UsesDesignerLayout())
	{
		if (!FSlateApplication::IsInitialized() || !GetOwningLocalPlayer() || !WidgetTree) { return; }
		const auto User=GetOwningLocalPlayer()->GetSlateUser(); if (!User) { return; }
		auto TryFocus=[&](UWidget* W)
		{
			const auto Slate=W?W->GetCachedWidget():nullptr; FWidgetPath Path;
			if (!Slate || !Slate->SupportsKeyboardFocus() || !Slate->IsEnabled()
				|| !FSlateApplication::Get().GeneratePathToWidgetUnchecked(Slate.ToSharedRef(),Path)) { return false; }
			if (!Slate->HasUserFocus(User->GetUserIndex()).IsSet() && !Slate->HasUserFocusedDescendants(User->GetUserIndex()))
			{ FSlateApplication::Get().SetUserFocus(User->GetUserIndex(),Slate,EFocusCause::Navigation); }
			return Slate->HasUserFocus(User->GetUserIndex()).IsSet() || Slate->HasUserFocusedDescendants(User->GetUserIndex());
		};
		if (TryFocus(DesignerFocusTarget)) { return; }
		bool Found=false; WidgetTree->ForEachWidget([&](UWidget* W){ if (!Found) { Found=TryFocus(W); } });
		return;
	}
	UE_LOG(LogHALMenuFocus,Verbose,TEXT("Focus %s direction=%d previous=%d"),*GetName(),Direction,FocusIndex);
	if (Controls.IsEmpty() || !FSlateApplication::IsInitialized()) { return; }
	for (int32 I=0;I<Controls.Num();++I)
	{ if (auto W=Controls[I].Widget.Pin(); W && (W->HasAnyUserFocus() || W->HasFocusedDescendants())) { FocusIndex=I; break; } }
	const int32 Step=Direction<0?-1:1;
	int32 Index=FocusIndex==INDEX_NONE ? (Step>0?0:Controls.Num()-1) : (FocusIndex+(Direction?Step:0)+Controls.Num())%Controls.Num();
	for (int32 Attempt=0;Attempt<Controls.Num();++Attempt,Index=(Index+Step+Controls.Num())%Controls.Num())
	{
		auto W=Controls[Index].Widget.Pin();
		if (!W || !W->IsEnabled() || !W->GetVisibility().IsVisible()) { continue; }
		const auto* Player=GetOwningLocalPlayer(); auto User=Player?Player->GetSlateUser():nullptr;
		if (User && (FSlateApplication::Get().GetUserFocusedWidget(User->GetUserIndex())==W || FSlateApplication::Get().SetUserFocus(User->GetUserIndex(),W,EFocusCause::Navigation))) { FocusIndex=Index; UE_LOG(LogHALMenuFocus,Verbose,TEXT("Focused %s index=%d type=%s"),*GetName(),Index,*W->GetTypeAsString()); return; }
	}
}
bool UHALMenuWidgetBase::ActivateMenu()
{
	for (int32 I=0;I<Controls.Num();++I)
	{
		auto W=Controls[I].Widget.Pin();
		if (W && W->IsEnabled() && W->GetVisibility().IsVisible() && (W->HasAnyUserFocus() || W->HasFocusedDescendants()))
		{ UE_LOG(LogHALMenuFocus,Verbose,TEXT("Activate %s index=%d"),*GetName(),I); auto Action=Controls[I].Action; Action.ExecuteIfBound(); return true; }
	}
	return false;
}
FReply UHALMenuWidgetBase::NativeOnPreviewKeyDown(const FGeometry& G,const FKeyEvent& E)
{
	const auto K=E.GetKey();
	UE_LOG(LogHALMenuFocus,Verbose,TEXT("Key %s %s"),*GetName(),*K.ToString());
	if (UsesDesignerLayout())
	{
		// Give a Designer modal/subpanel its normal preview-key chance before the shared Back fallback.
		const auto Reply=Super::NativeOnPreviewKeyDown(G,E);
		if (Reply.IsEventHandled() || (K!=EKeys::Escape && K!=EKeys::Gamepad_FaceButton_Right && K!=EKeys::Gamepad_Special_Right)) { return Reply; }
	}
	if (K==EKeys::Escape || K==EKeys::Gamepad_FaceButton_Right || K==EKeys::Gamepad_Special_Right) { if (!E.IsRepeat()) { BackMenu(); } return FReply::Handled(); }
	if (K==EKeys::Tab || K==EKeys::Down || K==EKeys::Up || K==EKeys::Gamepad_DPad_Down || K==EKeys::Gamepad_DPad_Up)
	{ FocusMenu(K==EKeys::Up || K==EKeys::Gamepad_DPad_Up || (K==EKeys::Tab && E.IsShiftDown())?-1:1); return FReply::Handled(); }
	if (K==EKeys::Enter || K==EKeys::Gamepad_FaceButton_Bottom)
	{ if (!E.IsRepeat()) { ActivateMenu(); } return FReply::Handled(); }
	if ((K==EKeys::Left || K==EKeys::Gamepad_DPad_Left || K==EKeys::Gamepad_LeftShoulder) && HorizontalMenu(-1)) { return FReply::Handled(); }
	if ((K==EKeys::Right || K==EKeys::Gamepad_DPad_Right || K==EKeys::Gamepad_RightShoulder) && HorizontalMenu(1)) { return FReply::Handled(); }
	return Super::NativeOnPreviewKeyDown(G,E);
}
FNavigationReply UHALMenuWidgetBase::NativeOnNavigation(const FGeometry& G,const FNavigationEvent& E,const FNavigationReply& Default)
{
	if (UsesDesignerLayout()) { return Super::NativeOnNavigation(G,E,Default); }
	const auto N=E.GetNavigationType();
	if (N==EUINavigation::Up || N==EUINavigation::Previous) { FocusMenu(-1); return FNavigationReply::Stop(); }
	if (N==EUINavigation::Down || N==EUINavigation::Next) { FocusMenu(1); return FNavigationReply::Stop(); }
	if ((N==EUINavigation::Left || N==EUINavigation::Right) && HorizontalMenu(N==EUINavigation::Left?-1:1)) { return FNavigationReply::Stop(); }
	return Super::NativeOnNavigation(G,E,Default);
}
