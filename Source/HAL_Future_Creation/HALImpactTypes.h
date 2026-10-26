#pragma once

#include "CoreMinimal.h"
#include "VehicleKnockbackTypes.h"
#include "HALImpactTypes.generated.h"

/** Separate from the saved legacy enum: Light/Medium/Heavy retain their old indices. */
UENUM(BlueprintType)
enum class EHALImpactTier : uint8 { None = 0, Light = 1, Medium = 2, Heavy = 3 };

UENUM(BlueprintType)
enum class EHALImpactVersion : uint8 { Legacy = 0, TieredV1 = 1 };

/** Horizontal response only. Zero keeps every existing saved source on the original path. */
UENUM(BlueprintType)
enum class EHALImpactPushPreset : uint8 { Standard = 0, Short = 1, Long = 2, ExtraLong = 3 };

/** Shared authored-attack multipliers; Standard is always exactly 1, not a tunable migration. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALImpactPushRules
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Horizontal Push", meta = (ClampMin = "0", ClampMax = "1"))
	float ShortMultiplier = .5f;
	UPROPERTY(EditAnywhere, Category = "Horizontal Push", meta = (ClampMin = "1", ClampMax = "3"))
	float LongMultiplier = 1.5f;
	UPROPERTY(EditAnywhere, Category = "Horizontal Push", meta = (ClampMin = "1", ClampMax = "3"))
	float ExtraLongMultiplier = 2;
	bool Validate(FString& Error) const;
	float GetMultiplier(EHALImpactPushPreset Preset) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALVehicleImpactConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Migration")
	EHALImpactVersion Version = EHALImpactVersion::Legacy;
	UPROPERTY(EditAnywhere, Category = "Impact")
	EHALImpactTier CollisionTierCap = EHALImpactTier::Heavy;
	UPROPERTY(EditAnywhere, Category = "Impact")
	EHALImpactTier MinimumDropTier = EHALImpactTier::Medium;
	UPROPERTY(EditAnywhere, Category = "Impact")
	bool bDropOnImpact = true;
	/** Discrete incoming-tier reduction. GE changes the corresponding GAS attribute. */
	UPROPERTY(EditAnywhere, Category = "Impact", meta = (ClampMin = "0", ClampMax = "3"))
	int32 TierReduction = 0;
	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALAuthoredImpactConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Migration")
	EHALImpactVersion Version = EHALImpactVersion::Legacy;
	UPROPERTY(EditAnywhere, Category = "Impact")
	EHALImpactTier Tier = EHALImpactTier::Medium;
	/** Captured with the attack. Does not change tier, upward launch, flips or damage. */
	UPROPERTY(EditAnywhere, Category = "Impact")
	EHALImpactPushPreset PushPreset = EHALImpactPushPreset::Standard;
	bool Validate(FString& Error) const;
};

/** Shared physical-contact calibration, not per-content attack strength. Explicitly migrated. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALCollisionImpactRules
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Migration")
	EHALImpactVersion Version = EHALImpactVersion::Legacy;
	/** Normal impulse / receiving mass, in cm/s. Below Light is ordinary support/rubbing. */
	UPROPERTY(EditAnywhere, Category = "Calibration", meta = (ClampMin = "1", Units = "cm/s"))
	float LightThreshold = 50.0f;
	UPROPERTY(EditAnywhere, Category = "Calibration", meta = (ClampMin = "1", Units = "cm/s"))
	float MediumThreshold = 125.0f;
	UPROPERTY(EditAnywhere, Category = "Calibration", meta = (ClampMin = "1", Units = "cm/s"))
	float HeavyThreshold = 300.0f;
	UPROPERTY(EditAnywhere, Category = "Defaults")
	EHALImpactTier EnvironmentTierCap = EHALImpactTier::Heavy;
	UPROPERTY(EditAnywhere, Category = "Defaults")
	EHALImpactTier FreeBallTierCap = EHALImpactTier::Light;
	/** Contact must stop reporting for this long before another collision episode can react. */
	UPROPERTY(EditAnywhere, Category = "Contact", meta = (ClampMin = "0.05", Units = "s"))
	float ContactRearmSeconds = 0.2f;
	/** Cap additional physical-contact bounce relative to reconstructed incoming normal speed. */
	UPROPERTY(EditAnywhere, Category = "Stability", meta = (ClampMin = "0", ClampMax = "0.5"))
	float MaximumRuleRestitution = 0.15f;
	bool Validate(FString& Error) const;
	EHALImpactTier SelectTier(float NormalDeltaSpeed, EHALImpactTier Cap) const;
};

enum class EHALImpactMode : uint8 { LegacyMeasurement, ExplicitTier, PhysicalContact };

struct FHALImpactProposal
{
	EHALImpactMode Mode = EHALImpactMode::LegacyMeasurement;
	EHALImpactTier Tier = EHALImpactTier::None;
	EHALImpactPushPreset PushPreset = EHALImpactPushPreset::Standard;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALImpactSummary
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	FGuid EventId;
	UPROPERTY()
	uint32 PulseSequence = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	EHALImpactTier SourceTier = EHALImpactTier::None;
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	EHALImpactTier EffectiveTier = EHALImpactTier::None;
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	EHALImpactPushPreset PushPreset = EHALImpactPushPreset::Standard;
	/** Target horizontal velocity change, before subtracting natural response; not actual distance. */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	float DesiredHorizontalDeltaSpeed = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	bool bImmune = false;
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	bool bBallDropped = false;
	/** Frame observed during authority resolution, not the later engine-assigned application frame. */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	int32 ServerPhysicsFrame = INDEX_NONE;
};

namespace HALImpact
{
	HAL_FUTURE_CREATION_API bool IsValidTier(EHALImpactTier Tier);
	HAL_FUTURE_CREATION_API bool IsValidPushPreset(EHALImpactPushPreset Preset);
	HAL_FUTURE_CREATION_API EHALImpactTier Reduce(EHALImpactTier Source, int32 Reduction, bool bImmune);
	HAL_FUTURE_CREATION_API EVehicleKnockbackTier ToLegacy(EHALImpactTier Tier);
	/** Supplemental normal delta, bounded by an energy-losing relative bounce policy. */
	HAL_FUTURE_CREATION_API float PhysicalSupplement(float DesiredDelta, float NaturalDelta,
		float CurrentRelativeNormalSpeed, float OtherToSelfInverseMassRatio, float MaximumRestitution);
}
