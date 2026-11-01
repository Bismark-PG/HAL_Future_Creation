#include "HALShockArcGeometry.h"
#include "Math/RandomStream.h"

namespace HALShockArcGeometry
{
	/** A point on the ellipsoid inscribed in the body extent, so endpoints land on the hull. */
	static FVector OnHull(const FVector& Extent, FRandomStream& Stream)
	{
		return Stream.GetUnitVector() * Extent;
	}

	/**
	 * Midpoint displacement: split every segment and kick the new point sideways by an
	 * amount proportional to that segment. Because segments halve each pass, the kick
	 * decays on its own and both original endpoints stay exactly where they were.
	 */
	static void Subdivide(TArray<FVector>& Points, int32 Passes, float Roughness, FRandomStream& Stream)
	{
		TArray<FVector> Next;
		for (int32 Pass = 0; Pass < Passes; ++Pass)
		{
			Next.Reset(Points.Num() * 2);
			for (int32 Index = 0; Index < Points.Num() - 1; ++Index)
			{
				const FVector Start = Points[Index];
				const FVector End = Points[Index + 1];
				const FVector Segment = End - Start;
				FVector Perpendicular = FVector::CrossProduct(Segment, Stream.GetUnitVector());
				if (!Perpendicular.Normalize()) { Perpendicular = FVector::UpVector; }
				Next.Add(Start);
				Next.Add((Start + End) * 0.5f + Perpendicular * Segment.Size() * Roughness * Stream.FRandRange(-1.f, 1.f));
			}
			Next.Add(Points.Last());
			Points = Next;
		}
	}

	/** Lays one ribbon down, tapered at both ends; WidthScale 0 hides an unrolled branch. */
	static void Emit(const TArray<FVector>& Points, const FVector& Extent, const FHALShockArcSettings& Settings,
		float WidthScale, TArray<FVector>& OutPositions, TArray<float>& OutWidths)
	{
		const float Radius = Extent.GetMax();
		const int32 Count = Points.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float T = Count > 1 ? float(Index) / float(Count - 1) : 0.f;
			// Float the arc clear of the paint so it reads as current in the air, not a decal.
			const FVector Lifted = Points[Index] + Points[Index].GetSafeNormal() * Radius * Settings.LiftFraction;
			OutPositions.Add(Lifted);
			OutWidths.Add(Settings.WidthCm * WidthScale * (0.25f + 0.75f * FMath::Sin(PI * T)));
		}
	}

	int32 PointsPerRibbon(const FHALShockArcSettings& Settings)
	{
		return (1 << FMath::Clamp(Settings.Subdivisions, 2, 5)) + 1;
	}

	int32 RibbonCount(const FHALShockArcSettings& Settings)
	{
		// Two slots per arc: the arc itself and its branch slot, rolled or not.
		return FMath::Clamp(Settings.ArcCount, 1, 8) * 2;
	}

	void Build(const FVector& LocalExtent, const FHALShockArcSettings& Settings, uint32 Seed,
		TArray<FVector>& OutPositions, TArray<float>& OutWidths)
	{
		const int32 Arcs = FMath::Clamp(Settings.ArcCount, 1, 8);
		const int32 Passes = FMath::Clamp(Settings.Subdivisions, 2, 5);
		const int32 PerRibbon = PointsPerRibbon(Settings);
		const FVector Extent = LocalExtent.GetAbsMax() > KINDA_SMALL_NUMBER ? LocalExtent : FVector(100.f);
		const float Radius = Extent.GetMax();

		OutPositions.Reset(RibbonCount(Settings) * PerRibbon);
		OutWidths.Reset(RibbonCount(Settings) * PerRibbon);

		// Named seed: FRandomStream Stream(int32(Seed)) parses as a function declaration.
		const int32 SeedValue = static_cast<int32>(Seed);
		FRandomStream Stream(SeedValue);
		TArray<FVector> Points;
		for (int32 Arc = 0; Arc < Arcs; ++Arc)
		{
			const FVector Start = OnHull(Extent, Stream);
			FVector End = OnHull(Extent, Stream);
			// Endpoints too close collapse the arc into a dot; bounce to the opposite side instead.
			if ((End - Start).SizeSquared() < FMath::Square(Radius * 0.6f)) { End = -Start; }

			Points.Reset(); Points.Add(Start); Points.Add(End);
			Subdivide(Points, Passes, Settings.Roughness, Stream);
			Emit(Points, Extent, Settings, 1.f, OutPositions, OutWidths);

			// Branch: leaves the parent partway along and dies early, which is what makes
			// the whole thing read as electricity rather than as a drawn curve.
			const float BranchWidth = Stream.GetFraction() < Settings.BranchChance ? 0.6f : 0.f;
			const int32 Fork = FMath::Clamp(int32(PerRibbon * 0.35f), 1, PerRibbon - 2);
			const FVector ForkPoint = Points[Fork];
			const FVector Away = (ForkPoint - Points[Fork - 1]).GetSafeNormal();
			const FVector Tip = ForkPoint + (Away + Stream.GetUnitVector() * 0.8f).GetSafeNormal()
				* Radius * Settings.BranchLength * 2.f;

			Points.Reset(); Points.Add(ForkPoint); Points.Add(Tip);
			// Same pass count as the parent, so the branch lands on PerRibbon points by itself:
			// padding a short run with duplicates would leave zero-length ribbon segments.
			// A branch covers less distance, so equal passes simply make it finer, as it should be.
			Subdivide(Points, Passes, Settings.Roughness * 1.3f, Stream);
			checkSlow(Points.Num() == PerRibbon);
			Emit(Points, Extent, Settings, BranchWidth, OutPositions, OutWidths);
		}
	}
}
