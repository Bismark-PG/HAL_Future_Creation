#pragma once
#include "CoreMinimal.h"
#include "HALShockArcGeometry.generated.h"

/**
 * Shape of the hero arcs that crawl over the body. Pure data: the geometry is built
 * on the CPU and handed to Niagara as a position array, so Niagara only renders ribbons
 * and owns none of the look. Every arc is pinned at both ends and jagged in between,
 * which is what separates an arc from a smooth noodle.
 */
USTRUCT()
struct FHALShockArcSettings
{
	GENERATED_BODY()

	/** Main arcs per rebuild. Each one may also carry a dead-end branch. */
	UPROPERTY(EditAnywhere, Category="Arc", meta=(ClampMin="1", ClampMax="8"))
	int32 ArcCount = 4;

	/** Midpoint displacement passes; point count per ribbon is 2^Subdivisions + 1. */
	UPROPERTY(EditAnywhere, Category="Arc", meta=(ClampMin="2", ClampMax="5"))
	int32 Subdivisions = 4;

	/** Sideways kick per pass, as a fraction of the segment being split. Higher is more jagged. */
	UPROPERTY(EditAnywhere, Category="Arc", meta=(ClampMin="0", ClampMax="1"))
	float Roughness = 0.32f;

	/** How far the arc floats off the body, as a fraction of the body radius. */
	UPROPERTY(EditAnywhere, Category="Arc", meta=(ClampMin="0", ClampMax="0.5"))
	float LiftFraction = 0.07f;

	/** Chance that a given arc also throws a short dead-end branch. */
	UPROPERTY(EditAnywhere, Category="Arc", meta=(ClampMin="0", ClampMax="1"))
	float BranchChance = 0.55f;

	/** Branch length as a fraction of its parent arc. */
	UPROPERTY(EditAnywhere, Category="Arc", meta=(ClampMin="0.05", ClampMax="0.8"))
	float BranchLength = 0.3f;

	UPROPERTY(EditAnywhere, Category="Arc", meta=(Units="cm", ClampMin="0.1", ClampMax="30"))
	float WidthCm = 2.6f;
};

namespace HALShockArcGeometry
{
	/** Points per ribbon for these settings; Niagara needs this to group particles into ribbons. */
	HAL_FUTURE_CREATION_API int32 PointsPerRibbon(const FHALShockArcSettings& Settings);

	/** Ribbon count per rebuild, branches included. Stays constant so the spawn count can be fixed. */
	HAL_FUTURE_CREATION_API int32 RibbonCount(const FHALShockArcSettings& Settings);

	/**
	 * Builds every ribbon into one flat list; ribbon index is PointIndex / PointsPerRibbon.
	 * Positions are in the body's local space and sit on an ellipsoid inscribed in LocalExtent,
	 * so the arcs wrap the vehicle and move with it. A branch that did not roll gets zero
	 * width rather than being dropped, which keeps the layout a fixed stride.
	 */
	HAL_FUTURE_CREATION_API void Build(const FVector& LocalExtent, const FHALShockArcSettings& Settings,
		uint32 Seed, TArray<FVector>& OutPositions, TArray<float>& OutWidths);
}
