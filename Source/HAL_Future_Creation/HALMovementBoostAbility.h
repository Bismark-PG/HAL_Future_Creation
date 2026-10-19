#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "HALMovementBoostAbility.generated.h"

class UHALMovementBoostEffect;

/** Instant server action. The GE owns duration/refresh; the movement bridge owns physics. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALMovementBoostAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UHALMovementBoostAbility();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	/** Authoritative effect selection; independent of HAL Debug's development-only class setting. */
	UPROPERTY(EditDefaultsOnly, Category = "Ability|Movement Boost")
	TSubclassOf<UHALMovementBoostEffect> MovementEffectClass;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
