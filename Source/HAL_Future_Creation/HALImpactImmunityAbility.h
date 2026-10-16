#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "HALImpactImmunityAbility.generated.h"

/** Authority activation; GE owns the timed state after this instantaneous action ends. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALImpactImmunityAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UHALImpactImmunityAbility();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Ability|Impact Immunity")
	TSubclassOf<UGameplayEffect> ImmunityEffectClass;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

private:
	mutable FActiveGameplayEffectHandle CommittedCooldown;
};
