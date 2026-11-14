#pragma once

#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "GameplayEffect.h"
#include "HALImpactTypes.h"
#include "HALOverdriveMotion.h"
#include "HALBullChargeFXTypes.h"
#include "HALBullChargeAbility.generated.h"

class UCurveFloat;

UCLASS()
class HAL_FUTURE_CREATION_API UHALBullChargeConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Charge") FHALOverdriveMotion Motion;
	UPROPERTY(EditAnywhere, Category="Presentation") FHALBullChargeFXConfig FX;
	UPROPERTY(EditAnywhere, Category="Timing", meta=(Units="s", ClampMin="0", ClampMax="120")) float CooldownSeconds = 20.f;
	UPROPERTY(EditAnywhere, Category="Contact", meta=(Units="s", ClampMin="0.05", ClampMax="10")) float TargetCooldownSeconds = .8f;
	UPROPERTY(EditAnywhere, Category="Contact", meta=(Units="deg", ClampMin="1", ClampMax="89")) float FrontHalfAngle = 45.f;
	UPROPERTY(EditAnywhere, Category="Contact", meta=(Units="cm/s", ClampMin="1", ClampMax="10000")) float MinimumForwardSpeed = 400.f;
	UPROPERTY(EditAnywhere, Category="Contact", meta=(Units="cm/s", ClampMin="1", ClampMax="10000")) float MinimumClosingSpeed = 50.f;
	UPROPERTY(EditAnywhere, Category="Damage", meta=(Units="cm/s", ClampMin="1", ClampMax="12000")) float FullDamageSpeed = 3500.f;
	UPROPERTY(EditAnywhere, Category="Damage", meta=(ClampMin="0", ClampMax="10")) float MinimumDamageMultiplier = .4f;
	UPROPERTY(EditAnywhere, Category="Damage", meta=(ClampMin="0", ClampMax="10")) float MaximumDamageMultiplier = 1.8f;
	/** Optional normalized speed [0,1] to action multiplier. Output is clamped to Maximum Damage Multiplier. */
	UPROPERTY(EditAnywhere, Category="Damage") TObjectPtr<UCurveFloat> DamageMultiplierCurve;
	UPROPERTY(EditAnywhere, Category="Impact") EHALImpactTier ContactTier = EHALImpactTier::Heavy;
	UPROPERTY(EditAnywhere, Category="Impact") EHALImpactPushPreset ContactPush = EHALImpactPushPreset::Long;
	UPROPERTY(EditAnywhere, Category="Ball", meta=(Units="cm/s", ClampMin="1", ClampMax="12000")) float BaseBallSpeed = 3000.f;
	UPROPERTY(EditAnywhere, Category="Ball", meta=(ClampMin="0", ClampMax="2")) float BallInheritedSpeedFraction = .75f;
	UPROPERTY(EditAnywhere, Category="Ball", meta=(Units="cm/s", ClampMin="1", ClampMax="12000")) float MaximumBallSpeed = 6000.f;
	bool Validate(FString& Error) const;
	bool Qualifies(const FVector& Forward, const FVector& ToContact, const FVector& ClosingNormal,
		const FVector& BeforeVelocity, const FVector& OtherBeforeVelocity, float& OutForwardSpeed) const;
	float DamageMultiplier(float ForwardSpeed) const;
	float BallSpeed(float ForwardSpeed) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

UCLASS()
class HAL_FUTURE_CREATION_API UHALBullChargeEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UHALBullChargeEffect();
};
UCLASS()
class HAL_FUTURE_CREATION_API UHALBullChargeCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UHALBullChargeCooldownEffect();
};
UCLASS()
class HAL_FUTURE_CREATION_API UHALBullChargeAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UHALBullChargeAbility();
	UPROPERTY(EditDefaultsOnly, Category="Bull Charge") TObjectPtr<UHALBullChargeConfig> SkillConfig;
	UHALBullChargeConfig* GetSkillConfig() const;
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCooldown(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
private:
	mutable FActiveGameplayEffectHandle CommittedCooldown;
};
