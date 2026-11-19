#include "HALMovementBoostAbility.h"
#include "HALGameFlowPermissions.h"
#include "HALAbilitySystemComponent.h"
#include "HALAbilityTypes.h"
#include "HALGameplayTags.h"
#include "HALMatchGameState.h"
#include "HALMovementAttributeSet.h"
#include "HALMovementBoostEffect.h"
#include "TestVehiclePawn.h"
#include "VehicleHealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

UHALMovementBoostAbility::UHALMovementBoostAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	MovementEffectClass = UHALMovementBoostEffect::StaticClass();
	SetAssetTags(FGameplayTagContainer(HALGameplayTags::Ability_MovementBoost));
}

bool UHALMovementBoostAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) { return false; }
	const auto* ASC = ActorInfo ? Cast<UHALAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	const auto* Vehicle = ActorInfo ? Cast<ATestVehiclePawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	const AActor* Owner = ActorInfo ? ActorInfo->OwnerActor.Get() : nullptr;
	const auto* Health = Vehicle ? Vehicle->FindComponentByClass<UVehicleHealthComponent>() : nullptr;
	const auto* Effect = MovementEffectClass ? MovementEffectClass->GetDefaultObject<UHALMovementBoostEffect>() : nullptr;
	const bool bReady = ASC && !ASC->GetActiveGameplayEffect(ActiveUltimateEffect) && Vehicle && Owner && ASC->GetOwnerActor() == Owner
		&& Vehicle->GetPlayerState() == Owner && Vehicle->GetAbilitySystemComponent() == ASC
		&& Vehicle->GetController() && Vehicle->IsConfigurationValid()
		&& ASC->IsHealthInitialized() && ASC->GetSet<UHALMovementAttributeSet>() && Health && Health->IsHealthReady()
		&& HALGameFlow::Permissions(Vehicle->GetWorld(),Vehicle).bAbilities;
	const bool bEffectValid = Effect && Effect->DurationPolicy == EGameplayEffectDurationType::HasDuration
		&& Effect->GetStackingType() == EGameplayEffectStackingType::AggregateByTarget && Effect->GetStackLimitCount() == 1
		&& Effect->StackDurationRefreshPolicy == EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication
		&& Effect->StackExpirationPolicy == EGameplayEffectStackingExpirationPolicy::ClearEntireStack && !Effect->bDenyOverflowApplication
		&& Effect->GetAssetTags().HasTagExact(HALGameplayTags::Effect_Scope_Avatar)
		&& Effect->GetAssetTags().HasTagExact(HALGameplayTags::Effect_Movement_Boost)
		&& Effect->GetGrantedTags().HasTagExact(HALGameplayTags::State_Movement_Boost);
	if (!bReady || !bEffectValid)
	{
		if (OptionalRelevantTags) { OptionalRelevantTags->AddTag(HALGameplayTags::Ability_Fail_VehicleState); }
		return false;
	}
	const auto Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, FGameplayAbilityActivationInfo(), MovementEffectClass, GetAbilityLevel(Handle, ActorInfo));
	// A periodic Modifier executes against base values instead of a reversible duration contribution.
	const bool bSpecValid = Spec.IsValid() && FMath::IsFinite(Spec.Data->GetDuration()) && Spec.Data->GetDuration() > 0
		&& Spec.Data->GetPeriod() == 0;
	if (!bSpecValid && OptionalRelevantTags) { OptionalRelevantTags->AddTag(HALGameplayTags::Ability_Fail_VehicleState); }
	return bSpecValid;
}

EHALUltimateStart UHALMovementBoostAbility::StartUltimate(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData*)
{
	const auto Spec = MakeOutgoingGameplayEffectSpec(Handle, Info, Activation, MovementEffectClass, GetAbilityLevel(Handle, Info));
	if (!Spec.IsValid() || !FMath::IsFinite(Spec.Data->GetDuration()) || Spec.Data->GetDuration() <= 0) { return EHALUltimateStart::Failed; }
	ActiveUltimateEffect = ApplyGameplayEffectSpecToOwner(Handle, Info, Activation, Spec);
	return ActiveUltimateEffect.IsValid() ? EHALUltimateStart::Completed : EHALUltimateStart::Failed;
}
