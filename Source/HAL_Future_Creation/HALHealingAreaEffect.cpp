#include "HALHealingAreaEffect.h"
#include "HALHealthAttributeSet.h"
#include "HALGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"

bool FHALHealingAreaRecipe::IsValid() const
{
	return FMath::IsFinite(HealPerTick) && HealPerTick >= .01f && HealPerTick <= 10000.f
		&& FMath::IsFinite(PeriodSeconds) && PeriodSeconds >= .1f && PeriodSeconds <= 60.f;
}

UHALHealingAreaEffect::UHALHealingAreaEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = 1.f;
	bExecutePeriodicEffectOnApplication = false;
	StackingType = EGameplayEffectStackingType::None;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UHALHealthAttributeSet::GetCurrentHPAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat Magnitude; Magnitude.DataTag = HALGameplayTags::Data_Healing_AreaAmount;
	Modifier.ModifierMagnitude = Magnitude; Modifiers.Add(Modifier);
	FInheritedTagContainer Granted; Granted.AddTag(HALGameplayTags::State_Healing_Area);
	auto* Grant = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("HealingAreaGrantedTags"));
	GEComponents.Add(Grant); Grant->SetAndApplyTargetTagChanges(Granted);
	FInheritedTagContainer Asset; Asset.AddTag(HALGameplayTags::Effect_Scope_Avatar); Asset.AddTag(HALGameplayTags::Effect_Healing_Area);
	auto* Scope = CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("HealingAreaAssetTags"));
	GEComponents.Add(Scope); Scope->SetAndApplyAssetTagChanges(Asset);
}
