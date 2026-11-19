#include "HALHookThrowAbility.h"
#include "HALHookThrow.h"
#include "HALAbilitySystemComponent.h"
#include "HALGameplayTags.h"
#include "TestVehiclePawn.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Misc/DataValidation.h"

bool UHALHookThrowConfig::Validate(FString& Error) const
{
	if(Rules.IsValid()&&FX.IsValid()) { Error.Reset(); return true; }
	Error=TEXT("Hook needs finite gameplay/metal parameters within their documented ranges and a bounded total lifetime."); return false;
}
#if WITH_EDITOR
EDataValidationResult UHALHookThrowConfig::IsDataValid(FDataValidationContext& Context) const
{
	FString Error; if(Validate(Error)&&(!FX.bEnabled||(FX.ChainMesh&&FX.HookMesh&&FX.MetalMaterial))) { return EDataValidationResult::Valid; }
	Context.AddError(FText::FromString(Error.IsEmpty()?TEXT("Enabled hook FX requires both meshes and its toon metal material."):Error)); return EDataValidationResult::Invalid;
}
#endif
namespace
{
	UTargetTagsGameplayEffectComponent* HookGrantTag(UGameplayEffect& Effect,FGameplayTag Tag)
	{
		FInheritedTagContainer Tags; Tags.AddTag(Tag);
		auto* Component=Effect.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("HookGrantedTags"));
		Component->SetAndApplyTargetTagChanges(Tags); return Component;
	}
	UAssetTagsGameplayEffectComponent* HookAvatarScope(UGameplayEffect& Effect)
	{
		FInheritedTagContainer Tags; Tags.AddTag(HALGameplayTags::Effect_Scope_Avatar);
		auto* C=Effect.CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("HookAvatarScope")); C->SetAndApplyAssetTagChanges(Tags); return C;
	}
}
UHALHookCooldownEffect::UHALHookCooldownEffect()
{ DurationPolicy=EGameplayEffectDurationType::HasDuration; DurationMagnitude=FScalableFloat(1); GEComponents.Add(HookGrantTag(*this,HALGameplayTags::Cooldown_HookThrow)); }
UHALHookCastingEffect::UHALHookCastingEffect()
{ DurationPolicy=EGameplayEffectDurationType::HasDuration; DurationMagnitude=FScalableFloat(1); GEComponents.Add(HookGrantTag(*this,HALGameplayTags::State_HookCasting)); GEComponents.Add(HookAvatarScope(*this)); }
UHALHookCapturedEffect::UHALHookCapturedEffect()
{ DurationPolicy=EGameplayEffectDurationType::HasDuration; DurationMagnitude=FScalableFloat(1); GEComponents.Add(HookGrantTag(*this,HALGameplayTags::State_Stun)); GEComponents.Add(HookAvatarScope(*this)); }
UHALHookThrowAbility::UHALHookThrowAbility()
{
	InstancingPolicy=EGameplayAbilityInstancingPolicy::InstancedPerActor; NetExecutionPolicy=EGameplayAbilityNetExecutionPolicy::ServerOnly;
	SetAssetTags(FGameplayTagContainer(HALGameplayTags::Ability_HookThrow));
}
bool UHALHookThrowAbility::Ready(const FGameplayAbilityActorInfo* Info) const
{
	const auto* C=GetClass()->GetDefaultObject<UHALHookThrowAbility>()->SkillConfig.Get(); const auto* V=Info?Cast<ATestVehiclePawn>(Info->AvatarActor.Get()):nullptr;
	FString Error; return C&&C->Validate(Error)&&V&&AHALHookThrow::CanStart(*V);
}
bool UHALHookThrowAbility::CanActivateAbility(FGameplayAbilitySpecHandle H,const FGameplayAbilityActorInfo* I,const FGameplayTagContainer* S,const FGameplayTagContainer* T,FGameplayTagContainer* R) const
{ return Super::CanActivateAbility(H,I,S,T,R)&&!ActiveHook.IsValid()&&Ready(I); }
EHALUltimateStart UHALHookThrowAbility::StartUltimate(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData*)
{
	const auto* Config = GetClass()->GetDefaultObject<UHALHookThrowAbility>()->SkillConfig.Get();
	if (ActiveHook.IsValid() || !Config || !Ready(Info)) { return EHALUltimateStart::Failed; }
	FActorSpawnParameters Params;
	Params.Owner = Info->AvatarActor.Get();
	auto* Actor = Params.Owner->GetWorld()->SpawnActor<AHALHookThrow>(Params);
	if (!Actor || !Actor->Start(*Config))
	{
		if (Actor) { Actor->Destroy(); }
		return EHALUltimateStart::Failed;
	}
	ActiveHook = Actor;
	Actor->OnDestroyed.AddDynamic(this, &ThisClass::HookDestroyed);
	return EHALUltimateStart::Active;
}
void UHALHookThrowAbility::HookDestroyed(AActor* Actor)
{ if(ActiveHook.Get(true)!=Actor) { return; } ActiveHook.Reset(); EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,true,false); }
void UHALHookThrowAbility::EndAbility(FGameplayAbilitySpecHandle H,const FGameplayAbilityActorInfo* I,FGameplayAbilityActivationInfo A,bool Replicate,bool Cancelled)
{
	if(auto* Hook=ActiveHook.Get()) { ActiveHook.Reset(); Hook->OnDestroyed.RemoveDynamic(this,&ThisClass::HookDestroyed); Hook->Destroy(); }
	Super::EndAbility(H,I,A,Replicate,Cancelled);
}
