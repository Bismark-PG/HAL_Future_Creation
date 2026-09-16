// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VehicleKnockbackSettings.generated.h"

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

/** Project-wide impact thresholds and tier outcomes shared by every standard ball. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Vehicle Knockback"))
class HAL_FUTURE_CREATION_API UVehicleKnockbackSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UVehicleKnockbackSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	EVehicleKnockbackTier SelectTier(float ImpactSpeed) const;
	const FVehicleKnockbackTierDefinition& GetTierDefinition(EVehicleKnockbackTier Tier) const;

	/** Medium begins at this equivalent collision speed. */
	UPROPERTY(Config, EditAnywhere, Category = "Thresholds", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MediumImpactSpeedThreshold = 1000.0f;

	/** Heavy begins here; runtime clamps it to at least the Medium threshold. */
	UPROPERTY(Config, EditAnywhere, Category = "Thresholds", meta = (ClampMin = "0.0", Units = "cm/s"))
	float HeavyImpactSpeedThreshold = 2600.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Tier Outcomes")
	FVehicleKnockbackTierDefinition LightTier;

	UPROPERTY(Config, EditAnywhere, Category = "Tier Outcomes")
	FVehicleKnockbackTierDefinition MediumTier;

	UPROPERTY(Config, EditAnywhere, Category = "Tier Outcomes")
	FVehicleKnockbackTierDefinition HeavyTier;

	/** Hard cap for tier-authored Pitch/Roll angular speed. */
	UPROPERTY(Config, EditAnywhere, Category = "Stability", meta = (ClampMin = "0.0", Units = "rad/s"))
	float MaxAirborneAngularSpeed = 8.0f;

	/** Extra distance below the target bounds used to recognize a grounded Light hit. */
	UPROPERTY(Config, EditAnywhere, Category = "Stability", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundProbeExtraDistance = 20.0f;
};
