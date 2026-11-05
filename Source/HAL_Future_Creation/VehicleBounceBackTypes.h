#pragma once
#include "CoreMinimal.h"
#include "VehicleBounceBackTypes.generated.h"

/** Immutable GT observation of the consumed Internal pose, never a render-interpolated transform. */
struct FVehicleContactObservation
{
	bool bValid=false;
	FVector Position=FVector::ZeroVector, Velocity=FVector::ZeroVector;
	FQuat Rotation=FQuat::Identity;
};

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
	static float DistanceFromEntry(float BaseDistance,const FVector& Current,const FVector& Entry,const FVector& Direction)
	{
		return FMath::Min(1000.f,BaseDistance+FMath::Max(0.f,float(FVector::DotProduct(Current-Entry,-Direction))));
	}
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
