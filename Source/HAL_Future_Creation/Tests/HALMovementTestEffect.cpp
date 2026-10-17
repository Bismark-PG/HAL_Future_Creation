#include "HALMovementTestEffect.h"
#include "../HALMovementAttributeSet.h"
#include "../HALGameplayTags.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
UHALMovementBridgeTestEffect::UHALMovementBridgeTestEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(2);
	FGameplayModifierInfo Acceleration;
	Acceleration.Attribute = UHALMovementAttributeSet::GetAccelerationMultiplierDeltaAttribute();
	Acceleration.ModifierOp = EGameplayModOp::Additive;
	Acceleration.ModifierMagnitude = FScalableFloat(0.5f);
	Modifiers.Add(Acceleration);
	FGameplayModifierInfo Speed = Acceleration;
	Speed.Attribute = UHALMovementAttributeSet::GetSpeedLimitMultiplierDeltaAttribute();
	Speed.ModifierMagnitude = FScalableFloat(0.25f);
	Modifiers.Add(Speed);
	FInheritedTagContainer Tags; Tags.AddTag(HALGameplayTags::Effect_Scope_Avatar);
	auto* Scope = CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("MovementTestScope"));
	GEComponents.Add(Scope); Scope->SetAndApplyAssetTagChanges(Tags);
}
