#include "HALPolicePursuitAbility.h"
#include "HALPolicePursuit.h"
#include "HALAbilitySystemComponent.h"
#include "HALGameplayTags.h"
#include "TestVehiclePawn.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "Misc/DataValidation.h"
namespace
{
	void Tags(UGameplayEffect& GE,TArray<TObjectPtr<UGameplayEffectComponent>>& Components,FGameplayTag Tag,bool Scope)
	{
		FInheritedTagContainer T; T.AddTag(Tag);
		auto* Granted=GE.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("GrantedTags")); Components.Add(Granted); Granted->SetAndApplyTargetTagChanges(T);
		if(Scope) { FInheritedTagContainer A; A.AddTag(HALGameplayTags::Effect_Scope_Avatar);
			auto* Asset=GE.CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("AvatarScope")); Components.Add(Asset); Asset->SetAndApplyAssetTagChanges(A); }
	}
}
bool UHALPolicePursuitConfig::Validate(FString& Error) const
{ if(FMath::IsFinite(CooldownSeconds)&&CooldownSeconds>=0&&CooldownSeconds<=120&&Rules.IsValid()) { Error.Reset(); return true; } Error=TEXT("Pursuit rules exceed finite documented ranges."); return false; }
#if WITH_EDITOR
EDataValidationResult UHALPolicePursuitConfig::IsDataValid(FDataValidationContext& Context) const
{ FString Error; if(Validate(Error)&&FX.IsValid()&&FX.Chains.IsValid()) { return EDataValidationResult::Valid; } Context.AddError(FText::FromString(Error.IsEmpty()?TEXT("Invalid pursuit presentation ranges; chain link count must be even."):Error)); return EDataValidationResult::Invalid; }
#endif
UHALPursuitCooldownEffect::UHALPursuitCooldownEffect()
{ DurationPolicy=EGameplayEffectDurationType::HasDuration; DurationMagnitude=FScalableFloat(1); Tags(*this,GEComponents,HALGameplayTags::Cooldown_Pursuit,false); }
UHALPursuitArrestEffect::UHALPursuitArrestEffect()
{ DurationPolicy=EGameplayEffectDurationType::Infinite; Tags(*this,GEComponents,HALGameplayTags::State_Arrested,true); }
UHALPursuitProtectionEffect::UHALPursuitProtectionEffect()
{ DurationPolicy=EGameplayEffectDurationType::Infinite; Tags(*this,GEComponents,HALGameplayTags::State_PursuitProtected,true); }
UHALPolicePursuitAbility::UHALPolicePursuitAbility()
{
	InstancingPolicy=EGameplayAbilityInstancingPolicy::InstancedPerActor; NetExecutionPolicy=EGameplayAbilityNetExecutionPolicy::ServerOnly;
	CooldownGameplayEffectClass=UHALPursuitCooldownEffect::StaticClass(); SetAssetTags(FGameplayTagContainer(HALGameplayTags::Ability_Pursuit));
}
bool UHALPolicePursuitAbility::Ready(const FGameplayAbilityActorInfo* I) const
{
	const auto* C=GetClass()->GetDefaultObject<UHALPolicePursuitAbility>()->SkillConfig.Get();
	const auto* V=I?Cast<ATestVehiclePawn>(I->AvatarActor.Get()):nullptr; const auto* ASC=I?Cast<UHALAbilitySystemComponent>(I->AbilitySystemComponent.Get()):nullptr;
	FString Error; return C&&C->Validate(Error)&&AHALPolicePursuit::VehicleReady(V)&&ASC&&ASC->GetAvatarActor()==V&&ASC->IsHealthInitialized()&&!ASC->IsStunned();
}
bool UHALPolicePursuitAbility::CanActivateAbility(FGameplayAbilitySpecHandle H,const FGameplayAbilityActorInfo* I,const FGameplayTagContainer* S,const FGameplayTagContainer* T,FGameplayTagContainer* F) const
{ if(!Super::CanActivateAbility(H,I,S,T,F)) { return false; } const bool OK=!Active.IsValid()&&Ready(I); if(!OK&&F) { F->AddTag(HALGameplayTags::Ability_Fail_VehicleState); } return OK; }
void UHALPolicePursuitAbility::ApplyCooldown(FGameplayAbilitySpecHandle H,const FGameplayAbilityActorInfo* I,FGameplayAbilityActivationInfo A) const
{
	const auto* C=GetClass()->GetDefaultObject<UHALPolicePursuitAbility>()->SkillConfig.Get(); if(!C||C->CooldownSeconds<=0) { return; }
	auto S=MakeOutgoingGameplayEffectSpec(H,I,A,UHALPursuitCooldownEffect::StaticClass(),GetAbilityLevel(H,I));
	if(S.IsValid()) { S.Data->SetDuration(C->CooldownSeconds,true); Cooldown=ApplyGameplayEffectSpecToOwner(H,I,A,S); }
}
void UHALPolicePursuitAbility::ActivateAbility(FGameplayAbilitySpecHandle H,const FGameplayAbilityActorInfo* I,FGameplayAbilityActivationInfo A,const FGameplayEventData*)
{
	Cooldown.Invalidate(); if(!I||!I->IsNetAuthority()||Active.IsValid()||!Ready(I)||!CommitAbility(H,I,A)) { EndAbility(H,I,A,true,true); return; }
	const auto* C=GetClass()->GetDefaultObject<UHALPolicePursuitAbility>()->SkillConfig.Get(); AHALPolicePursuit* P=nullptr;
	if(C->CooldownSeconds<=0||Cooldown.IsValid()) { FActorSpawnParameters Params; Params.Owner=I->AvatarActor.Get(); P=Params.Owner->GetWorld()->SpawnActor<AHALPolicePursuit>(Params); }
	if(!P||!P->Start(*C)) { if(P) { P->Destroy(); } if(Cooldown.IsValid()) { I->AbilitySystemComponent->RemoveActiveGameplayEffect(Cooldown); } EndAbility(H,I,A,true,true); return; }
	Active=P; P->OnDestroyed.AddDynamic(this,&ThisClass::PursuitEnded);
}
void UHALPolicePursuitAbility::PursuitEnded(AActor* Actor)
{ if(Active.Get(true)!=Actor) { return; } Active.Reset(); EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,true,false); }
void UHALPolicePursuitAbility::EndAbility(FGameplayAbilitySpecHandle H,const FGameplayAbilityActorInfo* I,FGameplayAbilityActivationInfo A,bool R,bool C)
{ if(auto* P=Active.Get()) { Active.Reset(); P->OnDestroyed.RemoveDynamic(this,&ThisClass::PursuitEnded); P->Destroy(); } Super::EndAbility(H,I,A,R,C); }
