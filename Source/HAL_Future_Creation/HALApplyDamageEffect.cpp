#include "HALApplyDamageEffect.h"

#include "HALGameplayTags.h"
#include "HALHealthAttributeSet.h"

UHALApplyDamageEffect::UHALApplyDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UHALHealthAttributeSet::GetCurrentHPAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat Magnitude;
	Magnitude.DataTag = HALGameplayTags::Data_Combat_Damage;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
}
