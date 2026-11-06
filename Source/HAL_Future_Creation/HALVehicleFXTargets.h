#pragma once

#include "CoreMinimal.h"

class UMeshComponent;
class UHALVehiclePresentationComponent;

/** Cosmetic geometry only. No collision, gameplay state or mutable render resources. */
enum class EHALVehicleFXMeshKind : uint8 { Static, Skeletal };

struct HAL_FUTURE_CREATION_API FHALVehicleFXTarget
{
	FName Id;
	EHALVehicleFXMeshKind Kind = EHALVehicleFXMeshKind::Static;
	TWeakObjectPtr<UMeshComponent> Mesh;
	TWeakObjectPtr<UObject> MeshAsset;
	FBox LocalBounds = FBox(ForceInit);
	FTransform MeshToWorld = FTransform::Identity;
	FBox VehicleBounds = FBox(ForceInit);

	/** Sequential transforms preserve rotated, nonuniform and mirrored scales. */
	FVector MeshPointToVehicle(const FVector& Point, const FTransform& VehicleToWorld) const;
};

/** Game-thread, current-frame snapshot. Re-query before positioning effects, never cache transforms. */
struct HAL_FUTURE_CREATION_API FHALVehicleFXTargetSnapshot
{
	TWeakObjectPtr<UHALVehiclePresentationComponent> Provider;
	uint32 BindingRevision = 0;
	FTransform VehicleToWorld = FTransform::Identity;
	FBox VehicleBounds = FBox(ForceInit);
	/** Body first when usable; explicit parts follow in lexical Id order. */
	TArray<FHALVehicleFXTarget> Targets;
};

namespace HALVehicleFX
{
	/** Returns false for nonfinite/degenerate input; Out is cleared on failure. Units: cm. */
	HAL_FUTURE_CREATION_API bool BuildBounds(const FBox& LocalBounds, const FTransform& MeshToWorld,
		const FTransform& VehicleToWorld, FBox& OutVehicleBounds);
}
