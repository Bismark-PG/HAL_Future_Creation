#include "HALMovementBoostAbility.h"
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
	// No cost/cooldown in this first movement skill. In particular, no immunity cooldown/tag dependency.
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
	const auto* Match = Vehicle ? Vehicle->GetWorld()->GetGameState<AHALMatchGameState>() : nullptr;
	const auto* Health = Vehicle ? Vehicle->FindComponentByClass<UVehicleHealthComponent>() : nullptr;
	const auto* Effect = MovementEffectClass ? MovementEffectClass->GetDefaultObject<UHALMovementBoostEffect>() : nullptr;
	const bool bReady = ASC && Vehicle && Owner && ASC->GetOwnerActor() == Owner
		&& Vehicle->GetPlayerState() == Owner && Vehicle->GetAbilitySystemComponent() == ASC
		&& Vehicle->GetController() && Vehicle->IsConfigurationValid()
		&& ASC->IsHealthInitialized() && ASC->GetSet<UHALMovementAttributeSet>() && Health && Health->IsHealthReady()
		&& Match && Match->GetMatchPhase() == EHALMatchPhase::Playing;
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

void UHALMovementBoostAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->IsNetAuthority()) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	const auto Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, MovementEffectClass, GetAbilityLevel(Handle, ActorInfo));
	if (!Spec.IsValid() || !FMath::IsFinite(Spec.Data->GetDuration()) || Spec.Data->GetDuration() <= 0 || Spec.Data->GetPeriod() != 0
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return;
	}
	const auto Applied = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	if (HALAbilities::ShouldLog())
	{
		const auto* ASC = CastChecked<UHALAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get());
		UE_LOG(LogHALAbility, Log, TEXT("Movement ability: Accepted=%d Owner=%s Avatar=%s Ability=%s Effect=%s Duration=%.3f Remaining=%.3f GameTime=%.3f"),
			Applied.IsValid(), *GetNameSafe(ActorInfo->OwnerActor.Get()), *GetNameSafe(ActorInfo->AvatarActor.Get()),
			*GetName(), *GetNameSafe(Spec.Data->Def), Spec.Data->GetDuration(),
			ASC->GetEffectTimeRemaining(HALGameplayTags::State_Movement_Boost), GetWorld()->GetTimeSeconds());
	}
	if (Applied.IsValid()) { ActorInfo->OwnerActor->ForceNetUpdate(); }
	// End the input action now, so another press can refresh the still-running GE.
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !Applied.IsValid());
}
