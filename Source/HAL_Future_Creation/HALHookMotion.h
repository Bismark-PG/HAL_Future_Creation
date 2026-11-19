#pragma once
#include "CoreMinimal.h"
#include "HALHookMotion.generated.h"

/** Pure authority command in movement history. No actor references or gameplay callbacks on PT. */
USTRUCT()
struct HAL_FUTURE_CREATION_API FHALHookMotion
{
	GENERATED_BODY()
	UPROPERTY() FGuid Activation;
	UPROPERTY() FVector Anchor = FVector::ZeroVector;
	UPROPERTY() FVector ThrowVelocity = FVector::ZeroVector;
	UPROPERTY() float Stiffness = 80;
	UPROPERTY() float Damping = 18;
	UPROPERTY() float MaximumAcceleration = 8000;
	UPROPERTY() float DurationSeconds = 0;
	UPROPERTY() bool bCapture = false;
	UPROPERTY() bool bThrow = false;
	UPROPERTY() bool bSwing = false;
	UPROPERTY() bool bPreserveHorizontal = false;
	UPROPERTY() FVector Pivot = FVector::ZeroVector;
	UPROPERTY() float SwingRadians = 0;
	UPROPERTY() float AccelerationPower = 2;
	UPROPERTY() float FixedDelta = 1.f/60;
	UPROPERTY() int32 StartFrame = INDEX_NONE;
	UPROPERTY() int32 EndFrame = INDEX_NONE;
	bool operator==(const FHALHookMotion& Other) const = default;
	bool IsValid(bool bStamped) const;
	void Stamp(const FHALHookMotion* Previous, int32 Frame, float FixedDelta);
	void Serialize(FArchive& Ar);
	bool IsHolding(int32 Frame) const;
	FVector Acceleration(const FVector& Position, const FVector& Velocity, int32 Frame) const;
	FVector ReleaseDelta(const FVector& Velocity, int32 Frame) const;
	bool IsSwinging(int32 Frame) const;
	FVector SwingPosition(int32 Frame) const;
	FVector SwingDelta(const FVector& RootPosition,const FVector& RootVelocity,int32 Frame,float Delta) const;
};
