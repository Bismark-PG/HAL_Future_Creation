#include "HALImpactImmunityAbility.h"
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
	CooldownGameplayEffectClass = UHALImpactImmunityCooldownEffect::StaticClass();
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
	const auto* Match = Vehicle ? Vehicle->GetWorld()->GetGameState<AHALMatchGameState>() : nullptr;
	const auto* Health = Vehicle ? Vehicle->FindComponentByClass<UVehicleHealthComponent>() : nullptr;
	const auto* Receiver = Vehicle ? Vehicle->FindComponentByClass<UHALImpactReceiverComponent>() : nullptr;
	FHALCollisionImpactRules Rules;
	const bool bTiered = Receiver && Receiver->GetConfiguration().Version == EHALImpactVersion::TieredV1
		&& GetDefault<UVehicleKnockbackSettings>()->GetCollisionImpactRules(Rules)
		&& Rules.Version == EHALImpactVersion::TieredV1;
	const UGameplayEffect* Immunity = ImmunityEffectClass ? ImmunityEffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;
	const UGameplayEffect* Cooldown = GetCooldownGameplayEffect();
	const bool bReady = ASC && ASC->IsHealthInitialized() && ASC->IsImpactInitialized() && Vehicle && Owner
		&& ASC->GetOwnerActor() == Owner && Vehicle->GetAbilitySystemComponent() == ASC
		&& Vehicle->GetController() && Vehicle->IsConfigurationValid()
		&& bTiered && Health && Health->IsHealthReady() && Match && Match->GetMatchPhase() == EHALMatchPhase::Playing
		&& Immunity && Immunity->DurationPolicy == EGameplayEffectDurationType::HasDuration
		&& Immunity->GetGrantedTags().HasTagExact(HALGameplayTags::State_Combat_ImpactImmune)
		&& Immunity->GetAssetTags().HasTagExact(HALGameplayTags::Effect_Scope_Avatar)
		&& Cooldown && Cooldown->DurationPolicy == EGameplayEffectDurationType::HasDuration
		&& Cooldown->GetGrantedTags().HasTagExact(HALGameplayTags::Cooldown_ImpactImmunity);
	if (!bReady && OptionalRelevantTags) { OptionalRelevantTags->AddTag(HALGameplayTags::Ability_Fail_VehicleState); }
	return bReady;
}

void UHALImpactImmunityAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	CommittedCooldown = ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo,
		GetCooldownGameplayEffect(), GetAbilityLevel(Handle, ActorInfo));
}

void UHALImpactImmunityAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->IsNetAuthority()) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	CommittedCooldown.Invalidate();
	const FGameplayEffectSpecHandle Immunity = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo,
		ImmunityEffectClass, GetAbilityLevel(Handle, ActorInfo));
	if (!Immunity.IsValid() || !FMath::IsFinite(Immunity.Data->GetDuration()) || Immunity.Data->GetDuration() <= 0
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const FActiveGameplayEffectHandle Effect = CommittedCooldown.IsValid()
		? ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Immunity) : FActiveGameplayEffectHandle();
	if (!Effect.IsValid())
	{
		// A rejected GE application must not consume this action's cooldown or another source's effect.
		if (CommittedCooldown.IsValid()) { ASC->RemoveActiveGameplayEffect(CommittedCooldown); }
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (HALAbilities::ShouldLog())
	{
		UE_LOG(LogHALAbility, Log, TEXT("Ability accepted: Owner=%s Avatar=%s Ability=%s Immunity=%.2fs Cooldown=%.2fs"),
			*GetNameSafe(ActorInfo->OwnerActor.Get()), *GetNameSafe(ActorInfo->AvatarActor.Get()), *GetName(),
			Immunity.Data->GetDuration(), CastChecked<UHALAbilitySystemComponent>(ASC)->GetEffectTimeRemaining(HALGameplayTags::Cooldown_ImpactImmunity));
	}
	ActorInfo->OwnerActor->ForceNetUpdate();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
