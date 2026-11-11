#include "HALGroundSurfacePresentation.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Materials/MaterialInstanceDynamic.h"

bool FHALGroundSurfaceDeformation::IsValid() const
{
	return WidthSubdivisions >= 2 && WidthSubdivisions <= 16 && WidthSubdivisions % 2 == 0
		&& VertexSpacing >= 20 && VertexSpacing <= 200 && BulgeHeight >= 0 && BulgeHeight <= 60
		&& WaveAmplitude >= 0 && WaveAmplitude <= 20 && WaveAmplitude <= BulgeHeight
		&& WavePeriod >= .5f && WavePeriod <= 20 && WaveLength >= 40 && WaveLength <= 2000
		&& WaveDirection >= -180 && WaveDirection <= 180;
}

namespace
{
	constexpr int32 MaximumSourcePoints = 256;
	constexpr int32 MaximumRows = 512;
	constexpr int32 CapSegments = 12;
	struct FSurfaceRow
	{
		FHALGroundSurfacePoint Point;
		FVector Direction = FVector::ForwardVector;
		int32 FirstVertex = INDEX_NONE;
	};
}

UE::Geometry::FDynamicMesh3 HALGroundSurface::BuildRibbon(TConstArrayView<FHALGroundSurfacePoint> Points,
	const FVector& Origin, float GroundLift, float TextureTileSize, const FHALGroundSurfaceDeformation& Deformation)
{
	using namespace UE::Geometry;
	FDynamicMesh3 Mesh;
	if (Points.IsEmpty() || Points.Num() > MaximumSourcePoints || !Deformation.IsValid()
		|| !FMath::IsFinite(GroundLift) || !FMath::IsFinite(TextureTileSize) || TextureTileSize <= 0 || Origin.ContainsNaN()) { return Mesh; }
	double TotalLength = 0;
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		const auto& Point = Points[Index];
		if (Point.Ground.ContainsNaN() || Point.Normal.ContainsNaN() || Point.Normal.Z < .09
			|| !FMath::IsFinite(Point.HalfWidth) || Point.HalfWidth <= 0) { return Mesh; }
		if (Index > 0 && Point.bLinked) { TotalLength += FVector::Dist2D(Points[Index - 1].Ground, Point.Ground); }
	}
	const double Spacing = FMath::Max(double(Deformation.VertexSpacing), TotalLength / (MaximumRows - Points.Num()));
	TArray<FSurfaceRow> Rows;
	Rows.Reserve(MaximumRows);
	Rows.AddDefaulted_GetRef().Point = Points[0];
	Rows[0].Point.bLinked = false;
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		const auto& A = Points[Index - 1];
		const auto& B = Points[Index];
		const double Distance = FVector::Dist2D(A.Ground, B.Ground);
		const int32 Desired = Deformation.bEnabled && B.bLinked ? FMath::CeilToInt(Distance / Spacing) : 1;
		const int32 Steps = FMath::Clamp(Desired, 1, MaximumRows - Rows.Num() - (Points.Num() - Index - 1));
		for (int32 Step = 1; Step <= Steps; ++Step)
		{
			const float Alpha = float(Step) / Steps;
			auto& Row = Rows.AddDefaulted_GetRef();
			Row.Point.Ground = FMath::Lerp(A.Ground, B.Ground, Alpha);
			Row.Point.Normal = FMath::Lerp(A.Normal, B.Normal, Alpha).GetSafeNormal();
			Row.Point.HalfWidth = FMath::Lerp(A.HalfWidth, B.HalfWidth, Alpha);
			Row.Point.bLinked = B.bLinked;
		}
	}

	Mesh.EnableAttributes();
	Mesh.Attributes()->SetNumUVLayers(2);
	Mesh.Attributes()->EnablePrimaryColors();
	auto* UVs = Mesh.Attributes()->PrimaryUV();
	auto* Gradients = Mesh.Attributes()->GetUVLayer(1);
	auto* Normals = Mesh.Attributes()->PrimaryNormals();
	auto* Colors = Mesh.Attributes()->PrimaryColors();
	const int32 Across = Deformation.bEnabled ? Deformation.WidthSubdivisions : 2;
	const auto AddTriangle = [&](int32 A, int32 B, int32 C)
	{
		const double CrossZ = FVector::CrossProduct(Mesh.GetVertex(B) - Mesh.GetVertex(A), Mesh.GetVertex(C) - Mesh.GetVertex(A)).Z;
		if (FMath::Abs(CrossZ) <= UE_SMALL_NUMBER) { return; }
		if (CrossZ > 0) { Swap(B, C); }
		const FIndex3i Vertices(A, B, C);
		const int32 Triangle = Mesh.AppendTriangle(Vertices);
		if (Triangle >= 0)
		{
			UVs->SetTriangle(Triangle, Vertices);
			Gradients->SetTriangle(Triangle, Vertices);
			Normals->SetTriangle(Triangle, Vertices);
			Colors->SetTriangle(Triangle, Vertices);
		}
	};
	const auto AddVertex = [&](const FHALGroundSurfacePoint& Point, FVector Offset, float Weight, FVector2f Gradient)
	{
		const FVector Normal = Point.Normal.GetSafeNormal();
		Offset.Z = -(Normal.X * Offset.X + Normal.Y * Offset.Y) / Normal.Z + GroundLift;
		const FVector Position = Point.Ground - Origin + Offset;
		const int32 Vertex = Mesh.AppendVertex(Position);
		UVs->AppendElement(FVector2f(Position.X / TextureTileSize, Position.Y / TextureTileSize));
		// Smooth analytic d(weight)/d(world XY), in inverse centimeters, for stylized volume normals.
		Gradients->AppendElement(Gradient);
		Normals->AppendElement(FVector3f(Normal));
		Colors->AppendElement(FVector4f(Weight, 0, 0, 1));
		return Vertex;
	};
	const auto LinkDirection = [&](int32 Index) -> FVector
	{
		return Index > 0 && Index < Rows.Num() && Rows[Index].Point.bLinked
			? (Rows[Index].Point.Ground - Rows[Index - 1].Point.Ground).GetSafeNormal2D() : FVector::ZeroVector;
	};
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		auto& Row = Rows[Index];
		const FVector Incoming = LinkDirection(Index), Outgoing = LinkDirection(Index + 1);
		const FVector Fallback = !Outgoing.IsZero() ? Outgoing : !Incoming.IsZero() ? Incoming : FVector::ForwardVector;
		Row.Direction = (Incoming + Outgoing).GetSafeNormal(UE_SMALL_NUMBER, Fallback);
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Row.Direction);
		for (int32 Column = 0; Column <= Across; ++Column)
		{
			const float Coordinate = 2.f * Column / Across - 1.f;
			const int32 Vertex = AddVertex(Row.Point, Side * (Coordinate * Row.Point.HalfWidth), 1.f - Coordinate * Coordinate,
				FVector2f(Side.X, Side.Y) * (-2.f * Coordinate / Row.Point.HalfWidth));
			if (Column == 0) { Row.FirstVertex = Vertex; }
		}
	}
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const auto& Row = Rows[Index];
		const bool bIncoming = !LinkDirection(Index).IsZero();
		const bool bOutgoing = !LinkDirection(Index + 1).IsZero();
		if (bIncoming)
		{
			const int32 Previous = Rows[Index - 1].FirstVertex;
			for (int32 Column = 0; Column < Across; ++Column)
			{
				AddTriangle(Previous + Column, Previous + Column + 1, Row.FirstVertex + Column + 1);
				AddTriangle(Previous + Column, Row.FirstVertex + Column + 1, Row.FirstVertex + Column);
			}
		}
		const auto AddCap = [&](float Sign)
		{
			const FVector Along = Row.Direction * Sign;
			const FVector Side = FVector::CrossProduct(FVector::UpVector, Row.Direction) * -Sign;
			const int32 Center = Row.FirstVertex + Across / 2;
			TArray<int32, TInlineAllocator<CapSegments + 1>> Inner;
			for (int32 Ring = 1; Ring <= Across / 2; ++Ring)
			{
				const float Radius = float(Ring) / (Across / 2);
				TArray<int32, TInlineAllocator<CapSegments + 1>> Outer;
				Outer.Add(Center - int32(Sign) * Ring);
				for (int32 Segment = 1; Segment < CapSegments; ++Segment)
				{
					const double Angle = UE_DOUBLE_PI * Segment / CapSegments;
					const FVector Radial = Side * FMath::Cos(Angle) + Along * FMath::Sin(Angle);
					Outer.Add(AddVertex(Row.Point, Radial * (Row.Point.HalfWidth * Radius), 1.f - Radius * Radius,
						FVector2f(Radial.X, Radial.Y) * (-2.f * Radius / Row.Point.HalfWidth)));
				}
				Outer.Add(Center + int32(Sign) * Ring);
				for (int32 Segment = 0; Segment < CapSegments; ++Segment)
				{
					if (Inner.IsEmpty()) { AddTriangle(Center, Outer[Segment], Outer[Segment + 1]); }
					else
					{
						AddTriangle(Inner[Segment], Outer[Segment], Outer[Segment + 1]);
						AddTriangle(Inner[Segment], Outer[Segment + 1], Inner[Segment + 1]);
					}
				}
				Inner = MoveTemp(Outer);
			}
		};
		if (!bIncoming) { AddCap(-1); }
		if (!bOutgoing) { AddCap(1); }
	}
	return Mesh;
}

UHALGroundSurfaceComponent::UHALGroundSurfaceComponent()
{
	SetMobility(EComponentMobility::Movable);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetDeferredCollisionUpdatesEnabled(true, false);
	SetCastShadow(false);
}

void UHALGroundSurfaceComponent::ConfigureSurface(UMaterialInterface* Material, float TextureTileSize,
	const FHALGroundSurfaceDeformation& Deformation)
{
	if (SourceMaterial == Material && TileSize == TextureTileSize && Settings == Deformation) { return; }
	Settings = Deformation;
	TileSize = TextureTileSize;
	SourceMaterial = Material;
	RuntimeMaterial = nullptr;
	SetCastShadow(Material && Settings.bEnabled && Settings.IsValid() && Settings.bCastShadow);
	if (Material && Settings.bEnabled && Settings.IsValid())
	{
		RuntimeMaterial = UMaterialInstanceDynamic::Create(Material, this);
		RuntimeMaterial->SetScalarParameterValue(TEXT("HAL_GroundBulgeHeight"), Settings.BulgeHeight);
		RuntimeMaterial->SetScalarParameterValue(TEXT("HAL_GroundWaveAmplitude"), Settings.WaveAmplitude);
		RuntimeMaterial->SetScalarParameterValue(TEXT("HAL_GroundInvTileSize"), 1.f / TileSize);
		const float Angle = FMath::DegreesToRadians(Settings.WaveDirection);
		const float Scale = TileSize / Settings.WaveLength;
		RuntimeMaterial->SetVectorParameterValue(TEXT("HAL_GroundWaveUV"), FLinearColor(FMath::Cos(Angle) * Scale, FMath::Sin(Angle) * Scale, 0, 0));
	}
	SetMaterial(0, RuntimeMaterial ? RuntimeMaterial.Get() : Material);
	UpdateBounds();
	MarkRenderTransformDirty();
}

void UHALGroundSurfaceComponent::SetSurfaceAge(float Age)
{
	if (RuntimeMaterial && FMath::IsFinite(Age))
	{
		RuntimeMaterial->SetScalarParameterValue(TEXT("HAL_GroundWavePhase"), FMath::Fmod(FMath::Max(Age, 0.f) / Settings.WavePeriod, 1.f));
	}
}

FBoxSphereBounds UHALGroundSurfaceComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	const auto Base = Super::CalcBounds(LocalToWorld);
	const double Height = Settings.IsValid() ? Settings.MaximumDisplacement() : 0;
	const FVector HalfDisplacement = FVector::UpVector * (Height * .5);
	const FVector Extent = Base.BoxExtent + HalfDisplacement;
	return FBoxSphereBounds(Base.Origin + HalfDisplacement, Extent, Extent.Size());
}
