#include "HALCameraDebugWidget.h"
#include "HALDynamicCameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SSpacer.h"

namespace
{
	TAutoConsoleVariable<int32> CVarCameraDebug(TEXT("hal.CameraDebug"),0,TEXT("Local camera screen frames, target weights and state overlay: 0=off, 1=on."));
}
TSharedRef<SWidget> UHALCameraDebugWidget::RebuildWidget() { return SNew(SSpacer); }
int32 UHALCameraDebugWidget::NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Culling,
	FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const int32 Base = Super::NativePaint(Args,G,Culling,Elements,Layer,Style,bParentEnabled);
	if (!CVarCameraDebug.GetValueOnGameThread()) { return Base; }
	const auto* PC = GetOwningPlayer(); const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const auto* Camera = Pawn ? Pawn->FindComponentByClass<UHALDynamicCameraComponent>() : nullptr;
	if (!Camera) { return Base; }
	const auto& F = Camera->GetDebugFrame(); const FVector2D Size = G.GetLocalSize();
	const auto Font = FCoreStyle::GetDefaultFontStyle("Regular",16);
	auto Text = [&](const FVector2D& At, const FString& Value, FLinearColor Color)
	{ FSlateDrawElement::MakeText(Elements,Base+3,G.ToPaintGeometry(FVector2D(1,1),FSlateLayoutTransform(At)),Value,Font,ESlateDrawEffect::None,Color); };
	if (!F.bValid) { Text(FVector2D(16,16),TEXT("Camera: legacy fallback / profile or local viewport unavailable"),FLinearColor::Yellow); return Base+3; }
	FVector2D Origin = FVector2D::ZeroVector, ViewSize = Size;
	const double ActualAspect = Size.X/FMath::Max(Size.Y,1.0);
	if (F.View.bConstrainAspectRatio)
	{
		if (ActualAspect > F.View.AspectRatio) { ViewSize.X = Size.Y*F.View.AspectRatio; Origin.X = (Size.X-ViewSize.X)*.5; }
		else { ViewSize.Y = Size.X/F.View.AspectRatio; Origin.Y = (Size.Y-ViewSize.Y)*.5; }
	}
	auto At = [&](FVector2D N) { return Origin+N*ViewSize; };
	auto Box = [&](FVector2D FrameSize, FLinearColor Color)
	{
		const FVector2D Min = At((FVector2D(1,1)-FrameSize)*.5), Max = At((FVector2D(1,1)+FrameSize)*.5);
		TArray<FVector2D> Points { Min,FVector2D(Max.X,Min.Y),Max,FVector2D(Min.X,Max.Y),Min };
		FSlateDrawElement::MakeLines(Elements,Base+1,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,1.5f);
	};
	Box(F.SoftSize,FLinearColor(.2f,.7f,1)); Box(F.HardSize,FLinearColor(1,.45f,.1f));
	for (const auto& Target : F.Targets)
	{
		FVector2D Screen;
		if (HALCamera::Project(F.View,Target.Position,Screen) && Screen.X >= 0 && Screen.X <= 1 && Screen.Y >= 0 && Screen.Y <= 1)
		{ Text(At(Screen),FString::Printf(TEXT("+ %s  w=%.2f"),*Target.Label,Target.Weight),FLinearColor::Green); }
	}
	const TArray<TPair<FVector,FString>> FocusPoints { {F.AutoGoal,TEXT("Goal")},{F.AutoFocus,TEXT("Auto")},{F.Focus,TEXT("Final")} };
	for (const auto& Point : FocusPoints)
	{
		FVector2D Screen; if (HALCamera::Project(F.View,Point.Key,Screen)) { Text(At(Screen),Point.Value,FLinearColor::Yellow); }
	}
	Text(FVector2D(FMath::Max(16.0,Size.X-480),16),FString::Printf(
		TEXT("CAM %s  FOV %.1f  D %.0f -> %.0f\nOwn %.3f / %.3f  Primary candidates %d\nLook %.3f / %.3f  Device %u  Idle %.2f\nHard=%d LookLimit=%d Return=%d\nBlue: soft / Orange: hard"),
		F.bDynamic ? TEXT("Dynamic") : TEXT("Static"),F.View.FOV,F.Distance,F.DesiredDistance,F.OwnScreen.X,F.OwnScreen.Y,
		F.MainBallCandidates,F.ManualOffset.X,F.ManualOffset.Y,F.InputDevice,F.IdleSeconds,F.bHardLimited,F.bManualLimited,F.bReturning),FLinearColor::White);
	return Base+3;
}
