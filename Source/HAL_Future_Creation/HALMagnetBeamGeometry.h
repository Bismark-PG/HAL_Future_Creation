#pragma once
#include "CoreMinimal.h"
#include "HALMagnetBeamGeometry.generated.h"

/**
 * Shape of the magnet beam that arcs from a holder's roof onto the ball it controls.
 * Pure data: the curve is sampled on the CPU and handed to Niagara as a position array,
 * so Niagara only renders ribbons and owns none of the look.
 *
 * The beam exists because the ball sits almost flush against the vehicle nose, which
 * leaves no horizontal room for a tether. Vertically there is room, and at the game's
 * -60 degree camera a vertical span still reads at half its length on screen, so the
 * arc occupies empty screen space above the vehicle instead of competing with the hull.
 */
USTRUCT()
struct FHALMagnetBeamSettings
{
	GENERATED_BODY()

	/** Strands in the bundle; one ribbon each. Three reads as a bundle without crowding the screen. */
	UPROPERTY(EditDefaultsOnly, Category="Beam", meta=(ClampMin="1", ClampMax="6"))
	int32 StrandCount = 3;

	/** Samples per strand. The curve is smooth, so it needs far fewer points than a jagged arc. */
	UPROPERTY(EditDefaultsOnly, Category="Beam", meta=(ClampMin="5", ClampMax="41"))
	int32 PointsPerStrand = 21;

	/**
	 * Apex rise above the straight roof-to-ball chord. Below ~80 the arc hugs the roof and
	 * wastes the vertical space it exists for; above ~250 it starts covering the arena behind
	 * the vehicle and tangles with other holders' beams.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Beam", meta=(Units="cm", ClampMin="20", ClampMax="400"))
	float ApexHeightCm = 170.f;

	/** Helix revolutions along the arc. Zero gives parallel bows, which read as rope, not magnetism. */
	UPROPERTY(EditDefaultsOnly, Category="Beam", meta=(ClampMin="0", ClampMax="4"))
	float HelixTurns = 1.2f;

	/** Helix radius at mid-arc. Pinched to nothing at both ends so the strands converge on the anchors. */
	UPROPERTY(EditDefaultsOnly, Category="Beam", meta=(Units="cm", ClampMin="0", ClampMax="120"))
	float SpreadCm = 26.f;

	UPROPERTY(EditDefaultsOnly, Category="Beam", meta=(Units="cm", ClampMin="0.1", ClampMax="40"))
	float WidthCm = 5.f;

	/** Fraction of each strand that tapers at each end, so the beam lands instead of being cut off. */
	UPROPERTY(EditDefaultsOnly, Category="Beam", meta=(ClampMin="0", ClampMax="0.45"))
	float EndTaperFraction = 0.18f;
};

namespace HALMagnetBeamGeometry
{
	/** Points per ribbon for these settings; Niagara needs this to group particles into ribbons. */
	HAL_FUTURE_CREATION_API int32 PointsPerRibbon(const FHALMagnetBeamSettings& Settings);

	/** Ribbon count per rebuild. Stays constant so the asset's spawn count can be fixed. */
	HAL_FUTURE_CREATION_API int32 RibbonCount(const FHALMagnetBeamSettings& Settings);

	/**
	 * Builds every strand into one flat list; ribbon index is PointIndex / PointsPerRibbon.
	 * Start and End are the roof and ball anchors, Up is the apex direction.
	 *
	 * Up is passed in rather than derived from Start and End because it must be world up:
	 * taking it from the vehicle's own axes would fire the beam into the ground whenever the
	 * car is on its side or inverted. Phase advances the helix; it is the caller's clock, so
	 * the geometry stays a pure function of its inputs and is testable without a world.
	 */
	HAL_FUTURE_CREATION_API void Build(const FVector& Start, const FVector& End, const FVector& Up,
		const FHALMagnetBeamSettings& Settings, float Phase,
		TArray<FVector>& OutPositions, TArray<float>& OutWidths);
}
