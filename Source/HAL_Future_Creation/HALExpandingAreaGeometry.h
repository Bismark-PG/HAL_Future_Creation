#pragma once

#include "CoreMinimal.h"

class AActor;

/** World-XY cylinder math shared by expanding fields; actor scale/rotation never changes rules. */
namespace HALExpandingArea
{
	inline float RadiusAt(double Age, float ExpansionSeconds, float MaximumRadius)
	{
		return ExpansionSeconds <= 0 ? MaximumRadius
			: FMath::Lerp(1.f, MaximumRadius, float(FMath::Clamp(Age / ExpansionSeconds, 0., 1.)));
	}
	inline bool Contains(const FVector& Center, const FVector& Point, float Radius, float HalfHeight)
	{
		const FVector Offset = Point - Center;
		return FMath::Abs(Offset.Z) <= HalfHeight && Offset.SizeSquared2D() <= FMath::Square(double(Radius));
	}
	/** Debug presentation only: projects onto supporting ground, ignoring the source and instigator. */
	HAL_FUTURE_CREATION_API void DrawGroundDebug(const AActor& Area, float Radius, float HalfHeight, FColor Color);
}
