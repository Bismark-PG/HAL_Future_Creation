#pragma once
#include "CoreMinimal.h"
#include "HALSideShiftMotion.generated.h"

/** Immutable authority-authored side-step. Direction never follows subsequent chassis yaw. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALSideShiftMotion
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Translation", meta=(Units="cm", ClampMin="10", ClampMax="2000")) float Distance = 300;
	UPROPERTY(EditAnywhere, Category="Translation", meta=(Units="s", ClampMin="0.1", ClampMax="5")) float DurationSeconds = .8f;
	UPROPERTY(EditAnywhere, Category="Translation", meta=(Units="cm/s^2", ClampMin="100", ClampMax="30000")) float MaximumAcceleration = 6000;
	UPROPERTY(EditAnywhere, Category="Drift", meta=(Units="s", ClampMin="0.05", ClampMax="5")) float DriftSeconds = 1;
	UPROPERTY(EditAnywhere, Category="Drift", meta=(Units="deg/s", ClampMin="30", ClampMax="720")) float DriftYawRate = 240;
	UPROPERTY(EditAnywhere, Category="Drift", meta=(ClampMin="1", ClampMax="30", ToolTip="Yaw velocity response, in inverse seconds; steering release also uses this response.")) float DriftResponse = 10;
	UPROPERTY(EditAnywhere, Category="Drift", meta=(ClampMin="1", ClampMax="100", ToolTip="Maximum skill steering angular acceleration, radians per second squared.")) float MaximumYawAcceleration = 40;
	UPROPERTY() FGuid Activation;
	UPROPERTY() bool bActive = false;
	UPROPERTY() FVector Direction = FVector::ZeroVector;
	UPROPERTY() float InitialLateralSpeed = 0;
	UPROPERTY() int32 StartFrame = INDEX_NONE;
	UPROPERTY() int32 EndFrame = INDEX_NONE;
	UPROPERTY() int32 ShiftFrames = 0;
	UPROPERTY() int32 DriftFrames = 0;
	bool IsValidTuning() const;
	bool IsValid(bool bStamped) const;
	bool IsSliding(int32 Frame) const;
	bool IsDrifting(int32 Frame) const;
	/** Average velocity over this exact physics interval; smooth position profile integrates to Distance. */
	float StepSpeed(int32 Frame, float FixedDelta) const;
	float YawAcceleration(float Steering, float CurrentYawRate) const;
	void Stamp(const FHALSideShiftMotion* Previous, int32 Frame, float FixedDelta);
	void Serialize(FArchive& Ar);
	bool operator==(const FHALSideShiftMotion& Other) const = default;
};
