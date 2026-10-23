#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HALVehicleAssetGeneratorSettings.generated.h"

class USkeletalMesh;
class UNetworkPhysicsSettingsDataAsset;

/** Transient editor panel state. No editor-only types are referenced by generated assets. */
UCLASS()
class UHALVehicleAssetGeneratorSettings : public UObject
{
	GENERATED_BODY()
public:
	/** Asset name must be SKM_<model>. Skeleton is taken directly from this mesh. */
	UPROPERTY(EditAnywhere, Category = "Source", meta = (DisplayName = "Skeletal Mesh"))
	TObjectPtr<USkeletalMesh> Mesh;
	/** Existing vehicle network physics DA. Assigned to the generated native vehicle's settings component. */
	UPROPERTY(EditAnywhere, Category = "Shared Configuration", meta = (DisplayName = "Shared Vehicle Network Physics"))
	TObjectPtr<UNetworkPhysicsSettingsDataAsset> SharedNetworkPhysics;
};
