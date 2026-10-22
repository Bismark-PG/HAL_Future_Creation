#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HALVehicleAssetGeneratorSettings.generated.h"

class USkeletalMesh;
class UBlueprint;
class UVehicleDefinition;

/** Transient editor panel state. No editor-only types are referenced by generated assets. */
UCLASS()
class UHALVehicleAssetGeneratorSettings : public UObject
{
	GENERATED_BODY()
public:
	/** Asset name must be SKM_<model>. Skeleton is taken directly from this mesh. */
	UPROPERTY(EditAnywhere, Category = "Source", meta = (DisplayName = "Skeletal Mesh"))
	TObjectPtr<USkeletalMesh> Mesh;
	/** A compiled TestVehiclePawn Blueprint. Its child inherits network physics and component defaults. */
	UPROPERTY(EditAnywhere, Category = "Source", meta = (DisplayName = "Base Vehicle Blueprint"))
	TObjectPtr<UBlueprint> BasePawn;
	/** Copied into a new DA; all external asset references remain shared. Auto-filled on base Blueprint selection. */
	UPROPERTY(EditAnywhere, Category = "Source", meta = (DisplayName = "Base Vehicle Definition"))
	TObjectPtr<UVehicleDefinition> BaseDefinition;
};
