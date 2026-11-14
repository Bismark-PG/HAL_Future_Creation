#pragma once

#include "CoreMinimal.h"
#include "HALVehicleFXMount.generated.h"

class UMeshComponent;
struct FHALVehicleFXTargetSnapshot;

/** Cosmetic attachment on an explicitly registered presentation mesh, never the collision body. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALVehicleFXMount
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Mount") FName PartId = TEXT("Body");
	/** Socket is preferred; an existing skeletal bone name is also accepted. None uses mesh origin. */
	UPROPERTY(EditAnywhere, Category="Mount") FName SocketOrBone;
	/** Relative to the socket/bone, or the mesh origin when no usable socket is selected. Units: cm. */
	UPROPERTY(EditAnywhere, Category="Mount") FTransform Offset = FTransform::Identity;
	/** Explicit fallback for a missing named socket. A missing Part Id always skips the layer. */
	UPROPERTY(EditAnywhere, Category="Mount") bool bFallbackToMeshOrigin = false;
	bool IsValid() const;
};

struct HAL_FUTURE_CREATION_API FHALResolvedFXMount
{
	TWeakObjectPtr<UMeshComponent> Mesh;
	FName Socket;
	FTransform Relative = FTransform::Identity;
	FTransform World = FTransform::Identity;
	bool bUsedFallback = false;
};

namespace HALVehicleFX
{
	/** Re-query on mesh replacement; the returned World transform is valid only for this frame. */
	HAL_FUTURE_CREATION_API bool ResolveMount(const FHALVehicleFXTargetSnapshot& Targets,
		const FHALVehicleFXMount& Mount, FHALResolvedFXMount& Out);
}
