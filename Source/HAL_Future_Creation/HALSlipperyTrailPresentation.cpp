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
		&& DissolveSeconds >= 0 && DissolveSeconds <= 5
		&& Deformation.IsValid();
}

float FHALSlipperyTrailPresentation::DissolveProgress(float Age, float GameplayLifetime) const
{
	if (Age < GameplayLifetime) { return 0; }
	return DissolveSeconds > 0 ? FMath::Clamp((Age - GameplayLifetime) / DissolveSeconds, 0.f, 1.f) : 1.f;
}

UE::Geometry::FDynamicMesh3 AHALSlipperyTrail::BuildGroundMesh(TConstArrayView<FHALSlipperyTrailPoint> Path,
	const FHALSlipperyTrailShape& Shape, const FHALSlipperyTrailPresentation& Look, float Age)
{
	if (Path.IsEmpty() || Age < 0 || Age >= Shape.LifetimeSeconds + Look.DissolveSeconds) { return {}; }
	Age = FMath::Min(Age, Shape.LifetimeSeconds);
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
		if (Presentation.DissolveSeconds > 0) { GroundMesh->SetDissolveProgress(Presentation.DissolveProgress(Age, Shape.LifetimeSeconds)); }
	}
	const float ShapeAge = FMath::Min(Age, Shape.LifetimeSeconds);
	const float ExpansionEnd = Points.Last().EmissionAge + Shape.ExpansionSeconds;
	const bool bClockRewound = ShapeAge < LastMeshAge;
	const bool bNeedsFinalWidth = LastMeshAge < FMath::Min(ExpansionEnd, Shape.LifetimeSeconds);
	if (!bGroundMeshDirty && !bClockRewound && !bNeedsFinalWidth) { return; }
	constexpr float MeshUpdateInterval = 1.f / 30.f;
	if (!bGroundMeshDirty && !bClockRewound && ShapeAge - LastMeshAge < MeshUpdateInterval) { return; }

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
	if (Presentation.DissolveSeconds > 0) { GroundMesh->SetDissolveProgress(Presentation.DissolveProgress(Age, Shape.LifetimeSeconds)); }
	GroundMesh->SetMesh(MoveTemp(Mesh));
	if (bNewComponent) { GroundMesh->RegisterComponent(); }
	GroundMesh->SetVisibility(true);
	LastMeshAge = ShapeAge;
	bGroundMeshDirty = false;
}

void AHALSlipperyTrail::HideGroundMesh()
{
	if (GroundMesh) { GroundMesh->SetVisibility(false); }
}
