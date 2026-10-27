#pragma once

#include "CoreMinimal.h"
#include "HALMovementAreaTypes.generated.h"

UENUM(BlueprintType)
enum class EHALMovementAreaKind : uint8
{
	Slow,
	Slippery
};

/** Authoring data only. Vehicle DA values are never modified by an area. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALMovementAreaConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Movement Area") EHALMovementAreaKind Kind = EHALMovementAreaKind::Slow;
	/** Regional multiplier before other ordinary buff deltas are added. Lower is stronger. */
	UPROPERTY(EditAnywhere, Category="Movement Area", meta=(ClampMin="0", ClampMax="1", EditCondition="Kind == EHALMovementAreaKind::Slow"))
	float AccelerationMultiplier = 0.5f;
	UPROPERTY(EditAnywhere, Category="Movement Area", meta=(ClampMin="0", ClampMax="1", EditCondition="Kind == EHALMovementAreaKind::Slow"))
	float SpeedLimitMultiplier = 0.5f;
	/** Scales both lateral grip rate and maximum lateral grip acceleration, including handbrake grip. */
	UPROPERTY(EditAnywhere, Category="Movement Area", meta=(ClampMin="0", ClampMax="1", EditCondition="Kind == EHALMovementAreaKind::Slippery"))
	float LateralGripMultiplier = 0.2f;
	/** Parameter recovery in game seconds, not a promise that physical velocity has settled. */
	UPROPERTY(EditAnywhere, Category="Movement Area", meta=(ClampMin="0", ClampMax="10", Units="s"))
	float RecoverySeconds = 0.4f;
	bool IsValid() const;
	FVector GetScales() const;
};

/** Per-parameter winners and their recovery policies. Equal-strength ties use the longer recovery. */
struct HAL_FUTURE_CREATION_API FHALMovementAreaSelection
{
	FVector Scales = FVector::OneVector;
	FVector RecoverySeconds = FVector::ZeroVector;
	void Include(const FHALMovementAreaConfig& Config);
};
