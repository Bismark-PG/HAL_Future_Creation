#pragma once
#include "CoreMinimal.h"
#include "VehicleKnockbackTypes.generated.h"

UENUM(BlueprintType)
enum class EVehicleKnockbackTier : uint8
{
	Light,
	Medium,
	Heavy
};

/** Deterministic physical outcome selected by a global impact-speed tier. */
USTRUCT(BlueprintType)
struct FVehicleKnockbackTierDefinition
{
	GENERATED_BODY()

	/** Mass-independent horizontal velocity change along the push direction. */
	UPROPERTY(EditAnywhere, Category = "Knockback", meta = (ClampMin = "0.0", Units = "cm/s"))
	float HorizontalDeltaSpeed = 0.0f;

	/** Minimum upward speed after the hit; zero does not deliberately launch. */
	UPROPERTY(EditAnywhere, Category = "Knockback", meta = (ClampMin = "0.0", Units = "cm/s"))
	float TargetVerticalSpeed = 0.0f;

	/** Approximate airborne flips in an uninterrupted ballistic arc. */
	UPROPERTY(EditAnywhere, Category = "Knockback", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float AirborneFlipTurns = 0.0f;
};

