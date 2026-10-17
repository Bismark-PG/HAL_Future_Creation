#pragma once
#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "HALMovementAttributeSet.generated.h"

/** Additive deltas only. Clamp final movement scales, never discard GE contributions. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALMovementAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& Value) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& Value) const override;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UHALMovementAttributeSet, AccelerationMultiplierDelta)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(AccelerationMultiplierDelta)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(AccelerationMultiplierDelta)
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UHALMovementAttributeSet, SpeedLimitMultiplierDelta)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(SpeedLimitMultiplierDelta)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(SpeedLimitMultiplierDelta)
private:
	void RejectNonFinite(const FGameplayAttribute& Attribute, float& Value) const;
	UPROPERTY(ReplicatedUsing=OnRep_AccelerationDelta) FGameplayAttributeData AccelerationMultiplierDelta;
	UPROPERTY(ReplicatedUsing=OnRep_SpeedDelta) FGameplayAttributeData SpeedLimitMultiplierDelta;
	UFUNCTION() void OnRep_AccelerationDelta(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_SpeedDelta(const FGameplayAttributeData& Old);
};
