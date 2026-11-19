#include "HALUltimateEnergy.h"
#include "Net/UnrealNetwork.h"

void UHALUltimateEnergySet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UHALUltimateEnergySet, Energy, COND_OwnerOnly, REPNOTIFY_Always);
}

void UHALUltimateEnergySet::OnRep_Energy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHALUltimateEnergySet, Energy, OldValue);
}

void UHALUltimateEnergySet::PreAttributeChange(const FGameplayAttribute& Attribute, float& Value)
{
	Super::PreAttributeChange(Attribute, Value);
	if (Attribute == GetEnergyAttribute()) { Value = FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, HALUltimate::Capacity) : 0.f; }
}

void UHALUltimateEnergySet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& Value) const
{
	Super::PreAttributeBaseChange(Attribute, Value);
	if (Attribute == GetEnergyAttribute()) { Value = FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, HALUltimate::Capacity) : 0.f; }
}
