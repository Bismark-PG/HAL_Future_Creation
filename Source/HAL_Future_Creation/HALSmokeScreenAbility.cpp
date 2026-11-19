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
	if(Rules.IsValid()&&FX.IsValid())
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
EHALUltimateStart UHALSmokeScreenAbility::StartUltimate(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData*)
{
	const auto* Config = GetClass()->GetDefaultObject<UHALSmokeScreenAbility>()->SkillConfig.Get();
	FVector Ground;
	if (ActiveSmoke.IsValid() || !Config || !ResolveStart(Info, Ground)) { return EHALUltimateStart::Failed; }
	FActorSpawnParameters Params;
	Params.Owner = Info->AvatarActor.Get();
	auto* Actor = Params.Owner->GetWorld()->SpawnActor<AHALSmokeScreen>(Params);
	if (!Actor || !Actor->Start(*Config, Ground))
	{
		if (Actor) { Actor->Destroy(); }
		return EHALUltimateStart::Failed;
	}
	ActiveSmoke = Actor;
	Actor->OnDestroyed.AddDynamic(this, &ThisClass::SmokeDestroyed);
	Actor->OnEmissionEnded.AddUObject(this, &ThisClass::SmokeEmissionEnded);
	return EHALUltimateStart::Active;
}
void UHALSmokeScreenAbility::SmokeDestroyed(AActor* Actor)
{ if(ActiveSmoke.Get(true)!=Actor) { return; } ActiveSmoke.Reset(); EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,true,false); }
void UHALSmokeScreenAbility::SmokeEmissionEnded(AHALSmokeScreen* Smoke)
{
	if (ActiveSmoke.Get() != Smoke) { return; }
	ActiveSmoke.Reset();
	Smoke->OnDestroyed.RemoveDynamic(this, &ThisClass::SmokeDestroyed);
	Smoke->OnEmissionEnded.RemoveAll(this);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
void UHALSmokeScreenAbility::EndAbility(FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,FGameplayAbilityActivationInfo Activation,bool bReplicate,bool bCancelled)
{
	if(auto* Smoke=ActiveSmoke.Get()) { ActiveSmoke.Reset(); Smoke->OnDestroyed.RemoveDynamic(this,&ThisClass::SmokeDestroyed); Smoke->OnEmissionEnded.RemoveAll(this); Smoke->Destroy(); }
	Super::EndAbility(Handle,Info,Activation,bReplicate,bCancelled);
}
