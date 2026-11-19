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
	if (InRange(DurationSeconds, .1f, 30.f)
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


EHALUltimateStart UHALBattleTopAbility::StartUltimate(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData*)
{
	auto* Vehicle = Cast<ATestVehiclePawn>(Info->AvatarActor.Get());
	auto* ASC = CastChecked<UHALAbilitySystemComponent>(Info->AbilitySystemComponent.Get());
	auto* Component = Vehicle ? Vehicle->FindComponentByClass<UHALBattleTopComponent>() : nullptr;
	auto* Config = GetSkillConfig();
	FString Error;
	if (!Component || !Component->CanStart() || !Config || !Config->Validate(Error)) { return EHALUltimateStart::Failed; }
	const auto Spec = MakeOutgoingGameplayEffectSpec(Handle, Info, Activation, UHALBattleTopEffect::StaticClass(), GetAbilityLevel(Handle, Info));
	if (!Spec.IsValid()) { return EHALUltimateStart::Failed; }
	Spec.Data->SetDuration(Config->DurationSeconds, true);
	const auto Effect = ApplyGameplayEffectSpecToOwner(Handle, Info, Activation, Spec);
	if (!Effect.IsValid()) { return EHALUltimateStart::Failed; }
	if (Component->Start(*ASC, *Config, Effect)) { return EHALUltimateStart::Completed; }
	ASC->RemoveActiveGameplayEffect(Effect);
	return EHALUltimateStart::Failed;
}
