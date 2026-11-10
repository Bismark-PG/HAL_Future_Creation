#include "HALSlipperyTrail.h"

#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"

bool FHALSlipperyTrailPresentation::HasUsableMaterial() const
{
	const UMaterial* BaseMaterial = GroundMaterial ? GroundMaterial->GetMaterial() : nullptr;
	return BaseMaterial && BaseMaterial->MaterialDomain == MD_Surface
		&& GroundLift >= .1f && GroundLift <= 20.f && TextureTileSize >= 20.f && TextureTileSize <= 2000.f;
}

UE::Geometry::FDynamicMesh3 AHALSlipperyTrail::BuildGroundMesh(TConstArrayView<FHALSlipperyTrailPoint> Path,
	const FHALSlipperyTrailShape& Shape, const FHALSlipperyTrailPresentation& Look, float Age)
{
	using namespace UE::Geometry;
	FDynamicMesh3 Mesh;
	if (Path.IsEmpty() || Age < 0 || Age >= Shape.LifetimeSeconds) { return Mesh; }
	int32 VisiblePoints = 0;
	while (VisiblePoints < Path.Num() && Path[VisiblePoints].EmissionAge <= Age) { ++VisiblePoints; }
	if (VisiblePoints == 0) { return Mesh; }

	Mesh.EnableAttributes();
	auto* UVs = Mesh.Attributes()->PrimaryUV();
	auto* Normals = Mesh.Attributes()->PrimaryNormals();
	const FVector Origin = Path[0].Ground;
	const auto AddTriangle = [&](int32 A, int32 B, int32 C)
	{
		const double CrossZ = FVector::CrossProduct(Mesh.GetVertex(B) - Mesh.GetVertex(A),
			Mesh.GetVertex(C) - Mesh.GetVertex(A)).Z;
		if (FMath::Abs(CrossZ) <= UE_SMALL_NUMBER) { return; }
		if (CrossZ > 0) { Swap(B, C); }
		const FIndex3i Vertices(A, B, C);
		const int32 Triangle = Mesh.AppendTriangle(Vertices);
		if (Triangle >= 0)
		{
			// Each vertex and its two overlay elements are appended together in the same order.
			UVs->SetTriangle(Triangle, Vertices);
			Normals->SetTriangle(Triangle, Vertices);
		}
	};

	const auto LinkDirection = [&](int32 Index) -> FVector
	{
		if (Index <= 0 || Index >= VisiblePoints || !Path[Index].bLinked) { return FVector::ZeroVector; }
		return (Path[Index].Ground - Path[Index - 1].Ground).GetSafeNormal2D();
	};

	int32 PreviousLeft = INDEX_NONE;
	int32 PreviousRight = INDEX_NONE;
	for (int32 Index = 0; Index < VisiblePoints; ++Index)
	{
		const auto& Point = Path[Index];
		const FVector Incoming = LinkDirection(Index);
		const FVector Outgoing = LinkDirection(Index + 1);
		const bool bIncoming = !Incoming.IsZero();
		const bool bOutgoing = !Outgoing.IsZero();
		const FVector Fallback = bOutgoing ? Outgoing : bIncoming ? Incoming : FVector::ForwardVector;
		const FVector Direction = (Incoming + Outgoing).GetSafeNormal(UE_SMALL_NUMBER, Fallback);
		const FVector Across = FVector::CrossProduct(FVector::UpVector, Direction);
		const float Width = Shape.HalfWidthAt(Age - Point.EmissionAge);
		const FVector Center = Point.Ground - Origin;
		const FVector Normal = Point.GroundNormal.Z >= .09 ? FVector(Point.GroundNormal).GetSafeNormal() : FVector::UpVector;
		const auto AddVertex = [&](FVector Offset)
		{
			// Use the sampled ground plane while keeping the XY width in gameplay units.
			Offset.Z = -(Normal.X * Offset.X + Normal.Y * Offset.Y) / Normal.Z + Look.GroundLift;
			const FVector Position = Center + Offset;
			const int32 Vertex = Mesh.AppendVertex(Position);
			UVs->AppendElement(FVector2f(Position.X / Look.TextureTileSize, Position.Y / Look.TextureTileSize));
			Normals->AppendElement(FVector3f(Normal));
			return Vertex;
		};

		const int32 Left = AddVertex(-Across * Width);
		const int32 Right = AddVertex(Across * Width);
		if (bIncoming)
		{
			AddTriangle(PreviousLeft, PreviousRight, Right);
			AddTriangle(PreviousLeft, Right, Left);
		}

		// Join adjacent sections with a shared cross-section. Round off open ends and isolated points.
		const auto AddCap = [&](const FVector& Along, const FVector& Side, int32 First, int32 Last)
		{
			constexpr int32 ArcSegments = 12;
			const int32 CapCenter = AddVertex(FVector::ZeroVector);
			int32 Previous = First;
			for (int32 Segment = 1; Segment < ArcSegments; ++Segment)
			{
				const double Angle = UE_DOUBLE_PI * Segment / ArcSegments;
				const int32 Next = AddVertex((Side * FMath::Cos(Angle) + Along * FMath::Sin(Angle)) * Width);
				AddTriangle(CapCenter, Previous, Next);
				Previous = Next;
			}
			AddTriangle(CapCenter, Previous, Last);
		};
		if (!bIncoming) { AddCap(-Direction, Across, Right, Left); }
		if (!bOutgoing) { AddCap(Direction, -Across, Left, Right); }
		PreviousLeft = Left;
		PreviousRight = Right;
	}
	return Mesh;
}

void AHALSlipperyTrail::OnRep_PresentationInput()
{
	bGroundMeshDirty = true;
	if (!PrimaryActorTick.bCanEverTick || IsNetMode(NM_DedicatedServer)) { return; }
	const bool bDrawOutline = ENABLE_DRAW_DEBUG && Shape.bDrawOutline;
	const bool bDrawGround = Presentation.HasUsableMaterial();
	if (!bDrawGround) { HideGroundMesh(); }
	SetActorTickEnabled(bDrawOutline || bDrawGround);
}

void AHALSlipperyTrail::UpdateGroundMesh(float Age)
{
	if (!Presentation.HasUsableMaterial()) { return; }
	const float ExpansionEnd = Points.Last().EmissionAge + Shape.ExpansionSeconds;
	const bool bClockRewound = Age < LastMeshAge;
	const bool bNeedsFinalWidth = LastMeshAge < ExpansionEnd;
	if (!bGroundMeshDirty && !bClockRewound && !bNeedsFinalWidth) { return; }
	constexpr float MeshUpdateInterval = 1.f / 30.f;
	if (!bGroundMeshDirty && !bClockRewound && Age - LastMeshAge < MeshUpdateInterval) { return; }

	auto Mesh = BuildGroundMesh(Points, Shape, Presentation, Age);
	if (Mesh.TriangleCount() == 0)
	{
		HideGroundMesh();
		return;
	}
	const bool bNewComponent = !GroundMesh;
	if (bNewComponent)
	{
		GroundMesh = NewObject<UDynamicMeshComponent>(this);
		SetRootComponent(GroundMesh);
		GroundMesh->SetMobility(EComponentMobility::Movable);
		GroundMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GroundMesh->SetGenerateOverlapEvents(false);
		GroundMesh->SetCanEverAffectNavigation(false);
		GroundMesh->SetDeferredCollisionUpdatesEnabled(true, false);
		GroundMesh->SetCastShadow(false);
		GroundMesh->SetWorldLocation(Points[0].Ground);
	}
	if (GroundMesh->GetMaterial(0) != Presentation.GroundMaterial)
	{
		GroundMesh->SetMaterial(0, Presentation.GroundMaterial);
	}
	GroundMesh->SetMesh(MoveTemp(Mesh));
	if (bNewComponent) { GroundMesh->RegisterComponent(); }
	GroundMesh->SetVisibility(true);
	LastMeshAge = Age;
	bGroundMeshDirty = false;
}

void AHALSlipperyTrail::HideGroundMesh()
{
	if (GroundMesh) { GroundMesh->SetVisibility(false); }
}
