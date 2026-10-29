#include "HALMovementAreaEffects.h"
#include "HALMovementAttributeSet.h"
#include "HALGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"

namespace
{
	FGameplayModifierInfo MakeAreaModifier(FGameplayAttribute Attribute, FGameplayTag Tag)
	{
		FGameplayModifierInfo Modifier; Modifier.Attribute = Attribute; Modifier.ModifierOp = EGameplayModOp::Additive;
		FSetByCallerFloat Magnitude; Magnitude.DataTag = Tag; Modifier.ModifierMagnitude = Magnitude;
		return Modifier;
	}
}
UHALSlowAreaEffect::UHALSlowAreaEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Modifiers.Add(MakeAreaModifier(UHALMovementAttributeSet::GetAreaAccelerationDeltaAttribute(), HALGameplayTags::Data_Movement_AreaAcceleration));
	Modifiers.Add(MakeAreaModifier(UHALMovementAttributeSet::GetAreaSpeedDeltaAttribute(), HALGameplayTags::Data_Movement_AreaSpeed));
	FInheritedTagContainer Granted; Granted.AddTag(HALGameplayTags::State_Movement_SlowArea);
	auto* GrantComponent = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("SlowAreaGrantedTags"));
	GEComponents.Add(GrantComponent); GrantComponent->SetAndApplyTargetTagChanges(Granted);
	FInheritedTagContainer Asset; Asset.AddTag(HALGameplayTags::Effect_Scope_Avatar); Asset.AddTag(HALGameplayTags::Effect_Movement_Area);
	auto* AssetComponent = CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("SlowAreaAssetTags"));
	GEComponents.Add(AssetComponent); AssetComponent->SetAndApplyAssetTagChanges(Asset);
}
UHALSlipperyAreaEffect::UHALSlipperyAreaEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Modifiers.Add(MakeAreaModifier(UHALMovementAttributeSet::GetAreaGripDeltaAttribute(), HALGameplayTags::Data_Movement_AreaGrip));
	FInheritedTagContainer Granted; Granted.AddTag(HALGameplayTags::State_Movement_SlipperyArea);
	auto* GrantComponent = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("SlipperyAreaGrantedTags"));
	GEComponents.Add(GrantComponent); GrantComponent->SetAndApplyTargetTagChanges(Granted);
	FInheritedTagContainer Asset; Asset.AddTag(HALGameplayTags::Effect_Scope_Avatar); Asset.AddTag(HALGameplayTags::Effect_Movement_Area);
	auto* AssetComponent = CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("SlipperyAreaAssetTags"));
	GEComponents.Add(AssetComponent); AssetComponent->SetAndApplyAssetTagChanges(Asset);
}
