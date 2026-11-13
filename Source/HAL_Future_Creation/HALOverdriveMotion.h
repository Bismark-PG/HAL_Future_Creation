#pragma once

#include "CoreMinimal.h"
#include "HALOverdriveMotion.generated.h"

/** Immutable, server-frame motion profile. No GAS objects are read during physics replay. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALOverdriveMotion
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Timing", meta=(Units="s", ClampMin="0.1", ClampMax="30")) float DurationSeconds = 5.f;
	UPROPERTY(EditAnywhere, Category="Timing", meta=(Units="s", ClampMin="0", ClampMax="30")) float RampSeconds = 2.f;
	UPROPERTY(EditAnywhere, Category="Timing", meta=(Units="s", ClampMin="0.05", ClampMax="5")) float RecoverySeconds = .5f;
	UPROPERTY(EditAnywhere, Category="Drive", meta=(ClampMin="0", ClampMax="3")) float InitialAccelerationScale = .4f;
	UPROPERTY(EditAnywhere, Category="Drive", meta=(ClampMin="0", ClampMax="3")) float FinalAccelerationScale = 1.5f;
	UPROPERTY(EditAnywhere, Category="Drive", meta=(ClampMin="1", ClampMax="3")) float FinalSpeedScale = 1.7f;
	UPROPERTY(EditAnywhere, Category="Steering", meta=(ClampMin="0", ClampMax="1")) float SteeringScale = .35f;
	UPROPERTY() FGuid Activation;
	UPROPERTY() bool bActive = false;
	UPROPERTY() bool bStunned = false;
	UPROPERTY() int32 StartFrame = INDEX_NONE;
	UPROPERTY() int32 EndFrame = INDEX_NONE;
	UPROPERTY() int32 RampFrames = 0;
	UPROPERTY() int32 RecoveryFrames = 0;
	UPROPERTY() int32 StunFrame = INDEX_NONE;
	bool IsValidTuning() const;
	bool IsValidState(bool bStamped) const;
	bool IsDriving(int32 Frame) const;
	bool IsRecovering(int32 Frame) const;
	float RampAlpha(int32 Frame) const;
	float SteeringAt(int32 Frame) const;
	void Stamp(const FHALOverdriveMotion* Previous, int32 Frame, float FixedDelta);
	void Serialize(FArchive& Ar);
	bool operator==(const FHALOverdriveMotion& Other) const = default;
};
