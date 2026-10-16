#include "HALImpactImmunityEffects.h"
#include "HALGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"

UHALImpactImmunityEffect::UHALImpactImmunityEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(8.0f);
	// UE 5.8.2's SetStackingType is not exported by the installed engine DLL.
	// Set the constructor default directly; keep this compatibility exception local.
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::ClearEntireStack;
	bDenyOverflowApplication = false; // At limit, a legal reapplication still refreshes duration.
	FInheritedTagContainer Granted;
	Granted.AddTag(HALGameplayTags::State_Combat_ImpactImmune);
	auto* GrantComponent = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("ImmunityGrantedTags"));
	GEComponents.Add(GrantComponent);
	GrantComponent->SetAndApplyTargetTagChanges(Granted);
	FInheritedTagContainer Asset;
	Asset.AddTag(HALGameplayTags::Effect_Scope_Avatar);
	Asset.AddTag(HALGameplayTags::Effect_ImpactImmunity);
	auto* AssetComponent = CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("ImmunityAssetTags"));
	GEComponents.Add(AssetComponent);
	AssetComponent->SetAndApplyAssetTagChanges(Asset);
}

UHALImpactImmunityCooldownEffect::UHALImpactImmunityCooldownEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(20.0f);
	FInheritedTagContainer Granted;
	Granted.AddTag(HALGameplayTags::Cooldown_ImpactImmunity);
	auto* GrantComponent = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("CooldownGrantedTags"));
	GEComponents.Add(GrantComponent);
	GrantComponent->SetAndApplyTargetTagChanges(Granted);
}

UHALDebugImpactImmunityEffect::UHALDebugImpactImmunityEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	FInheritedTagContainer Granted;
	Granted.AddTag(HALGameplayTags::State_Combat_ImpactImmune);
	auto* GrantComponent = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("DebugImmunityGrantedTags"));
	GEComponents.Add(GrantComponent);
	GrantComponent->SetAndApplyTargetTagChanges(Granted);
}
