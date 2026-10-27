#include "HALStunEffect.h"
#include "HALGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"

UHALStunEffect::UHALStunEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(1.f); // Overridden by the authority application spec.
	FInheritedTagContainer Granted;
	Granted.AddTag(HALGameplayTags::State_Stun);
	auto* Tags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("StunGrantedTags"));
	GEComponents.Add(Tags);
	Tags->SetAndApplyTargetTagChanges(Granted);
	FInheritedTagContainer Asset;
	Asset.AddTag(HALGameplayTags::Effect_Scope_Avatar);
	Asset.AddTag(HALGameplayTags::Effect_Stun);
	auto* Scope = CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("StunAssetTags"));
	GEComponents.Add(Scope);
	Scope->SetAndApplyAssetTagChanges(Asset);
}
