#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "HALHealthAttributeSet.generated.h"

/** The only runtime HP store for players and non-player combat targets. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALHealthAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UHALHealthAttributeSet, MaxHP)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(MaxHP)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(MaxHP)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(MaxHP)
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UHALHealthAttributeSet, CurrentHP)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(CurrentHP)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(CurrentHP)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(CurrentHP)

private:
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

	/** Zero until authority initializes from the vehicle's saved configuration. */
	UPROPERTY(ReplicatedUsing = OnRep_MaxHP)
	FGameplayAttributeData MaxHP;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentHP)
	FGameplayAttributeData CurrentHP;

	UFUNCTION()
	void OnRep_MaxHP(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_CurrentHP(const FGameplayAttributeData& OldValue);
};
