#include "HALSkillTestDummy.h"
#include "HALAbilitySystemComponent.h"
#include "VehicleHealthComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Widgets/Layout/SBox.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"

FHALSkillTestDummyView UHALSkillTestDummyWidget::GetDummyView() const
{
	FHALSkillTestDummyView V; const auto* D=Dummy.Get(); if (!D) { return V; }
	const auto* C=D->GetTestConfig(); const auto* H=D->FindComponentByClass<UVehicleHealthComponent>();
	V.Label=C?C->Label:FText(); V.bReady=H&&H->IsHealthReady(); V.HP=H?H->GetCurrentHP():0; V.MaxHP=H?H->GetMaxHP():1;
	V.bStunned=UHALAbilitySystemComponent::IsActorStunned(D); return V;
}
TSharedRef<SWidget> UHALSkillTestDummyWidget::RebuildWidget()
{ bNative=!(WidgetTree&&WidgetTree->RootWidget); return bNative?SNew(SBox):Super::RebuildWidget(); }
void UHALSkillTestDummyWidget::NativeConstruct()
{ Super::NativeConstruct(); SetVisibility(ESlateVisibility::HitTestInvisible); GetWorld()->GetTimerManager().SetTimer(RefreshTimer,this,&ThisClass::Refresh,.1f,true); }
void UHALSkillTestDummyWidget::NativeDestruct()
{ if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(RefreshTimer); } Super::NativeDestruct(); }
void UHALSkillTestDummyWidget::Refresh() { ReceiveDummyView(GetDummyView()); }
int32 UHALSkillTestDummyWidget::NativePaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& C,FSlateWindowElementList& E,int32 L,const FWidgetStyle& S,bool B) const
{
	const int32 Base=Super::NativePaint(A,G,C,E,L,S,B); if (!bNative) { return Base; } const auto V=GetDummyView();
	const FVector2D Size=G.GetLocalSize(); const auto* Brush=FCoreStyle::Get().GetBrush("WhiteBrush");
	const auto Rect=[&](FVector2D At,FVector2D Ext,FLinearColor Color,int32 Layer)
	{ FSlateDrawElement::MakeBox(E,Layer,G.ToPaintGeometry(Ext,FSlateLayoutTransform(At)),Brush,ESlateDrawEffect::None,Color); };
	Rect(FVector2D(Size.X*.1,30),FVector2D(Size.X*.8,10),FLinearColor::Black,Base+1);
	Rect(FVector2D(Size.X*.1+1,31),FVector2D((Size.X*.8-2)*FMath::Clamp(V.HP/FMath::Max(1.f,V.MaxHP),0.f,1.f),8),V.bStunned?FLinearColor(1,.65,.05):FLinearColor(.1,.9,.3),Base+2);
	FSlateDrawElement::MakeText(E,Base+2,G.ToPaintGeometry(FVector2D(1),FSlateLayoutTransform(FVector2D(15,5))),V.Label,FCoreStyle::GetDefaultFontStyle("Bold",13),ESlateDrawEffect::None,FLinearColor::White);
	const FString HP=V.bReady?FString::Printf(TEXT("%.0f / %.0f"),V.HP,V.MaxHP):TEXT("GAS not ready");
	FSlateDrawElement::MakeText(E,Base+2,G.ToPaintGeometry(FVector2D(1),FSlateLayoutTransform(FVector2D(15,43))),HP,FCoreStyle::GetDefaultFontStyle("Regular",10),ESlateDrawEffect::None,FLinearColor::White);
	return Base+2;
}
