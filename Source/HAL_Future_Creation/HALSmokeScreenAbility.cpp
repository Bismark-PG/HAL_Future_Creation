#include "HALSmokeScreenAbility.h"
#include "HALSmokeScreen.h"
#include "HALAbilitySystemComponent.h"
#include "HALGameFlowPermissions.h"
#include "HALGameplayTags.h"
#include "TestVehiclePawn.h"
#include "VehicleHealthComponent.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Misc/DataValidation.h"

bool UHALSmokeScreenConfig::Validate(FString& Error) const
{
	if(FMath::IsFinite(CooldownSeconds)&&CooldownSeconds>=0&&CooldownSeconds<=120&&Rules.IsValid()&&FX.IsValid())
	{ Error.Reset(); return true; }
	Error=TEXT("Smoke needs finite timing/shape/FX in the documented ranges, sufficient puff capacity, and UI-domain overlay materials."); return false;
}
#if WITH_EDITOR
EDataValidationResult UHALSmokeScreenConfig::IsDataValid(FDataValidationContext& Context) const
{
	FString Error;
	if(Validate(Error)&&FX.JetSystem&&FX.CloudMaterial&&FX.ScreenMaterial) { return EDataValidationResult::Valid; }
	Context.AddError(FText::FromString(Error.IsEmpty()?TEXT("Smoke requires its Niagara jet and both UI-domain materials."):Error));
	return EDataValidationResult::Invalid;
}
#endif
UHALSmokeScreenCooldownEffect::UHALSmokeScreenCooldownEffect()
{
	DurationPolicy=EGameplayEffectDurationType::HasDuration; DurationMagnitude=FScalableFloat(1);
	FInheritedTagContainer Tags; Tags.AddTag(HALGameplayTags::Cooldown_SmokeScreen);
	auto* Granted=CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("SmokeCooldown"));
	GEComponents.Add(Granted); Granted->SetAndApplyTargetTagChanges(Tags);
}
UHALSmokeObscuredEffect::UHALSmokeObscuredEffect()
{
	DurationPolicy=EGameplayEffectDurationType::Infinite;
	FInheritedTagContainer Tags; Tags.AddTag(HALGameplayTags::State_Vision_SmokeObscured);
	auto* Granted=CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("SmokeVisionState"));
	GEComponents.Add(Granted); Granted->SetAndApplyTargetTagChanges(Tags);
	FInheritedTagContainer Scope; Scope.AddTag(HALGameplayTags::Effect_Scope_Avatar);
	auto* Asset=CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("SmokeAvatarScope"));
	GEComponents.Add(Asset); Asset->SetAndApplyAssetTagChanges(Scope);
}
UHALSmokeScreenAbility::UHALSmokeScreenAbility()
{
	InstancingPolicy=EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy=EGameplayAbilityNetExecutionPolicy::ServerOnly;
	CooldownGameplayEffectClass=UHALSmokeScreenCooldownEffect::StaticClass();
	SetAssetTags(FGameplayTagContainer(HALGameplayTags::Ability_SmokeScreen));
}
bool UHALSmokeScreenAbility::ResolveStart(const FGameplayAbilityActorInfo* Info,FVector& Ground) const
{
	const auto* Config=GetClass()->GetDefaultObject<UHALSmokeScreenAbility>()->SkillConfig.Get();
	const auto* V=Info?Cast<ATestVehiclePawn>(Info->AvatarActor.Get()):nullptr;
	const auto* ASC=Info?Cast<UHALAbilitySystemComponent>(Info->AbilitySystemComponent.Get()):nullptr;
	const auto* Health=V?V->FindComponentByClass<UVehicleHealthComponent>():nullptr; FString Error;
	return Config&&Config->Validate(Error)&&V&&ASC&&ASC->GetAvatarActor()==V&&ASC->IsHealthInitialized()&&!ASC->IsStunned()
		&&V->IsConfigurationValid()&&Health&&Health->IsHealthReady()&&Health->GetCurrentHP()>0
		&&V->GetController()&&V->GetController()->GetPawn()==V&&HALGameFlow::Permissions(V->GetWorld(),V).bAbilities
		&&AHALSmokeScreen::FindOutletGround(*V,Config->Rules,Ground);
}
bool UHALSmokeScreenAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,
	const FGameplayTagContainer* Source,const FGameplayTagContainer* Target,FGameplayTagContainer* Reasons) const
{
	if(!Super::CanActivateAbility(Handle,Info,Source,Target,Reasons)) { return false; }
	FVector Ground; const bool Ready=!ActiveSmoke.IsValid()&&ResolveStart(Info,Ground);
	if(!Ready&&Reasons) { Reasons->AddTag(HALGameplayTags::Ability_Fail_VehicleState); } return Ready;
}
void UHALSmokeScreenAbility::ApplyCooldown(FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,FGameplayAbilityActivationInfo Activation) const
{
	const auto* C=GetClass()->GetDefaultObject<UHALSmokeScreenAbility>()->SkillConfig.Get(); if(!C||C->CooldownSeconds<=0) { return; }
	const auto Spec=MakeOutgoingGameplayEffectSpec(Handle,Info,Activation,UHALSmokeScreenCooldownEffect::StaticClass(),GetAbilityLevel(Handle,Info));
	if(!Spec.IsValid()) { return; } Spec.Data->SetDuration(C->CooldownSeconds,true);
	CommittedCooldown=ApplyGameplayEffectSpecToOwner(Handle,Info,Activation,Spec);
}
void UHALSmokeScreenAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,FGameplayAbilityActivationInfo Activation,const FGameplayEventData*)
{
	FVector Ground; CommittedCooldown.Invalidate();
	if(!Info||!Info->IsNetAuthority()||ActiveSmoke.IsValid()||!ResolveStart(Info,Ground)||!CommitAbility(Handle,Info,Activation))
	{ EndAbility(Handle,Info,Activation,true,true); return; }
	const auto* C=GetClass()->GetDefaultObject<UHALSmokeScreenAbility>()->SkillConfig.Get(); AHALSmokeScreen* Smoke=nullptr;
	if(C->CooldownSeconds<=0||CommittedCooldown.IsValid())
	{ FActorSpawnParameters P; P.Owner=Info->AvatarActor.Get(); Smoke=P.Owner->GetWorld()->SpawnActor<AHALSmokeScreen>(P); }
	if(!Smoke||!Smoke->Start(*C,Ground))
	{
		if(Smoke) { Smoke->Destroy(); }
		if(CommittedCooldown.IsValid()) { Info->AbilitySystemComponent->RemoveActiveGameplayEffect(CommittedCooldown); }
		EndAbility(Handle,Info,Activation,true,true); return;
	}
	ActiveSmoke=Smoke; Smoke->OnDestroyed.AddDynamic(this,&ThisClass::SmokeDestroyed);
}
void UHALSmokeScreenAbility::SmokeDestroyed(AActor* Actor)
{ if(ActiveSmoke.Get(true)!=Actor) { return; } ActiveSmoke.Reset(); EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,true,false); }
void UHALSmokeScreenAbility::EndAbility(FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,FGameplayAbilityActivationInfo Activation,bool bReplicate,bool bCancelled)
{
	if(auto* Smoke=ActiveSmoke.Get()) { ActiveSmoke.Reset(); Smoke->OnDestroyed.RemoveDynamic(this,&ThisClass::SmokeDestroyed); Smoke->Destroy(); }
	Super::EndAbility(Handle,Info,Activation,bReplicate,bCancelled);
}
