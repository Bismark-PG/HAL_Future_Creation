#pragma once

#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "GameplayEffect.h"
#include "HALSlipperyTrail.h"
#include "HALSlipperyTrailAbility.generated.h"

UCLASS()
class HAL_FUTURE_CREATION_API UHALSlipperyTrailConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UHALSlipperyTrailConfig();
	UPROPERTY(EditAnywhere, Category="Slippery Trail", meta=(ClampMin="0", ClampMax="120", Units="s")) float CooldownSeconds = 18.f;
	UPROPERTY(EditAnywhere, Category="Slippery Trail") FHALSlipperyTrailEmission Emission;
	UPROPERTY(EditAnywhere, Category="Slippery Trail") FHALSlipperyTrailShape Shape;
	UPROPERTY(EditAnywhere, Category="Slippery Trail") FHALSlipperyTrailHandling Handling;
	bool Validate(FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

UCLASS()
class UHALSlipperyTrailCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UHALSlipperyTrailCooldownEffect();
};

UCLASS()
class HAL_FUTURE_CREATION_API UHALSlipperyTrailAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UHALSlipperyTrailAbility();
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
protected:
	UPROPERTY(EditDefaultsOnly, Category="Slippery Trail") TObjectPtr<UHALSlipperyTrailConfig> SkillConfig;
	virtual void ApplyCooldown(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) const override;
private:
	const UHALSlipperyTrailConfig* GetSkillConfig() const;
	bool ResolveStart(const FGameplayAbilityActorInfo* ActorInfo, const UHALSlipperyTrailConfig* Config, FVector& Ground) const;
	UFUNCTION() void TrailDestroyed(AActor* Actor);
	TWeakObjectPtr<AHALSlipperyTrail> ActiveTrail;
	mutable FActiveGameplayEffectHandle CommittedCooldown;
};
