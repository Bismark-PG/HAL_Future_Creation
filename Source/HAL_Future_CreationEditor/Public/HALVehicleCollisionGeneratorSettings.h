#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HALVehicleCollisionGeneratorSettings.generated.h"

/** Baked mesh dimensions in cm. +X is forward, +Y is right, +Z is up; pivot is the bounds center. */
USTRUCT()
struct FHALVehicleCollisionParameters
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Dimensions", meta = (DisplayName = "Length", Units = "cm", ClampMin = "1", ClampMax = "10000"))
	float LengthCm = 200;
	UPROPERTY(EditAnywhere, Category = "Dimensions", meta = (DisplayName = "Width", Units = "cm", ClampMin = "1", ClampMax = "10000"))
	float WidthCm = 140;
	UPROPERTY(EditAnywhere, Category = "Dimensions", meta = (DisplayName = "Height", Units = "cm", ClampMin = "1", ClampMax = "10000"))
	float HeightCm = 70;
	/** Radius of both +X corners in the XY plane. Width/2 produces a semicircular front. */
	UPROPERTY(EditAnywhere, Category = "Corners", meta = (DisplayName = "Front Corner Radius", Units = "cm", ClampMin = "0"))
	float FrontRadiusCm = 70;
	/** Radius of both -X corners in the XY plane. Width/2 produces a semicircular rear. */
	UPROPERTY(EditAnywhere, Category = "Corners", meta = (DisplayName = "Rear Corner Radius", Units = "cm", ClampMin = "0"))
	float RearRadiusCm = 70;
	/** Polygon approximation per 90 degree arc. More segments increase the collision vertex count. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Corners", meta = (ClampMin = "1", ClampMax = "8"))
	int32 SegmentsPerCorner = 4;

	bool Validate(FString& Error) const;
};

/** Transient panel state only. Recipes are saved as mesh package metadata, with no Editor class reference. */
UCLASS()
class UHALVehicleCollisionGeneratorSettings : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Shape", meta = (ShowOnlyInnerProperties))
	FHALVehicleCollisionParameters Shape;
	UPROPERTY(EditAnywhere, Category = "Output")
	FString OutputFolder = TEXT("/Game/Meshes/Collision");
	/** Existing assets are never overwritten. A numeric suffix is added automatically when necessary. */
	UPROPERTY(EditAnywhere, Category = "Output")
	FString AssetName = TEXT("SM_VehicleCollision_Rounded");
};
