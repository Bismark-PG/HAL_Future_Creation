#include "HALShockedEffect.h"
#include "HALGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"

UHALShockedEffect::UHALShockedEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	FInheritedTagContainer Granted;
	Granted.AddTag(HALGameplayTags::State_Visual_Shocked);
	auto* Tags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("ShockedTags"));
	GEComponents.Add(Tags); Tags->SetAndApplyTargetTagChanges(Granted);
	FInheritedTagContainer Asset;
	Asset.AddTag(HALGameplayTags::Effect_Scope_Avatar);
	Asset.AddTag(HALGameplayTags::Effect_Visual_Shocked);
	auto* Scope = CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("ShockedScope"));
	GEComponents.Add(Scope); Scope->SetAndApplyAssetTagChanges(Asset);
}
