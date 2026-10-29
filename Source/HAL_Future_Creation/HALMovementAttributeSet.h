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
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UHALMovementAttributeSet, LateralGripMultiplierDelta)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(LateralGripMultiplierDelta)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(LateralGripMultiplierDelta)
	// Reserved for the area's selected GE contributions; ordinary buffs use the three fields above.
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UHALMovementAttributeSet, AreaAccelerationDelta)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(AreaAccelerationDelta)
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UHALMovementAttributeSet, AreaSpeedDelta)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(AreaSpeedDelta)
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UHALMovementAttributeSet, AreaGripDelta)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(AreaGripDelta)
private:
	void RejectNonFinite(const FGameplayAttribute& Attribute, float& Value) const;
	UPROPERTY(ReplicatedUsing=OnRep_AccelerationDelta) FGameplayAttributeData AccelerationMultiplierDelta;
	UPROPERTY(ReplicatedUsing=OnRep_SpeedDelta) FGameplayAttributeData SpeedLimitMultiplierDelta;
	UPROPERTY(ReplicatedUsing=OnRep_GripDelta) FGameplayAttributeData LateralGripMultiplierDelta;
	UPROPERTY(ReplicatedUsing=OnRep_AreaAcceleration) FGameplayAttributeData AreaAccelerationDelta;
	UPROPERTY(ReplicatedUsing=OnRep_AreaSpeed) FGameplayAttributeData AreaSpeedDelta;
	UPROPERTY(ReplicatedUsing=OnRep_AreaGrip) FGameplayAttributeData AreaGripDelta;
	UFUNCTION() void OnRep_AccelerationDelta(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_SpeedDelta(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_GripDelta(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_AreaAcceleration(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_AreaSpeed(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_AreaGrip(const FGameplayAttributeData& Old);
};
