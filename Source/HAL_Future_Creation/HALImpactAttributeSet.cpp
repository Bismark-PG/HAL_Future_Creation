#include "HALImpactAttributeSet.h"
#include "Net/UnrealNetwork.h"

void UHALImpactAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UHALImpactAttributeSet, TierReduction, COND_None, REPNOTIFY_Always);
}

void UHALImpactAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& Value)
{
	Super::PreAttributeChange(Attribute, Value);
	if (Attribute == GetTierReductionAttribute())
	{
		Value = FMath::IsFinite(Value) ? FMath::Clamp(FMath::FloorToFloat(Value), 0.0f, 3.0f) : GetTierReduction();
	}
}

void UHALImpactAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& Value) const
{
	Super::PreAttributeBaseChange(Attribute, Value);
	if (Attribute == GetTierReductionAttribute())
	{
		Value = FMath::IsFinite(Value) ? FMath::Clamp(FMath::FloorToFloat(Value), 0.0f, 3.0f) : GetTierReduction();
	}
}

void UHALImpactAttributeSet::OnRep_TierReduction(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHALImpactAttributeSet, TierReduction, OldValue);
}
