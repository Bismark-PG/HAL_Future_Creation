#include "HALMagnetBeamGeometry.h"

namespace HALMagnetBeamGeometry
{
	int32 PointsPerRibbon(const FHALMagnetBeamSettings& Settings)
	{
		return FMath::Clamp(Settings.PointsPerStrand, 5, 41);
	}

	int32 RibbonCount(const FHALMagnetBeamSettings& Settings)
	{
		return FMath::Clamp(Settings.StrandCount, 1, 6);
	}

	void Build(const FVector& Start, const FVector& End, const FVector& Up,
		const FHALMagnetBeamSettings& Settings, float Phase,
		TArray<FVector>& OutPositions, TArray<float>& OutWidths)
	{
		const int32 Points = PointsPerRibbon(Settings);
		const int32 Strands = RibbonCount(Settings);
		OutPositions.SetNumUninitialized(Points * Strands);
		OutWidths.SetNumUninitialized(Points * Strands);

		FVector UpDir = Up.GetSafeNormal();
		if (UpDir.IsNearlyZero()) { UpDir = FVector::UpVector; }
		const FVector Chord = End - Start;
		// A quadratic Bezier reaches half of its control point's offset at the midpoint,
		// so the control rises twice the apex to put the apex exactly where it was authored.
		const FVector Control = Start + Chord * 0.5f + UpDir * (Settings.ApexHeightCm * 2.f);

		// Helix basis perpendicular to the chord. When the ball ends up directly above or
		// below the roof the cross product collapses, so fall back to a fixed perpendicular
		// rather than emitting NaNs into the position array.
		FVector Axis = Chord.GetSafeNormal();
		if (Axis.IsNearlyZero()) { Axis = UpDir; }
		FVector Side = FVector::CrossProduct(Axis, UpDir);
		if (!Side.Normalize()) { Side = FVector::CrossProduct(Axis, FVector::ForwardVector).GetSafeNormal(); }
		if (Side.IsNearlyZero()) { Side = FVector::RightVector; }
		const FVector Normal = FVector::CrossProduct(Side, Axis).GetSafeNormal();

		const float Taper = FMath::Max(KINDA_SMALL_NUMBER, Settings.EndTaperFraction);
		const float Step = 1.f / float(Points - 1);
		for (int32 Strand = 0; Strand < Strands; ++Strand)
		{
			const float StrandPhase = Phase + float(Strand) * UE_TWO_PI / float(Strands);
			for (int32 Index = 0; Index < Points; ++Index)
			{
				const float U = float(Index) * Step;
				const float IU = 1.f - U;
				const FVector Base = IU * IU * Start + 2.f * IU * U * Control + U * U * End;
				// Pinched at both ends so every strand converges on the roof and the ball
				// instead of ending in a ring the player has to interpret.
				const float Envelope = FMath::Sin(UE_PI * U);
				const float Angle = StrandPhase + Settings.HelixTurns * UE_TWO_PI * U;
				const FVector Offset = (Side * FMath::Cos(Angle) + Normal * FMath::Sin(Angle))
					* (Settings.SpreadCm * Envelope);
				const int32 Flat = Strand * Points + Index;
				OutPositions[Flat] = Base + Offset;
				OutWidths[Flat] = Settings.WidthCm
					* FMath::SmoothStep(0.f, Taper, U) * FMath::SmoothStep(0.f, Taper, 1.f - U);
			}
		}
	}
}
