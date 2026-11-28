#include "HALRecoveryPromptWidget.h"
#include "HALInputHintSubsystem.h"
#include "TestVehiclePawn.h"
#include "VehicleRecoveryComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/PrimitiveComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SSpacer.h"

namespace
{
	FKey MappedKey(const ULocalPlayer& Player, const UInputAction* Action)
	{
		const auto* Inputs = Player.GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		const auto* Hints = Player.GetSubsystem<UHALInputHintSubsystem>();
		if (!Inputs || !Action) { return EKeys::Invalid; }
		const bool bGamepad = Hints && Hints->GetCurrentInputDevice() == EHALInputHintDevice::Gamepad;
		for (const FKey& Key : Inputs->QueryKeysMappedToAction(Action))
		{
			if (Key.IsValid() && Key.IsGamepadKey() == bGamepad) { return Key; }
		}
		return EKeys::Invalid;
	}

	void DrawArc(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer, FVector2D Center,
		float Radius, float From, float To, const FLinearColor& Color, float Thickness)
	{
		const int32 Steps = FMath::Max(2, FMath::CeilToInt((To-From) * 10.f));
		TArray<FVector2f> Points;
		Points.Reserve(Steps+1);
		for (int32 I=0; I<=Steps; ++I)
		{
			const float Angle = FMath::Lerp(From,To,float(I)/Steps);
			Points.Emplace(Center.X + Radius*FMath::Cos(Angle), Center.Y + Radius*FMath::Sin(Angle));
		}
		FSlateDrawElement::MakeLines(Elements,Layer,Geometry.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,Thickness);
	}
}

TSharedRef<SWidget> UHALRecoveryPromptWidget::RebuildWidget() { return SNew(SSpacer); }

int32 UHALRecoveryPromptWidget::NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull,
	FSlateWindowElementList& E, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
	const int32 Base = Super::NativePaint(Args,G,Cull,E,Layer,Style,bEnabled);
	auto* PC = GetOwningPlayer();
	const auto* Car = GetOwningPlayerPawn<ATestVehiclePawn>();
	const auto* Recovery = Car ? Car->FindComponentByClass<UVehicleRecoveryComponent>() : nullptr;
	const auto* Body = Car ? Cast<UPrimitiveComponent>(Car->GetRootComponent()) : nullptr;
	if (!PC || !PC->GetLocalPlayer() || !Recovery || !Body || G.GetLocalSize().GetMin() < 100.) { return Base; }
	const double Now = GetWorld()->GetTimeSeconds();
	const double SinceComplete = Now - Recovery->GetExecutionCueTime();
	if (Recovery->IsExecuting() && SinceComplete >= .22) { return Base; }
	const FKey Key = MappedKey(*PC->GetLocalPlayer(),Car->GetSelfRightAction());
	if (!Key.IsValid()) { return Base; }

	// Project the chassis bounds, so the ring stays outside a long truck as the camera turns.
	FVector2D Screen;
	const auto& Bounds = Body->Bounds;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC,Bounds.Origin,Screen,true)) { return Base; }
	double Left = Screen.X, Right = Screen.X;
	for (int32 Corner=0; Corner<8; ++Corner)
	{
		const FVector Signs(Corner & 1 ? 1 : -1, Corner & 2 ? 1 : -1, Corner & 4 ? 1 : -1);
		FVector2D Point;
		if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC,Bounds.Origin+Signs*Bounds.BoxExtent,Point,true))
		{ Left = FMath::Min(Left,Point.X); Right = FMath::Max(Right,Point.X); }
	}
	const FVector2D Size = G.GetLocalSize();
	const FVector2D C(FMath::Clamp(Right+42. < Size.X-36. ? Right+42. : Left-42.,36.,Size.X-36.),
		FMath::Clamp(Screen.Y,36.,Size.Y-36.));
	const float Pulse = FMath::Clamp(1.f-float(Now-Recovery->GetLastLocalPressTime())/.15f,0.f,1.f);
	const float Flash = Recovery->IsExecuting() ? FMath::Clamp(1.f-float(SinceComplete)/.22f,0.f,1.f) : 0.f;
	const float Radius = 27.f + 1.5f*Pulse + 3.f*Flash;
	const FLinearColor Ink(.014f,.024f,.046f,.96f), Track(.17f,.24f,.33f), Cyan(.12f,.90f,.94f);
	const FLinearColor Bright = FMath::Lerp(Cyan,FLinearColor(1.f,.86f,.43f),Flash);
	static const FSlateRoundedBoxBrush Disc(FLinearColor::White,40.f);
	auto Box = [&](int32 Z, FVector2D Center, FVector2D Extent, const FSlateBrush& Brush, FLinearColor Color)
	{
		FSlateDrawElement::MakeBox(E,Z,G.ToPaintGeometry(Extent*2,FSlateLayoutTransform(Center-Extent)),&Brush,
			ESlateDrawEffect::None,Color);
	};
	Box(Base+1,C,FVector2D(Radius+4),Disc,Ink);
	const int32 Count = Recovery->GetRequiredPresses();
	const float Fill = Recovery->GetDisplayProgress()*Count;
	const float Step = 2*UE_PI/Count;
	for (int32 I=0; I<Count; ++I)
	{
		const float From = -UE_HALF_PI + Step*I + .055f, To = From + Step - .11f;
		DrawArc(G,E,Base+2,C,Radius,From,To,Track,3.5f);
		const float Part = FMath::Clamp(Fill-I,0.f,1.f);
		if (Part > 0) { DrawArc(G,E,Base+3,C,Radius,From,FMath::Lerp(From,To,Part),Bright,4.f+Pulse); }
	}
	// A small return arrow identifies the action even before the first segment is filled.
	DrawArc(G,E,Base+3,C+FVector2D(0,6),12.f,.45f,2.55f,FLinearColor::White,1.6f);
	const FVector2f Tip(C.X-10,C.Y+12);
	TArray<FVector2f> Arrow{Tip+FVector2f(0,5),Tip,Tip+FVector2f(5,1)};
	FSlateDrawElement::MakeLines(E,Base+3,G.ToPaintGeometry(),Arrow,ESlateDrawEffect::None,FLinearColor::White,true,1.6f);
	const FVector2D Glyph = C-FVector2D(0,3);
	static const FSlateRoundedBoxBrush Keycap(FLinearColor::White,3.f);
	if (Key == EKeys::Gamepad_Special_Left)
	{
		// Xbox View's two overlapping windows remain legible at gameplay scale.
		for (FVector2D Offset : {FVector2D(-3,-2.5),FVector2D(3,2.5)})
		{
			const bool bFront = Offset.X > 0;
			Box(Base+(bFront?6:4),Glyph+Offset,FVector2D(7,5.5),Keycap,FLinearColor::White);
			Box(Base+(bFront?7:5),Glyph+Offset,FVector2D(5,3.5),Keycap,Ink);
		}
	}
	else
	{
		const FText Label = Key.GetDisplayName();
		auto Font = FCoreStyle::GetDefaultFontStyle("Bold",13);
		const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		FVector2D TextSize = Measure->Measure(Label,Font);
		if (TextSize.X > 32) { Font.Size = FMath::Max(6,FMath::FloorToInt(13*32/TextSize.X)); TextSize = Measure->Measure(Label,Font); }
		const FVector2D Extent(FMath::Clamp(TextSize.X*.5+4.,10.,21.),10.);
		Box(Base+4,Glyph,Extent,Keycap,FLinearColor::White);
		FSlateDrawElement::MakeText(E,Base+5,G.ToPaintGeometry(FVector2D(1),FSlateLayoutTransform(Glyph-TextSize*.5)),
			Label,Font,ESlateDrawEffect::None,Ink);
	}
	return Base+7;
}
