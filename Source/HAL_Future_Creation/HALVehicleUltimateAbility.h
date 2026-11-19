#pragma once

#include "Abilities/GameplayAbility.h"
#include "HALUltimateEnergy.h"
#include "HALVehicleUltimateAbility.generated.h"

enum class EHALUltimateStart : uint8 { Failed, Completed, Active };

/** Server-side transaction shared by all vehicle primary abilities. */
UCLASS(Abstract, HideCategories=(Costs, Cooldowns))
class HAL_FUTURE_CREATION_API UHALVehicleUltimateAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	virtual float GetUltimateCost() const { return HALUltimate::Capacity; }
	virtual bool CheckCost(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayTagContainer* Reasons = nullptr) const override final;
	// Existing GA Blueprints can still serialize the former cooldown GE.
	virtual UGameplayEffect* GetCooldownGameplayEffect() const override final { return nullptr; }
	virtual UGameplayEffect* GetCostGameplayEffect() const override final { return nullptr; }
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override final;
protected:
	virtual EHALUltimateStart StartUltimate(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) PURE_VIRTUAL(UHALVehicleUltimateAbility::StartUltimate, return EHALUltimateStart::Failed;);
};
