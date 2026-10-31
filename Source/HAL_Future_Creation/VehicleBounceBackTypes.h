#pragma once
#include "CoreMinimal.h"
#include "VehicleBounceBackTypes.generated.h"

/** Physics-owned, rewindable horizontal bounce. Never changes Z or angular velocity. */
USTRUCT()
struct HAL_FUTURE_CREATION_API FVehicleBounceBackState
{
	GENERATED_BODY()
	UPROPERTY() uint32 Sequence = 0;
	UPROPERTY() FVector Direction = FVector::ZeroVector;
	UPROPERTY() float Distance = 0;
	UPROPERTY() float Duration = 0;
	UPROPERTY() float Elapsed = 0;
	UPROPERTY() bool bActive = false;
	bool IsValid() const
	{
		return !Direction.ContainsNaN() && FMath::IsFinite(Distance) && Distance >= 0 && Distance <= 1000
			&& FMath::IsFinite(Duration) && Duration >= 0 && Duration <= 2
			&& FMath::IsFinite(Elapsed) && Elapsed >= 0 && Elapsed <= Duration + .001f
			&& (!bActive || (Sequence && Distance > 0 && Duration >= .05f
				&& FMath::Abs(Direction.Z) < .001f && FMath::IsNearlyEqual(Direction.SizeSquared(),1.,.001)));
	}
	bool Equals(const FVehicleBounceBackState& B) const
	{
		return Sequence==B.Sequence && Direction.Equals(B.Direction,.001) && Distance==B.Distance
			&& Duration==B.Duration && FMath::IsNearlyEqual(Elapsed,B.Elapsed,.001f) && bActive==B.bActive;
	}
	void Serialize(FArchive& Ar)
	{
		Ar << Sequence << Direction << Distance << Duration << Elapsed << bActive;
		if (!IsValid()) { Ar.SetError(); }
	}
	/** Average of a linearly decaying speed over this step; integral is Distance. */
	FVector StepVelocity(float Dt)
	{
		const float A=Elapsed;
		Elapsed=FMath::Min(Duration,Elapsed+Dt);
		const float Integrated=Distance*((2*Elapsed/Duration-FMath::Square(Elapsed/Duration))
			-(2*A/Duration-FMath::Square(A/Duration)));
		return Direction*(Integrated/Dt);
	}
};
