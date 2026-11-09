#pragma once

#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "GameplayEffect.h"
#include "HALImpactTypes.h"
#include "HALBattleTopAbility.generated.h"

/** Battle Top gameplay and skill-specific visual tuning. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALBattleTopConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	static constexpr float DefaultSpinRate = 900.f;
	UPROPERTY(EditAnywhere, Category="Timing", meta=(Units="s", ClampMin="0.1", ClampMax="30")) float DurationSeconds = 5.f;
	UPROPERTY(EditAnywhere, Category="Timing", meta=(Units="s", ClampMin="0", ClampMax="120")) float CooldownSeconds = 20.f;
	UPROPERTY(EditAnywhere, Category="Contact", meta=(ClampMin="0", ClampMax="100")) float ContactDamage = 5.f;
	UPROPERTY(EditAnywhere, Category="Contact") EHALImpactTier ContactTier = EHALImpactTier::Medium;
	UPROPERTY(EditAnywhere, Category="Contact") EHALImpactPushPreset ContactPush = EHALImpactPushPreset::Long;
	UPROPERTY(EditAnywhere, Category="Wall", meta=(ClampMin="0", ClampMax="2")) float WallReboundScale = 1.15f;
	UPROPERTY(EditAnywhere, Category="Wall", meta=(Units="cm/s", ClampMin="1", ClampMax="6000")) float WallMinimumIncomingSpeed = 250.f;
	UPROPERTY(EditAnywhere, Category="Wall", meta=(Units="cm/s", ClampMin="1", ClampMax="6000")) float WallMaximumReboundSpeed = 2400.f;
	UPROPERTY(EditAnywhere, Category="Wall", meta=(Units="s", ClampMin="0.05", ClampMax="1")) float WallContactRearmSeconds = .15f;
	UPROPERTY(EditAnywhere, Category="Ball", meta=(Units="cm/s", ClampMin="1", ClampMax="12000")) float HeldBallSpeed = 4000.f;
	UPROPERTY(EditAnywhere, Category="Ball", meta=(Units="cm/s", ClampMin="1", ClampMax="12000")) float FreeBallSpeed = 3800.f;
	UPROPERTY(EditAnywhere, Category="Ball", meta=(Units="cm/s", ClampMin="1", ClampMax="12000")) float MaximumBallSpeed = 6000.f;
	/** Cosmetic angular speed. Snapshotted at activation for owners and observers. */
	UPROPERTY(EditAnywhere, Category="Presentation", meta=(Units="deg/s", ClampMin="90", ClampMax="2160"))
	float SpinRate = DefaultSpinRate;
	float ResolveSpinRate() const { return FMath::IsFinite(SpinRate) ? FMath::Clamp(SpinRate, 90.f, 2160.f) : DefaultSpinRate; }
	bool Validate(FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

UCLASS()
class UHALBattleTopEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UHALBattleTopEffect();
};

UCLASS()
class UHALBattleTopCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UHALBattleTopCooldownEffect();
};

UCLASS()
class HAL_FUTURE_CREATION_API UHALBattleTopAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UHALBattleTopAbility();
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
protected:
	UPROPERTY(EditDefaultsOnly, Category="Battle Top") TObjectPtr<UHALBattleTopConfig> SkillConfig;
	virtual void ApplyCooldown(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) const override;
private:
	UHALBattleTopConfig* GetSkillConfig() const;
	mutable FActiveGameplayEffectHandle CommittedCooldown;
};
