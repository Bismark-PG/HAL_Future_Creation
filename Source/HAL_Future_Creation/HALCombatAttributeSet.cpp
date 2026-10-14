#include "HALCombatAttributeSet.h"
#include "Net/UnrealNetwork.h"

void UHALCombatAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UHALCombatAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHALCombatAttributeSet, OutputMultiplierDelta, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHALCombatAttributeSet, DamageTakenMultiplier, COND_None, REPNOTIFY_Always);
}

void UHALCombatAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& Value) const
{
	if (Attribute == GetOutputMultiplierDeltaAttribute())
	{
		if (!FMath::IsFinite(Value)) { Value = GetOutputMultiplierDelta(); }
	}
	else if (Attribute == GetAttackPowerAttribute() || Attribute == GetDamageTakenMultiplierAttribute())
	{
		Value = FMath::IsFinite(Value) ? FMath::Max(0.0f, Value)
			: (Attribute == GetAttackPowerAttribute() ? GetAttackPower() : GetDamageTakenMultiplier());
	}
}

void UHALCombatAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UHALCombatAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UHALCombatAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHALCombatAttributeSet, AttackPower, OldValue);
}
void UHALCombatAttributeSet::OnRep_OutputMultiplierDelta(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHALCombatAttributeSet, OutputMultiplierDelta, OldValue);
}
void UHALCombatAttributeSet::OnRep_DamageTakenMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHALCombatAttributeSet, DamageTakenMultiplier, OldValue);
}
