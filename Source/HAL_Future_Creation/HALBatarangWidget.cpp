#include "HALBatarangWidget.h"
#include "HALBatarang.h"
#include "HALBatarangAbility.h"
#include "HALSmokeScreenView.h"
#include "HALVehiclePresentationComponent.h"
#include "TestVehiclePawn.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"

namespace
{
	// Screen-space silhouette of the visible body only. Nameplates and skill FX
	// must never enlarge the brackets or move them away from the doors.
	struct FProjectedDartBody
	{
		FVector2D Center = FVector2D::ZeroVector;
		FVector2D Right = FVector2D(1, 0);
		TArray<FVector2D, TInlineAllocator<8>> Corners;

		double Reach(FVector2D Axis) const
		{
			double Distance = 0;
			for (const FVector2D& Corner : Corners)
			{
				Distance = FMath::Max(Distance, FVector2D::DotProduct(Corner - Center, Axis));
			}
			return Distance;
		}
	};

	bool ProjectBody(const APlayerController& PC, const APawn& Pawn, FProjectedDartBody& Out)
	{
		const USceneComponent* Root = Pawn.GetRootComponent();
		if (!Root) { return false; }
		FBox Box = Root->CalcBounds(FTransform::Identity).GetBox();
		FTransform Frame = Root->GetComponentTransform();
		if (const auto* Visual = Pawn.FindComponentByClass<UHALVehiclePresentationComponent>())
		{
			FHALVehicleFXTargetSnapshot Snapshot;
			Visual->CaptureStatusFXTargets(Snapshot);
			if (Snapshot.VehicleBounds.IsValid)
			{
				Box = Snapshot.VehicleBounds;
				Frame = Snapshot.VehicleToWorld;
			}
		}
		auto ProjectLocal = [&](FVector P, FVector2D& Screen)
		{
			return UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(&PC, Frame.TransformPosition(P), Screen, true);
		};
		const FVector Middle = Box.GetCenter();
		if (!ProjectLocal(Middle, Out.Center)) { return false; }
		FVector2D Side;
		if (ProjectLocal(Middle + FVector(0, Box.GetExtent().Y, 0), Side) && !(Side-Out.Center).IsNearlyZero())
		{
			Out.Right = (Side - Out.Center).GetSafeNormal();
		}
		for (int32 I = 0; I < 8; ++I)
		{
			const FVector Corner(I&1 ? Box.Max.X : Box.Min.X, I&2 ? Box.Max.Y : Box.Min.Y, I&4 ? Box.Max.Z : Box.Min.Z);
			FVector2D Point;
			if (ProjectLocal(Corner, Point)) { Out.Corners.Add(Point); }
		}
		return true;
	}

	// Sweep two strips between the scalloped inner edge and the curved outer edge.
	// Leaving a gap in both strips at the eyelets shows the actual floor through them.
	struct FDartWingStencil
	{
		TArray<FVector2f> Points;
		TArray<SlateIndex> Indices;
		FDartWingStencil()
		{
			constexpr int32 Rows = 48;
			for (int32 Row = 0; Row <= Rows; ++Row)
			{
				const float Y = 2.f * Row / Rows - 1.f;
				const float Outer = 1.f - Y*Y;
				const float Scallop = FMath::Sqrt(FMath::Max(0.f, 1.f-FMath::Square((FMath::Abs(Y)-.50f)/.32f)));
				const float Inner = Outer * (.15f + .40f*Scallop);
				const float HoleY = (FMath::Abs(Y)-.145f)/.078f;
				const float Hole = .155f * FMath::Sqrt(FMath::Max(0.f, 1.f-HoleY*HoleY));
				const float Middle = FMath::Lerp(Inner, Outer, .54f);
				Points.Append({{Inner,Y}, {Middle-Hole,Y}, {Middle+Hole,Y}, {Outer,Y}});
				if (Row == 0) { continue; }
				for (int32 Column : {0, 2})
				{
					const SlateIndex I = SlateIndex((Row-1)*4 + Column);
					Indices.Append({I, SlateIndex(I+1), SlateIndex(I+4), SlateIndex(I+4), SlateIndex(I+1), SlateIndex(I+5)});
				}
			}
		}
	};

	void DrawBodyWings(const FGeometry& G, FSlateWindowElementList& E, int32 Layer, const FProjectedDartBody& Body,
		double HalfHeight, double Width, double Gap, float Pulse)
	{
		static const FDartWingStencil Stencil;
		const FVector2D Along(-Body.Right.Y, Body.Right.X);
		TArray<FSlateVertex> Vertices;
		TArray<SlateIndex> Indices;
		Vertices.Reserve(2*Stencil.Points.Num());
		Indices.Reserve(2*Stencil.Indices.Num());
		for (double Sign : {-1., 1.})
		{
			const FVector2D Out = Body.Right * Sign;
			const FVector2D Base = Body.Center + Out * (Body.Reach(Out) + Gap);
			const SlateIndex Offset = SlateIndex(Vertices.Num());
			for (int32 I = 0; I < Stencil.Points.Num(); ++I)
			{
				const FVector2f Point = Stencil.Points[I];
				const FVector2D At = Base + Out * (Point.X * Width * (1.+.09*Pulse)) + Along * (Point.Y * HalfHeight);
				// Lift just the inside of the silhouette to steel; both contours and the eyelets stay legible.
				const bool bInside = I%4 == 1 || I%4 == 2;
				const uint8 Glint = bInside ? FMath::RoundToInt(210.f*Pulse*(1.f-.58f*FMath::Abs(Point.Y))) : 0;
				Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),
					FVector2f(At), FVector2f(.5f), FColor(Glint,Glint,Glint)));
			}
			for (SlateIndex I : Stencil.Indices) { Indices.Add(Offset + I); }
		}
		const auto Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		FSlateDrawElement::MakeCustomVerts(E, Layer, Resource, Vertices, Indices, nullptr, 0, 0);
	}

	void DrawEdgeTrail(const FGeometry& G, FSlateWindowElementList& E, int32 Layer, FVector2D Size,
			const FProjectedDartBody& Owner, FVector2D Bearing, float Pulse)
	{
		const FVector2D Inset = Size * .026 + FVector2D(8);
		const FBox2D Frame(Inset, Size-Inset);
		const FVector2D Origin = Frame.GetClosestPointTo(Owner.Center);
		const auto ToEdge = [](double Position, double Axis, double Min, double Max)
		{
			return FMath::IsNearlyZero(Axis) ? UE_BIG_NUMBER : ((Axis > 0 ? Max : Min)-Position)/Axis;
		};
		const double Distance = FMath::Min(ToEdge(Origin.X,Bearing.X,Frame.Min.X,Frame.Max.X),
			ToEdge(Origin.Y,Bearing.Y,Frame.Min.Y,Frame.Max.Y));
		const FVector2D Edge = Origin + Bearing*Distance;
		const FVector2D Start = Owner.Center + Bearing*(Owner.Reach(Bearing)+7.);
		const double Length = FVector2D::DotProduct(Edge-Start, Bearing) - 12.;
		if (Length > 0)
		{
			const double DotSize = FMath::Clamp(Size.Y*.0058, 3.6, 6.);
			const int32 Count = FMath::Clamp(FMath::CeilToInt(Length/(DotSize*4.)), 1, 72);
			for (int32 I = 0; I < Count; ++I)
			{
				const FVector2D At = Start + Bearing*(Length*(I+.5)/Count) - FVector2D(DotSize*.5);
				FSlateDrawElement::MakeBox(E, Layer, G.ToPaintGeometry(FVector2D(DotSize), FSlateLayoutTransform(At)),
					FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor::Black);
			}
		}
		const FVector2D Across(-Bearing.Y, Bearing.X);
		TArray<FSlateVertex> Vertices;
		const double MarkerSize = FMath::Clamp(Size.Y*.022, 13., 25.)*(1.+.12*Pulse);
		for (double Sign : {-1., 1.})
		{
			for (FVector2D P : {FVector2D(-.72,.90), FVector2D(-.50,.32), FVector2D(.35,.07), FVector2D(-.03,.58)})
			{
				const FVector2D At = Edge + (Bearing*P.X + Across*(P.Y*Sign))*MarkerSize;
				Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),
					FVector2f(At), FVector2f(.5f), FColor::Black));
			}
		}
		const TArray<SlateIndex> Indices = {0,1,2, 0,2,3, 4,5,6, 4,6,7};
		const auto Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		FSlateDrawElement::MakeCustomVerts(E, Layer, Resource, Vertices, Indices, nullptr, 0, 0);
	}
}

FHALBatarangPinView UHALBatarangWidget::GetPinView() const
{
	FHALBatarangPinView View;
	const auto* Shot = Dart.Get();
	const auto* PC = GetOwningPlayer();
	if (!Shot || !PC || !PC->GetPawn() || PC->GetPawn() != Shot->GetOwner()) { return View; }
	const auto& State = Shot->GetState();
	if (State.Phase != EHALDartPhase::Pinned || !State.Config) { return View; }
	View.bPinned = true;
	View.bCanDetonate = Shot->IsManualInputOpen();
	View.RemainingSeconds = FMath::Clamp(float(State.PhaseStart + State.Config->FuseSeconds - Shot->ServerTime()), 0.f, State.Config->FuseSeconds);
	View.FusePulse = State.Config->FusePulse(View.RemainingSeconds);
	if (IsValid(State.Target) && !UHALSmokeScreenView::ShouldHideWorldTarget(State.Target))
	{
		View.VisibleVehicle = State.Target;
		View.PushDirection = Shot->PushDirection();
	}
	return View;
}

TSharedRef<SWidget> UHALBatarangWidget::RebuildWidget()
{
	bUseNativeArt = !(WidgetTree && WidgetTree->RootWidget);
	return bUseNativeArt ? SNew(SBox) : Super::RebuildWidget();
}

void UHALBatarangWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetIsFocusable(false);
	ForceVolatile(true);
}

int32 UHALBatarangWidget::NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull,
	FSlateWindowElementList& E, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
	const int32 Base = Super::NativePaint(Args,G,Cull,E,Layer,Style,bEnabled);
	if (!bUseNativeArt) { return Base; }
	const auto View = GetPinView();
	const FVector2D Size = G.GetLocalSize();
	if (!View.bPinned || Size.GetMin() < 36.) { return Base; }
	const APlayerController& PC = *GetOwningPlayer();
	const float Pulse = View.FusePulse*(View.bCanDetonate ? 1.f : .55f);
	if (!View.VisibleVehicle)
	{
		// Fixed above the skill meter, with no positional or directional information about the concealed car.
		FProjectedDartBody Glyph;
		Glyph.Center = FVector2D(Size.X*.5,Size.Y*.86);
		const double HalfHeight = FMath::Clamp(Size.Y*.014, 7., 15.);
		DrawBodyWings(G,E,Base+1,Glyph,HalfHeight,HalfHeight*.89,2.,Pulse);
		return Base+1;
	}

	FProjectedDartBody Body;
	const bool bProjected = ProjectBody(PC,*View.VisibleVehicle,Body);
	const FBox2D Screen(FVector2D::ZeroVector, Size);
	if (!bProjected || !Screen.IsInside(Body.Center))
	{
		FProjectedDartBody Owner;
		if (!ProjectBody(PC,*PC.GetPawn(),Owner)) { Owner.Center = Size*.5; }
		FVector2D Bearing = Body.Center - Owner.Center;
		if (!bProjected)
		{
			FVector Eye; FRotator Facing;
			PC.GetPlayerViewPoint(Eye, Facing);
			const FVector Local = Facing.UnrotateVector(View.VisibleVehicle->GetActorLocation()-Eye);
			Bearing = FVector2D(Local.Y,-Local.Z);
		}
		if (Bearing.Normalize()) { DrawEdgeTrail(G,E,Base+1,Size,Owner,Bearing,Pulse); }
		return Base+1;
	}

	const FVector2D Along(-Body.Right.Y, Body.Right.X);
	const double HalfHeight = FMath::Clamp(.38*(Body.Reach(Along)+Body.Reach(-Along)), 12., FMath::Max(12.,Size.Y*.20));
	const double Width = HalfHeight*.58;
	const double Gap = FMath::Clamp(HalfHeight*.105, 3., 14.);
	DrawBodyWings(G,E,Base+1,Body,HalfHeight,Width,Gap,Pulse);

	const FVector World = View.VisibleVehicle->GetActorLocation();
	FVector2D From, To;
	if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(&PC,World,From,true) &&
		UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(&PC,World+View.PushDirection*100.,To,true))
	{
		const FVector2D Axis = (To-From).GetSafeNormal();
		if (!Axis.IsNearlyZero())
		{
			const FVector2D Across(-Axis.Y, Axis.X);
			const double WingClearance = (Gap+Width*1.09)*FMath::Abs(FVector2D::DotProduct(Axis,Body.Right));
			const double Radius = FMath::Clamp(HalfHeight*.44, 7.5, FMath::Max(7.5,Size.Y*.075));
			const FVector2D Back = Body.Center + Axis*(Body.Reach(Axis)+WingClearance+Gap);
			const FVector2D Upper = Back-Across*Radius, Lower = Back+Across*Radius;
			const TArray<FVector2D> Chevron = {Upper, Upper+Axis*(Radius*.33), Back+Axis*(Radius*1.25),
				Lower+Axis*(Radius*.33), Lower, Back+Axis*(Radius*.86), Upper};
			FSlateDrawElement::MakeLines(E,Base+1,G.ToPaintGeometry(),Chevron,ESlateDrawEffect::None,
				FLinearColor::Black,true,FMath::Clamp(float(Radius*.14),1.7f,4.f));
		}
	}
	return Base+1;
}
