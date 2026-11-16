#include "HALSideShiftAbility.h"
#include "HALSideShiftComponent.h"
#include "HALAbilitySystemComponent.h"
#include "HALGameplayTags.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Misc/DataValidation.h"

bool UHALSideShiftConfig::Validate(FString& Error) const
{
	if(MaximumCharges<1||MaximumCharges>8||!FMath::IsFinite(RechargeSeconds)||RechargeSeconds<.2f||RechargeSeconds>60
		||!FMath::IsFinite(DirectionDeadZone)||DirectionDeadZone<.05f||DirectionDeadZone>1||!Motion.IsValidTuning())
	{ Error=TEXT("Side Shift needs 1-8 charges, 0.2-60 second recharge, valid direction threshold and finite motion tuning."); return false; }
	if(FX.bEnabled&&!FX.IsValid()) { Error=TEXT("Side Shift FX needs valid world-space Niagara recipes, tire marks and exhaust mounts."); return false; }
	return true;
}
#if WITH_EDITOR
EDataValidationResult UHALSideShiftConfig::IsDataValid(FDataValidationContext& Context) const
{ FString Error; if(Validate(Error)) { return EDataValidationResult::Valid; } Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
#endif

UHALSideShiftChargeEffect::UHALSideShiftChargeEffect()
{
	DurationPolicy=EGameplayEffectDurationType::Infinite;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType=EGameplayEffectStackingType::AggregateByTarget;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount=8; StackDurationRefreshPolicy=EGameplayEffectStackingDurationPolicy::NeverRefresh;
	FInheritedTagContainer Tags; Tags.AddTag(HALGameplayTags::Effect_Scope_Avatar);
	auto* Scope=CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("SideShiftChargeScope")); GEComponents.Add(Scope); Scope->SetAndApplyAssetTagChanges(Tags);
}
UHALSideShiftStateEffect::UHALSideShiftStateEffect()
{
	DurationPolicy=EGameplayEffectDurationType::HasDuration; DurationMagnitude=FScalableFloat(1);
	FInheritedTagContainer Tags; Tags.AddTag(HALGameplayTags::State_SideShift);
	auto* Granted=CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("SideShiftState")); GEComponents.Add(Granted); Granted->SetAndApplyTargetTagChanges(Tags);
	FInheritedTagContainer ScopeTags; ScopeTags.AddTag(HALGameplayTags::Effect_Scope_Avatar);
	auto* Scope=CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("SideShiftScope")); GEComponents.Add(Scope); Scope->SetAndApplyAssetTagChanges(ScopeTags);
}
UHALSideShiftAbility::UHALSideShiftAbility()
{
	InstancingPolicy=EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy=EGameplayAbilityNetExecutionPolicy::ServerOnly;
	SetAssetTags(FGameplayTagContainer(HALGameplayTags::Ability_SideShift));
}
void UHALSideShiftAbility::OnGiveAbility(const FGameplayAbilityActorInfo* Info,const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(Info,Spec);
	if(!Info||!Info->IsNetAuthority()||Spec.SourceObject!=Info->AvatarActor.Get()) { return; }
	auto* ASC=Cast<UHALAbilitySystemComponent>(Info->AbilitySystemComponent.Get());
	auto* Component=Info->AvatarActor.IsValid()?Info->AvatarActor->FindComponentByClass<UHALSideShiftComponent>():nullptr;
	auto* Settings=GetClass()->GetDefaultObject<UHALSideShiftAbility>()->SkillConfig.Get();
	if(ASC&&Component&&Settings) { Component->Initialize(*ASC,*Settings); }
}
bool UHALSideShiftAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,
	const FGameplayTagContainer* Source,const FGameplayTagContainer* Target,FGameplayTagContainer* Reasons) const
{
	if(!Super::CanActivateAbility(Handle,Info,Source,Target,Reasons)) { return false; }
	const auto* C=Info&&Info->AvatarActor.IsValid()?Info->AvatarActor->FindComponentByClass<UHALSideShiftComponent>():nullptr;
	if(C&&C->CanStart()) { return true; }
	if(Reasons) { Reasons->AddTag(HALGameplayTags::Ability_Fail_VehicleState); } return false;
}
void UHALSideShiftAbility::OnRemoveAbility(const FGameplayAbilityActorInfo* Info,const FGameplayAbilitySpec& Spec)
{
	if(Info&&Info->IsNetAuthority()&&Spec.SourceObject==Info->AvatarActor.Get()&&Info->AvatarActor.IsValid())
	{ if(auto* C=Info->AvatarActor->FindComponentByClass<UHALSideShiftComponent>()) { C->Shutdown(); } }
	Super::OnRemoveAbility(Info,Spec);
}
void UHALSideShiftAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation,const FGameplayEventData*)
{
	auto* C=Info&&Info->AvatarActor.IsValid()?Info->AvatarActor->FindComponentByClass<UHALSideShiftComponent>():nullptr;
	const bool Started=Info&&Info->IsNetAuthority()&&C&&C->CanStart()&&CommitAbility(Handle,Info,Activation)&&C->Start();
	EndAbility(Handle,Info,Activation,true,!Started);
}
