#include "HALImpactImmunityAbility.h"
#include "HALGameFlowPermissions.h"
#include "HALAbilitySystemComponent.h"
#include "HALAbilityTypes.h"
#include "HALGameplayTags.h"
#include "HALImpactImmunityEffects.h"
#include "HALImpactReceiverComponent.h"
#include "VehicleKnockbackSettings.h"
#include "HALMatchGameState.h"
#include "TestVehiclePawn.h"
#include "VehicleHealthComponent.h"
#include "Engine/World.h"

UHALImpactImmunityAbility::UHALImpactImmunityAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ImmunityEffectClass = UHALImpactImmunityEffect::StaticClass();
	SetAssetTags(FGameplayTagContainer(HALGameplayTags::Ability_ImpactImmunity));
}

bool UHALImpactImmunityAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) { return false; }
	const auto* ASC = ActorInfo ? Cast<UHALAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	const auto* Vehicle = ActorInfo ? Cast<ATestVehiclePawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	const AActor* Owner = ActorInfo ? ActorInfo->OwnerActor.Get() : nullptr;
	const auto* Health = Vehicle ? Vehicle->FindComponentByClass<UVehicleHealthComponent>() : nullptr;
	const auto* Receiver = Vehicle ? Vehicle->FindComponentByClass<UHALImpactReceiverComponent>() : nullptr;
	FHALCollisionImpactRules Rules;
	const bool bTiered = Receiver && Receiver->GetConfiguration().Version == EHALImpactVersion::TieredV1
		&& GetDefault<UVehicleKnockbackSettings>()->GetCollisionImpactRules(Rules)
		&& Rules.Version == EHALImpactVersion::TieredV1;
	const UGameplayEffect* Immunity = ImmunityEffectClass ? ImmunityEffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;
	const bool bReady = ASC && ASC->IsHealthInitialized() && ASC->IsImpactInitialized() && Vehicle && Owner
		&& ASC->GetOwnerActor() == Owner && Vehicle->GetAbilitySystemComponent() == ASC
		&& Vehicle->GetController() && Vehicle->IsConfigurationValid()
		&& bTiered && Health && Health->IsHealthReady() && HALGameFlow::Permissions(Vehicle->GetWorld(),Vehicle).bAbilities
		&& Immunity && Immunity->DurationPolicy == EGameplayEffectDurationType::HasDuration
		&& Immunity->GetGrantedTags().HasTagExact(HALGameplayTags::State_Combat_ImpactImmune)
		&& Immunity->GetAssetTags().HasTagExact(HALGameplayTags::Effect_Scope_Avatar)
		&& !ASC->GetActiveGameplayEffect(ActiveUltimateEffect);
	if (!bReady && OptionalRelevantTags) { OptionalRelevantTags->AddTag(HALGameplayTags::Ability_Fail_VehicleState); }
	return bReady;
}


EHALUltimateStart UHALImpactImmunityAbility::StartUltimate(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData*)
{
	const auto Spec = MakeOutgoingGameplayEffectSpec(Handle, Info, Activation, ImmunityEffectClass, GetAbilityLevel(Handle, Info));
	if (!Spec.IsValid() || !FMath::IsFinite(Spec.Data->GetDuration()) || Spec.Data->GetDuration() <= 0) { return EHALUltimateStart::Failed; }
	ActiveUltimateEffect = ApplyGameplayEffectSpecToOwner(Handle, Info, Activation, Spec);
	return ActiveUltimateEffect.IsValid() ? EHALUltimateStart::Completed : EHALUltimateStart::Failed;
}
