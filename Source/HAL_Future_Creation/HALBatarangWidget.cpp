#include "HALBatarangWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameStateBase.h"
#include "HALAbilitySystemComponent.h"
#include "HALBatarang.h"
#include "HALBatarangAbility.h"
#include "HALPlayerIdentitySettings.h"
#include "HALPlayerState.h"
#include "HALSmokeScreenView.h"
#include "HALVehiclePresentationComponent.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "TestVehiclePawn.h"
#include "Widgets/Layout/SBox.h"

TSharedRef<SWidget> UHALBatarangWidget::RebuildWidget() { return SNew(SBox); }
void UHALBatarangWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetIsFocusable(false);
	ForceVolatile(true);
}
int32 UHALBatarangWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Cull,
	FSlateWindowElementList& E,int32 Layer,const FWidgetStyle& Style,bool bEnabled) const
{
	const int32 Base = Super::NativePaint(Args,G,Cull,E,Layer,Style,bEnabled);
	const auto* Shot = Dart.Get();
	const auto* PC = GetOwningPlayer();
	if (!Shot || !PC || !PC->GetPawn() || PC->GetPawn() != Shot->GetOwner()) { return Base; }
	const auto& S = Shot->GetState();
	if (S.Phase != EHALDartPhase::Pinned || !S.Config) { return Base; }
	const FVector2D Size = G.GetLocalSize();
	if (Size.GetMin() <= 1.) { return Base; }
	auto Text = [&](FVector2D At,const FString& Value,FLinearColor Color,int32 Pt)
	{
		const auto Font = FCoreStyle::GetDefaultFontStyle("Bold",Pt);
		for (int32 Pass=0; Pass<2; ++Pass)
		{
			FSlateDrawElement::MakeText(E,Base+3+Pass,G.ToPaintGeometry(FVector2D(1),FSlateLayoutTransform(At+FVector2D(1-Pass))),
				Value,Font,ESlateDrawEffect::None,Pass ? Color : FLinearColor::Black);
		}
	};
	auto Line = [&](const TArray<FVector2D>& Points,FLinearColor Color,float Thickness)
	{
		FSlateDrawElement::MakeLines(E,Base+2,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,Thickness);
	};
	auto Plate = [&](FVector2D At,FVector2D Extent,const FLinearColor& Color)
	{
		FSlateDrawElement::MakeBox(E,Base+1,G.ToPaintGeometry(Extent,FSlateLayoutTransform(At)),
			FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Color);
	};
	const FLinearColor Ink(.012f,.021f,.035f,.93f);
	const auto Metrics = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	auto Readout = [&](const FString& Message, const FLinearColor& Accent)
	{
		const FVector2D Extent = Metrics->Measure(Message,FCoreStyle::GetDefaultFontStyle("Bold",14));
		const FVector2D At((Size.X-Extent.X)*.5,Size.Y*.82);
		Plate(At-FVector2D(10,3),Extent+FVector2D(20,6),Ink);
		Line({At-FVector2D(7,1),At+FVector2D(-7,Extent.Y+1)},Accent,2.f);
		Text(At,Message,FLinearColor::White,14);
	};
	const FString Prompt = Shot->IsManualInputOpen() ? TEXT("可引爆")
		: (UHALAbilitySystemComponent::IsActorStunned(PC->GetPawn()) ? TEXT("眩晕中") : TEXT("暂不可引爆"));
	if (!IsValid(S.Target) || UHALSmokeScreenView::ShouldHideWorldTarget(S.Target))
	{
		Readout(FString(TEXT("已挂镖 · "))+Prompt,FLinearColor(.69f,.77f,.70f));
		return Base+4;
	}
	const auto* PS = S.Target->GetPlayerState<AHALPlayerState>();
	int32 Number = PS ? PS->GetRoomData().Number : 0;
	// Non-room test maps use engine connection IDs (often 256+). Give their active drivers compact labels too.
	if (PS && Number <= 0)
	{
		Number = 1;
		if (const auto* GS = Shot->GetWorld()->GetGameState())
		{
			for (const APlayerState* Other : GS->PlayerArray)
			{ if (Other && !Other->IsOnlyASpectator() && Other->GetPlayerId() < PS->GetPlayerId()) { ++Number; } }
		}
	}
	const FString Label = Number > 0 ? FString::Printf(TEXT("P%d"),Number) : FString(TEXT("目标"));
	FLinearColor Tint(.82f,1.f,.23f);
	UHALPlayerIdentitySettings::ResolvePlayerColor(Shot->GetWorld(),S.Target,INDEX_NONE,Tint);
	const FVector World = S.Target->GetActorLocation();
	auto Project = [&](FVector Where,FVector2D& To) { return UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC,Where,To,true); };
	FVector2D Target = FVector2D::ZeroVector, Own = FVector2D::ZeroVector;
	const bool bProjected = Project(World,Target);
	if (!Project(PC->GetPawn()->GetActorLocation(),Own)) { Own = .5*Size; }
	FVector2D Bearing;
	if (bProjected) { Bearing = (Target-Own).GetSafeNormal(); }
	else
	{
		FVector Eye; FRotator Facing; PC->GetPlayerViewPoint(Eye,Facing);
		const FVector Local = Facing.UnrotateVector(World-Eye);
		Bearing = FVector2D(Local.Y,-Local.Z).GetSafeNormal();
	}
	if (Bearing.IsNearlyZero()) { Bearing = FVector2D(0,-1); }
	static const TCHAR* Compass[] = {TEXT("→"),TEXT("↘"),TEXT("↓"),TEXT("↙"),TEXT("←"),TEXT("↖"),TEXT("↑"),TEXT("↗")};
	static const TCHAR* Sides[] = {TEXT("前侧"),TEXT("后侧"),TEXT("左侧"),TEXT("右侧")};
	const int32 Dir = (FMath::RoundToInt(FMath::Atan2(Bearing.Y,Bearing.X)/(UE_PI*.25))+8)%8;
	const float Age = FMath::Max(0.f,float(Shot->ServerTime()-S.PhaseStart));
	const float Remaining = FMath::Max(0.f,S.Config->FuseSeconds-Age);
	Readout(FString::Printf(TEXT("%s %s · %s中镖 · %s  %.1f"),*Label,Compass[Dir],Sides[int32(S.Side)],*Prompt,Remaining),Tint);

	// Switch before the target's label or wings fall outside the frame, even if the car centre is still projected.
	const FVector2D Inset(FMath::Max(44.,Size.X*.04),FMath::Max(62.,Size.Y*.065));
	const bool bOnScreen = bProjected && Target.X>=Inset.X && Target.Y>=Inset.Y && Target.X<=Size.X-Inset.X && Target.Y<=Size.Y-Inset.Y;
	auto ExitDistance = [](FVector2D From,FVector2D Ray,const FBox2D& Box)
	{
		const double DX = FMath::Abs(Ray.X)<UE_SMALL_NUMBER ? UE_BIG_NUMBER : ((Ray.X>0 ? Box.Max.X : Box.Min.X)-From.X)/Ray.X;
		const double DY = FMath::Abs(Ray.Y)<UE_SMALL_NUMBER ? UE_BIG_NUMBER : ((Ray.Y>0 ? Box.Max.Y : Box.Min.Y)-From.Y)/Ray.Y;
		return FMath::Max(0.,FMath::Min(DX,DY));
	};
	FVector2D Marker = Target;
	if (!bOnScreen)
	{
		const FBox2D Frame(Size*.085,Size*.915);
		const FVector2D Origin = Frame.GetClosestPointTo(Own);
		Marker = Origin+Bearing*ExitDistance(Origin,Bearing,Frame);
	}
	const FVector2D Along = (Marker-Own).GetSafeNormal();
	if (!bOnScreen || Age < .35f)
	{
		// Begin outside the actual projected body even with a large truck, zoom or a shifted camera.
		const APawn* OwnerPawn = PC->GetPawn();
		FBox Body = OwnerPawn->GetRootComponent()->CalcBounds(FTransform::Identity).GetBox();
		FTransform Frame = OwnerPawn->GetActorTransform();
		if (const auto* Visual = OwnerPawn->FindComponentByClass<UHALVehiclePresentationComponent>())
		{
			FHALVehicleFXTargetSnapshot Bounds;
			Visual->CaptureStatusFXTargets(Bounds);
			if (Bounds.VehicleBounds.IsValid) { Body=Bounds.VehicleBounds; Frame=Bounds.VehicleToWorld; }
		}
		FBox2D Outline(ForceInit);
		for (int32 I=0; I<8; ++I)
		{
			FVector2D P;
			const FVector Corner(I&1?Body.Max.X:Body.Min.X,I&2?Body.Max.Y:Body.Min.Y,I&4?Body.Max.Z:Body.Min.Z);
			if (Project(Frame.TransformPosition(Corner),P)) { Outline+=P; }
		}
		const double Clearance = (Outline.bIsValid ? ExitDistance(Own,Along,Outline) : 30.)+5;
		const FVector2D Start = Own+Along*Clearance;
		for (int32 I=0; I<10 && Clearance+8 < (Marker-Own).Size(); ++I)
		{
			const double A = I*.1, B = (I+(Age<.35f?.58:1.))*.1;
			FLinearColor Trace = Tint; Trace.A = Age<.35f ? .6f : .15f+.18f*I/9.f;
			Line({FMath::Lerp(Start,Marker,A),FMath::Lerp(Start,Marker,B)},Trace,Age<.35f?2.f:1.3f);
		}
	}
	const FVector2D WingCenter = bOnScreen ? Marker-FVector2D(0,39) : Marker;
	const FVector2D LabelSize = Metrics->Measure(Label,FCoreStyle::GetDefaultFontStyle("Bold",14));
	const double HalfPlate = FMath::Max(16.,LabelSize.X*.5+5.);
	Plate(WingCenter-FVector2D(HalfPlate,11),FVector2D(2*HalfPlate,22),Ink);
	const double Spread = HalfPlate+1 + 17*(1.-FMath::Clamp(Age/.35f,0.f,1.f));
	for (const int32 Sign : {-1,1})
	{
		const FVector2D P = WingCenter+FVector2D(Sign*Spread,0);
		const TArray<FVector2D> Wing = {P+FVector2D(Sign*19,-8),P+FVector2D(Sign*11,3),
			P+FVector2D(Sign*9,0),P+FVector2D(Sign*5,5),P+FVector2D(0,3)};
		Line(Wing,Ink,6.f);
		Line(Wing,Tint,3.f);
	}
	Text(WingCenter-.5*LabelSize,Label,FLinearColor::White,14);
	// The cuts under the tab empty continuously from right to left during the fuse.
	const float Charge = Remaining/S.Config->FuseSeconds;
	for (int32 Tick=0; Tick<5; ++Tick)
	{
		const FVector2D Start = WingCenter+FVector2D(-HalfPlate+(2*HalfPlate/5)*Tick,11);
		const double Cell = 2*HalfPlate/5-1.5;
		const float Lit = FMath::Clamp(Charge*5-Tick,0.f,1.f);
		Line({Start,Start+FVector2D(Cell,0)},Ink,3.f);
		if (Lit > 0) { Line({Start,Start+FVector2D(Cell*Lit,0)},Tint,2.f); }
	}
	if (bOnScreen)
	{
		const auto Hull = S.Target->GetRootComponent()->CalcBounds(FTransform::Identity);
		const FVector Push = Shot->PushDirection();
		const bool bLongAxis = S.Side == EHALDartSide::Front || S.Side == EHALDartSide::Rear;
		const FVector Scale = S.Target->GetActorScale3D().GetAbs();
		const double Extent = bLongAxis ? Hull.BoxExtent.X*Scale.X : Hull.BoxExtent.Y*Scale.Y;
		const FVector Floor = World-FVector(0,0,Hull.BoxExtent.Z*Scale.Z-4);
		FVector2D Tail,Tip;
		if (Project(Floor+Push*Extent*.50,Tail) && Project(Floor+Push*(Extent+80),Tip))
		{
			const FVector2D Axis = (Tip-Tail).GetSafeNormal(), Perp(-Axis.Y,Axis.X);
			const double W = FMath::Clamp((Tip-Tail).Size()*.14,3.,9.);
			const FVector2D Neck = Tip-Axis*(3*W);
			const TArray<FVector2D> Points = {Tail+Perp*W,Neck+Perp*W,Neck+Perp*(2*W),Tip,Neck-Perp*(2*W),Neck-Perp*W,Tail-Perp*W,Tail+Perp*W};
			Line(Points,FLinearColor(.025f,.035f,.045f,.85f),4.5f);
			Line(Points,Tint,2.1f);
		}
	}
	return Base+4;
}
