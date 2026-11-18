#include "HALBattleTopAbility.h"

#include "HALAbilitySystemComponent.h"
#include "HALAbilityTypes.h"
#include "HALBattleTopComponent.h"
#include "HALGameplayTags.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Misc/DataValidation.h"
#include "TestVehiclePawn.h"

bool UHALBattleTopConfig::Validate(FString& Error) const
{
	const auto InRange = [](float Value, float Min, float Max)
	{ return FMath::IsFinite(Value) && Value >= Min && Value <= Max; };
	if (InRange(DurationSeconds, .1f, 30.f) && InRange(CooldownSeconds, 0, 120)
		&& InRange(TargetCooldownSeconds,.05f,10) && InRange(SpinEnterSeconds,0,2) && InRange(SpinExitSeconds,0,2) && FX.IsValid()
		&& InRange(ContactDamage, 0, 100) && HALImpact::IsValidTier(ContactTier) && ContactTier != EHALImpactTier::None
		&& HALImpact::IsValidPushPreset(ContactPush) && InRange(WallReboundScale, 0, 2)
		&& InRange(WallMinimumIncomingSpeed, 1, 6000) && InRange(WallMaximumReboundSpeed, 1, 6000)
		&& InRange(WallContactRearmSeconds, .05f, 1) && InRange(MaximumBallSpeed, 1, 12000)
		&& InRange(HeldBallSpeed, 1, MaximumBallSpeed) && InRange(FreeBallSpeed, 1, MaximumBallSpeed))
	{ Error.Reset(); return true; }
	Error = TEXT("Battle Top requires finite tuning in the editor ranges, a nonzero contact tier and both ball speeds at or below Maximum Ball Speed.");
	return false;
}

#if WITH_EDITOR
EDataValidationResult UHALBattleTopConfig::IsDataValid(FDataValidationContext& Context) const
{
	FString Error;
	if (Validate(Error)) { return EDataValidationResult::Valid; }
	Context.AddError(FText::FromString(Error));
	return EDataValidationResult::Invalid;
}
#endif

UHALBattleTopEffect::UHALBattleTopEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(1.f); // Runtime spec uses the skill DA's exact duration.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::NeverRefresh;
	bDenyOverflowApplication = true;
	FInheritedTagContainer Active;
	Active.AddTag(HALGameplayTags::State_BattleTop);
	Active.AddTag(HALGameplayTags::State_Visual_BattleTop);
	auto* Granted = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("BattleTopState"));
	GEComponents.Add(Granted);
	Granted->SetAndApplyTargetTagChanges(Active);

	FInheritedTagContainer Scope;
	Scope.AddTag(HALGameplayTags::Effect_Scope_Avatar);
	auto* Asset = CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("BattleTopScope"));
	GEComponents.Add(Asset);
	Asset->SetAndApplyAssetTagChanges(Scope);
}

UHALBattleTopCooldownEffect::UHALBattleTopCooldownEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(1.f);
	FInheritedTagContainer Tags;
	Tags.AddTag(HALGameplayTags::Cooldown_BattleTop);
	auto* Granted = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("BattleTopCooldown"));
	GEComponents.Add(Granted);
	Granted->SetAndApplyTargetTagChanges(Tags);
}

UHALBattleTopAbility::UHALBattleTopAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	CooldownGameplayEffectClass = UHALBattleTopCooldownEffect::StaticClass();
	SetAssetTags(FGameplayTagContainer(HALGameplayTags::Ability_BattleTop));
}

UHALBattleTopConfig* UHALBattleTopAbility::GetSkillConfig() const
{
	return GetClass()->GetDefaultObject<UHALBattleTopAbility>()->SkillConfig;
}

bool UHALBattleTopAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) { return false; }
	const auto* Vehicle = ActorInfo ? Cast<ATestVehiclePawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	const auto* Top = Vehicle ? Vehicle->FindComponentByClass<UHALBattleTopComponent>() : nullptr;
	const auto* Config = GetSkillConfig();
	FString Error;
	const bool bConfigured = Config && Config->Validate(Error);
	const bool bReady = bConfigured && Top && Top->CanStart();
	UE_CLOG(!bReady && HALAbilities::ShouldLog(), LogHALAbility, Log,
		TEXT("BattleTop rejected Avatar=%s Config=%s ConfigValid=%d StateReady=%d %s"),
		*GetNameSafe(Vehicle), *GetNameSafe(Config), bConfigured, Top && Top->CanStart(), *Error);
	if (!bReady && OptionalRelevantTags) { OptionalRelevantTags->AddTag(HALGameplayTags::Ability_Fail_VehicleState); }
	return bReady;
}

void UHALBattleTopAbility::ApplyCooldown(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo) const
{
	const auto* Config = GetSkillConfig();
	if (!Config || Config->CooldownSeconds <= 0) { return; }
	const auto Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo,
		UHALBattleTopCooldownEffect::StaticClass(), GetAbilityLevel(Handle, ActorInfo));
	if (!Spec.IsValid()) { return; }
	Spec.Data->SetDuration(Config->CooldownSeconds, true);
	CommittedCooldown = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

void UHALBattleTopAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData*)
{
	auto* Vehicle = ActorInfo ? Cast<ATestVehiclePawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	auto* ASC = ActorInfo ? Cast<UHALAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	auto* Top = Vehicle ? Vehicle->FindComponentByClass<UHALBattleTopComponent>() : nullptr;
	auto* Config = GetSkillConfig();
	FString Error;
	if (!ActorInfo || !ActorInfo->IsNetAuthority() || !ASC || !Top || !Top->CanStart() || !Config || !Config->Validate(Error))
	{ EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }

	const auto Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo,
		UHALBattleTopEffect::StaticClass(), GetAbilityLevel(Handle, ActorInfo));
	if (!Spec.IsValid()) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	Spec.Data->SetDuration(Config->DurationSeconds, true);
	CommittedCooldown.Invalidate();
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{ EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }

	const auto Effect = Config->CooldownSeconds <= 0 || CommittedCooldown.IsValid()
		? ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec) : FActiveGameplayEffectHandle();
	const bool bStarted = Effect.IsValid() && Top->Start(*ASC, *Config, Effect);
	if (!bStarted)
	{
		if (Effect.IsValid()) { ASC->RemoveActiveGameplayEffect(Effect); }
		if (CommittedCooldown.IsValid()) { ASC->RemoveActiveGameplayEffect(CommittedCooldown); }
	}
	ASC->GetOwner()->ForceNetUpdate();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !bStarted);
}
