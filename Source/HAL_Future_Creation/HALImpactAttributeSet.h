#pragma once
#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "HALImpactAttributeSet.generated.h"

/** Integer tier reduction, independent of damage resistance. Immunity is a separate GAS tag. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALImpactAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& Value) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& Value) const override;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UHALImpactAttributeSet, TierReduction)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(TierReduction)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(TierReduction)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(TierReduction)
private:
	UPROPERTY(ReplicatedUsing = OnRep_TierReduction)
	FGameplayAttributeData TierReduction;
	UFUNCTION()
	void OnRep_TierReduction(const FGameplayAttributeData& OldValue);
};
