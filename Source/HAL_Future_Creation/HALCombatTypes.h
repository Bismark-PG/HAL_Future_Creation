#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "VehicleHitContext.h"
#include "VehicleKnockbackTypes.h"
#include "HALImpactTypes.h"
#include "HALCombatTypes.generated.h"

class UWorld;
class APlayerState;

/** Old saved assets stay explicit fixed-damage sources until the team migrates them. */
UENUM(BlueprintType)
enum class EHALCombatConfigVersion : uint8 { LegacyFixedDamage = 0, AttackSnapshotV1 = 1 };

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FVehicleCombatConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Migration")
	EHALCombatConfigVersion Version = EHALCombatConfigVersion::LegacyFixedDamage;
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0"))
	float AttackPower = 25.0f;
	/** Additive action multiplier delta, not a second multiplicative multiplier. */
	UPROPERTY(EditAnywhere, Category = "Combat")
	float OutputMultiplierDelta = 0.0f;
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0"))
	float DamageTakenMultiplier = 1.0f;
	bool Validate(FString& Error) const;
};

UENUM(BlueprintType)
enum class EHALDamageMode : uint8 { SourceFixedDamage, VehicleAttackScaled };

/** A lightweight value snapshot; no ASC on balls or gimmicks, no live source lookup at impact. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALAttackSourceSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FGuid AttackId;
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	EHALDamageMode DamageMode = EHALDamageMode::SourceFixedDamage;
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	int32 InstigatorPlayerId = INDEX_NONE;
	UPROPERTY()
	TWeakObjectPtr<AActor> SourceActor;
	UPROPERTY()
	TWeakObjectPtr<AActor> InstigatorActor;
	UPROPERTY()
	TWeakObjectPtr<APlayerState> InstigatorPlayerState;
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FGameplayTag SourceTag;
	/** Reserved metadata for later status effects; 10-C does not apply effects from these tags. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FGameplayTagContainer EffectTags;
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float CapturedAttackPower = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float ActionMultiplier = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float MultiplierDeltaSum = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float FixedDamage = 0.0f;
	/** Source/action policy. Ordinary balls set false; future self-damage actions may allow it. */
	UPROPERTY()
	bool bCanHitInstigator = true;
	/** Captured with the attack, never re-read from a live ball/config at impact. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FHALAuthoredImpactConfig Impact;
	bool CalculateRawDamage(float& OutDamage) const;
};

enum class EHALHitRejectReason : uint8
{
	None, InvalidAction, ClosedAction, NonAuthority, InvalidTarget,
	ExcludedInstigator, TargetNotReady, InvalidDamage, EffectUnavailable, Duplicate, CapacityExceeded
};

enum class EHALImpactOutcome : uint8 { NotRequested, NoPhysicsBody, InvalidMeasurement, NoDirection, RulesUnavailable, Applied, Suppressed, TargetNotReady };

/** Server-generated request. Physics data is optional and never required by the damage branch. */
struct FHALCombatHitRequest
{
	FGuid AttackId;
	uint32 PulseSequence = 1;
	FVehicleHitContext Hit;
	bool bApplyDamage = true;
	bool bApplyImpact = false;
	float KnockbackStrengthMultiplier = 1.0f;
	FHALImpactProposal Impact;
};

struct FHALCombatHitResult
{
	bool bHitAccepted = false;
	EHALHitRejectReason RejectReason = EHALHitRejectReason::None;
	FGuid AttackId;
	uint32 PulseSequence = 0;
	int32 InstigatorPlayerId = INDEX_NONE;
	float RawDamage = 0.0f;
	float ResolvedDamage = 0.0f;
	float ActualHPRemoved = 0.0f;
	bool bDamageImmune = false;
	bool bImpactApplied = false;
	/** Accepted rule response, before physics integration; excludes natural Chaos impulse. */
	FVector RuleDeltaVelocity = FVector::ZeroVector;
	float RuleMinimumUpwardSpeed = 0;
	bool bSupportSupplementSuppressed = false;
	/** GT contact diagnostics; command classification, not an integrated physics sample. */
	FName SupportSourceReason;
	float NaturalNormalDeltaSpeed = 0;
	float RelativeNormalSpeedAfter = 0;
	EHALImpactOutcome ImpactOutcome = EHALImpactOutcome::NotRequested;
	float ImpactSpeed = 0.0f;
	EVehicleKnockbackTier KnockbackTier = EVehicleKnockbackTier::Light;
	FHALImpactSummary ImpactSummary;
};

/** Source-owned action lifetime. No global cache, timeout reopening or tick. */
class HAL_FUTURE_CREATION_API FHALCombatAction
{
public:
	FHALCombatAction() = default;
	FHALCombatAction(const FHALCombatAction&) = delete;
	FHALCombatAction& operator=(const FHALCombatAction&) = delete;
	bool Begin(AActor& AuthorityOwner, const FHALAttackSourceSnapshot& Source);
	bool AdvancePulse(uint32 NewSequence);
	void Close();
	const FHALAttackSourceSnapshot& GetSnapshot() const { return Snapshot; }
	uint32 GetPulseSequence() const { return PulseSequence; }
	bool IsActive() const { return bActive; }
private:
	friend struct FCombatResolver;
	EHALHitRejectReason CheckRequest(const FHALCombatHitRequest& Request) const;
	void ConsumeTarget(AActor* Target);
	static constexpr int32 MaximumTargetsPerPulse = 256;
	FHALAttackSourceSnapshot Snapshot;
	TWeakObjectPtr<UWorld> World;
	TSet<TWeakObjectPtr<AActor>> AcceptedTargets;
	uint32 PulseSequence = 0;
	bool bActive = false;
};
