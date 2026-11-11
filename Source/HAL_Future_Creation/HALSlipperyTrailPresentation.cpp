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
		&& GroundLift >= .1f && GroundLift <= 20.f && TextureTileSize >= 20.f && TextureTileSize <= 2000.f
		&& Deformation.IsValid();
}

UE::Geometry::FDynamicMesh3 AHALSlipperyTrail::BuildGroundMesh(TConstArrayView<FHALSlipperyTrailPoint> Path,
	const FHALSlipperyTrailShape& Shape, const FHALSlipperyTrailPresentation& Look, float Age)
{
	if (Path.IsEmpty() || Age < 0 || Age >= Shape.LifetimeSeconds) { return {}; }
	TArray<FHALGroundSurfacePoint> SurfacePoints;
	for (const auto& Point : Path)
	{
		if (Point.EmissionAge > Age) { break; }
		auto& Surface = SurfacePoints.AddDefaulted_GetRef();
		Surface.Ground = Point.Ground;
		Surface.Normal = Point.GroundNormal;
		Surface.HalfWidth = Shape.HalfWidthAt(Age - Point.EmissionAge);
		Surface.bLinked = Point.bLinked;
	}
	return HALGroundSurface::BuildRibbon(SurfacePoints, Path[0].Ground, Look.GroundLift, Look.TextureTileSize, Look.Deformation);
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
	if (GroundMesh)
	{
		GroundMesh->ConfigureSurface(Presentation.GroundMaterial, Presentation.TextureTileSize, Presentation.Deformation);
		GroundMesh->SetSurfaceAge(Age);
	}
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
		GroundMesh = NewObject<UHALGroundSurfaceComponent>(this);
		SetRootComponent(GroundMesh);
		GroundMesh->SetWorldLocation(Points[0].Ground);
	}
	GroundMesh->ConfigureSurface(Presentation.GroundMaterial, Presentation.TextureTileSize, Presentation.Deformation);
	GroundMesh->SetSurfaceAge(Age);
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
