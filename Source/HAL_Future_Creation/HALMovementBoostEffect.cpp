#include "HALMovementBoostEffect.h"
#include "HALMovementAttributeSet.h"
#include "HALGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"

UHALMovementBoostEffect::UHALMovementBoostEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(5.0f);
	// Same installed-engine export limitation as the existing immunity GE.
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::ClearEntireStack;
	bDenyOverflowApplication = false;
	FGameplayModifierInfo Acceleration;
	Acceleration.Attribute = UHALMovementAttributeSet::GetAccelerationMultiplierDeltaAttribute();
	Acceleration.ModifierOp = EGameplayModOp::Additive;
	Acceleration.ModifierMagnitude = FScalableFloat(0.5f);
	Modifiers.Add(Acceleration);
	FGameplayModifierInfo Speed;
	Speed.Attribute = UHALMovementAttributeSet::GetSpeedLimitMultiplierDeltaAttribute();
	Speed.ModifierOp = EGameplayModOp::Additive;
	Speed.ModifierMagnitude = FScalableFloat(0.25f);
	Modifiers.Add(Speed);
	FInheritedTagContainer Granted;
	Granted.AddTag(HALGameplayTags::State_Movement_Boost);
	auto* GrantComponent = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("BoostGrantedTags"));
	GEComponents.Add(GrantComponent);
	GrantComponent->SetAndApplyTargetTagChanges(Granted);
	FInheritedTagContainer Asset;
	Asset.AddTag(HALGameplayTags::Effect_Scope_Avatar);
	Asset.AddTag(HALGameplayTags::Effect_Movement_Boost);
	auto* AssetComponent = CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("BoostAssetTags"));
	GEComponents.Add(AssetComponent);
	AssetComponent->SetAndApplyAssetTagChanges(Asset);
}
