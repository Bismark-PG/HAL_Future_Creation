#pragma once

#include "CoreMinimal.h"
#include "VehicleInputCmd.h"

/** Narrow, deterministic calculations shared by grounded wall escape and its tests. */
namespace VehicleWallEscape
{

inline bool IsEligible(const FVehicleInputCmd& Input, float PlanarSpeed, bool bWasActive,
	float EnterSpeed, float ExitSpeed, float MinimumSteering)
{
	const float SpeedLimit = bWasActive ? FMath::Max(EnterSpeed, ExitSpeed) : EnterSpeed;
	return Input.Throttle > KINDA_SMALL_NUMBER
		&& Input.Brake <= KINDA_SMALL_NUMBER
		&& Input.bHandbrake
		&& FMath::Abs(Input.Steering) >= FMath::Max(MinimumSteering, KINDA_SMALL_NUMBER)
		&& PlanarSpeed <= FMath::Max(0.0f, SpeedLimit);
}

inline float UpdateBlend(float Current, bool bActive, float DeltaTime, float InTime, float OutTime)
{
	const float Duration = bActive ? InTime : OutTime;
	return Duration <= KINDA_SMALL_NUMBER ? (bActive ? 1.0f : 0.0f)
		: FMath::FInterpConstantTo(Current, bActive ? 1.0f : 0.0f, DeltaTime, 1.0f / Duration);
}

/** Normals point away from walls and lie in the driving plane. No added escape push. */
inline FVector ConstrainThrottle(const FVector& Acceleration, TConstArrayView<FVector> Normals,
	float IntoWallScale, float Blend)
{
	const auto IsAllowed = [Normals](const FVector& Candidate)
	{
		for (const FVector& Normal : Normals)
		{
			if (FVector::DotProduct(Candidate, Normal) < -KINDA_SMALL_NUMBER)
			{
				return false;
			}
		}
		return true;
	};

	if (IsAllowed(Acceleration))
	{
		return Acceleration;
	}

	// In a 2D driving plane, the closest point of this cone is on one wall boundary,
	// or at the origin. Checking all boundaries also handles a two-wall corner
	// without sequential projections reintroducing force into the other wall.
	FVector Constrained = FVector::ZeroVector;
	float BestDistanceSquared = Acceleration.SizeSquared();
	for (const FVector& Normal : Normals)
	{
		const FVector Candidate = FVector::VectorPlaneProject(Acceleration, Normal);
		const float DistanceSquared = FVector::DistSquared(Acceleration, Candidate);
		if (DistanceSquared < BestDistanceSquared && IsAllowed(Candidate))
		{
			Constrained = Candidate;
			BestDistanceSquared = DistanceSquared;
		}
	}
	return FMath::Lerp(Acceleration, Constrained,
		FMath::Clamp(Blend, 0.0f, 1.0f) * (1.0f - FMath::Clamp(IntoWallScale, 0.0f, 1.0f)));
}

inline float YawAcceleration(float TargetRate, float CurrentRate, float ResponseRate,
	float MaxAcceleration, float DeltaTime)
{
	const float Gain = FMath::Min(FMath::Max(ResponseRate, 0.0f),
		1.0f / FMath::Max(DeltaTime, SMALL_NUMBER));
	const float Limit = FMath::Max(MaxAcceleration, 0.0f);
	return FMath::Clamp((TargetRate - CurrentRate) * Gain, -Limit, Limit);
}

}
