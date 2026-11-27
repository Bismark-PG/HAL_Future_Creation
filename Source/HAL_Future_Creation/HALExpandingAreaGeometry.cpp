#include "HALExpandingAreaGeometry.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

void HALExpandingArea::DrawGroundDebug(const AActor& Area, float Radius, float HalfHeight, FColor Color)
{
#if ENABLE_DRAW_DEBUG
	// Presentation only: use a supporting surface below the center, without moving the authority volume.
	FVector Center = Area.GetActorLocation();
	FVector Normal = FVector::UpVector;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(ExpandingAreaDebugGround), false, &Area);
	if (Area.GetInstigator()) { Query.AddIgnoredActor(Area.GetInstigator()); }
	// A generator's stationary mesh collision is not the floor beneath its spawned field.
	if (Area.GetOwner()) { Query.AddIgnoredActor(Area.GetOwner()); }
	TArray<FHitResult> Hits;
	Area.GetWorld()->LineTraceMultiByObjectType(Hits, Center + FVector::UpVector * HalfHeight,
		Center - FVector::UpVector * HalfHeight, Objects, Query);
	for (const auto& Hit : Hits)
	{
		const auto* Surface = Hit.GetComponent();
		if (!Surface || Surface->IsSimulatingPhysics() || !Surface->IsPhysicsCollisionEnabled()
			|| Hit.bStartPenetrating || Hit.ImpactNormal.Z < .1f) { continue; }
		Center = Hit.ImpactPoint;
		Normal = Hit.ImpactNormal;
		break;
	}
	const auto GroundPoint = [&](double X, double Y)
	{
		// Keep world-XY radius identical to the gameplay cylinder, including on slopes.
		return Center + FVector(X, Y, -(Normal.X * X + Normal.Y * Y) / Normal.Z + 3.);
	};
	constexpr int32 Segments = 64;
	TArray<FVector> Vertices;
	TArray<int32> Indices;
	Vertices.Reserve(Segments + 1);
	Indices.Reserve(Segments * 3);
	Vertices.Add(GroundPoint(0, 0));
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const double Angle = 2. * UE_DOUBLE_PI * Index / Segments;
		Vertices.Add(GroundPoint(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius));
		Indices.Append({0, (Index + 1) % Segments + 1, Index + 1});
	}
	DrawDebugMesh(Area.GetWorld(), Vertices, Indices, FColor(Color.R, Color.G, Color.B, 45), false, 0);
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		DrawDebugLine(Area.GetWorld(), Vertices[Index + 1], Vertices[(Index + 1) % Segments + 1], Color, false, 0, 0, 3);
	}
	// At most 33 lines per direction, even for the largest authored area.
	const double Spacing = FMath::Max(100., Radius / 16.);
	const int32 Count = FMath::FloorToInt(Radius / Spacing);
	for (int32 Index = -Count; Index <= Count; ++Index)
	{
		const double Offset = Index * Spacing;
		const double Reach = FMath::Sqrt(FMath::Max(0., Radius * Radius - Offset * Offset));
		DrawDebugLine(Area.GetWorld(), GroundPoint(Offset, -Reach), GroundPoint(Offset, Reach), Color, false, 0, 0, 1.5f);
		DrawDebugLine(Area.GetWorld(), GroundPoint(-Reach, Offset), GroundPoint(Reach, Offset), Color, false, 0, 0, 1.5f);
	}
#endif
}

