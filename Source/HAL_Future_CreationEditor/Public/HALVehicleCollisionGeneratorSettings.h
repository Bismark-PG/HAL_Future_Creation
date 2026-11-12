#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HALVehicleCollisionGeneratorSettings.generated.h"

/** Baked mesh dimensions in collision-root local cm. +X is forward, +Y is right, +Z is up. */
USTRUCT()
struct FHALVehicleCollisionParameters
{
	GENERATED_BODY()

	/** Distance from the collision root's X=0 plane to the front (+X). */
	UPROPERTY(EditAnywhere, Category = "Dimensions", meta = (DisplayName = "Front", Units = "cm", ClampMin = "0", ClampMax = "10000"))
	float FrontCm = 100;
	/** Distance from the collision root's X=0 plane to the rear (-X). */
	UPROPERTY(EditAnywhere, Category = "Dimensions", meta = (DisplayName = "Rear", Units = "cm", ClampMin = "0", ClampMax = "10000"))
	float RearCm = 100;
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
	/** XZ end rounding, independent of top-view corners. Height/2 removes the vertical end face. */
	UPROPERTY(EditAnywhere, Category = "Side Corners", meta = (DisplayName = "Front Side Radius", Units = "cm", ClampMin = "0"))
	float FrontSideRadiusCm = 0;
	UPROPERTY(EditAnywhere, Category = "Side Corners", meta = (DisplayName = "Rear Side Radius", Units = "cm", ClampMin = "0"))
	float RearSideRadiusCm = 0;
	/** Polygon approximation per 90 degree arc. More segments increase the collision vertex count. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Corners", meta = (ClampMin = "1", ClampMax = "8"))
	int32 SegmentsPerCorner = 4;

	/** When set, the lower face stays at this Z and HeightCm extends upward. Legacy recipes are centered. */
	TOptional<double> BottomZCm;

	double GetLengthCm() const { return static_cast<double>(FrontCm) + RearCm; }
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
