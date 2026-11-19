#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "HALUltimateEnergy.generated.h"

namespace HALUltimate
{
	inline constexpr float Capacity = 100.f;
	inline constexpr int32 SideShiftCells = 4;
	inline constexpr float SideShiftCost = Capacity / SideShiftCells;
}

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALUltimateEnergyView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) float Energy = 0;
	UPROPERTY(BlueprintReadOnly) float Cost = 0;
	UPROPERTY(BlueprintReadOnly) float FullRechargeSeconds = 0;
	UPROPERTY(BlueprintReadOnly) double SampleServerTime = 0;
	UPROPERTY(BlueprintReadOnly) bool bHasEnergy = false;
	UPROPERTY(BlueprintReadOnly) bool bCanActivate = false;
	UPROPERTY(BlueprintReadOnly) bool bRecharging = false;
};

/** Owner-visible resource. Instant grants and successful casts change its base value on the server. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALUltimateEnergySet : public UAttributeSet
{
	GENERATED_BODY()
public:
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UHALUltimateEnergySet, Energy)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Energy)
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
private:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Energy, Category="Ultimate", meta=(AllowPrivateAccess="true"))
	FGameplayAttributeData Energy;
	UFUNCTION() void OnRep_Energy(const FGameplayAttributeData& OldValue);
};
