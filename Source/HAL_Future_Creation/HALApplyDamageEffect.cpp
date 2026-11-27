#include "HALApplyDamageEffect.h"

#include "HALGameplayTags.h"
#include "HALHealthAttributeSet.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"

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

UHALApplyElectrifiedBallDamageEffect::UHALApplyElectrifiedBallDamageEffect()
{
	FInheritedTagContainer Asset;
	Asset.AddTag(HALGameplayTags::Effect_Ball_Electrified);
	auto* Tags=CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("ElectricBallAssetTags"));
	GEComponents.Add(Tags); Tags->SetAndApplyAssetTagChanges(Asset);
}
