#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "HALCombatAttributeSet.generated.h"

/** Damage attributes only. Impact resistance and movement modifiers have separate contracts. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALCombatAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UHALCombatAttributeSet, AttackPower)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(AttackPower)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(AttackPower)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(AttackPower)
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UHALCombatAttributeSet, OutputMultiplierDelta)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(OutputMultiplierDelta)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(OutputMultiplierDelta)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(OutputMultiplierDelta)
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UHALCombatAttributeSet, DamageTakenMultiplier)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(DamageTakenMultiplier)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(DamageTakenMultiplier)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(DamageTakenMultiplier)
private:
	void ClampAttribute(const FGameplayAttribute& Attribute, float& Value) const;
	UPROPERTY(ReplicatedUsing = OnRep_AttackPower)
	FGameplayAttributeData AttackPower;
	UPROPERTY(ReplicatedUsing = OnRep_OutputMultiplierDelta)
	FGameplayAttributeData OutputMultiplierDelta;
	UPROPERTY(ReplicatedUsing = OnRep_DamageTakenMultiplier)
	FGameplayAttributeData DamageTakenMultiplier;
	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_OutputMultiplierDelta(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_DamageTakenMultiplier(const FGameplayAttributeData& OldValue);
};
