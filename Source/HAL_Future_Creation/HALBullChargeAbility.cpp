#include "HALBullChargeAbility.h"
#include "Curves/CurveFloat.h"

#include "HALAbilitySystemComponent.h"
#include "HALAbilityTypes.h"
#include "HALBullChargeComponent.h"
#include "HALGameplayTags.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Misc/DataValidation.h"
#include "TestVehiclePawn.h"

bool UHALBullChargeConfig::Validate(FString& Error) const
{
	const auto InRange = [](float Value, float Min, float Max)
	{ return FMath::IsFinite(Value) && Value >= Min && Value <= Max; };
	if (Motion.IsValidTuning() && InRange(CooldownSeconds, 0, 120) && InRange(TargetCooldownSeconds, .05f, 10)
		&& InRange(FrontHalfAngle, 1, 89) && InRange(MinimumForwardSpeed, 1, 10000)
		&& InRange(MinimumClosingSpeed, 1, 10000) && InRange(FullDamageSpeed, MinimumForwardSpeed + 1, 12000)
		&& InRange(MinimumDamageMultiplier, 0, 10) && InRange(MaximumDamageMultiplier, MinimumDamageMultiplier, 10)
		&& HALImpact::IsValidTier(ContactTier) && HALImpact::IsValidPushPreset(ContactPush)
		&& InRange(BaseBallSpeed, 1, 12000) && InRange(BallInheritedSpeedFraction, 0, 2)
		&& InRange(MaximumBallSpeed, BaseBallSpeed, 12000)) { return true; }
	Error = TEXT("Bull Charge needs finite tuning within editor ranges, Ramp <= Duration, Full Damage Speed > Minimum Forward Speed and valid impact/ball limits.");
	return false;
}

bool UHALBullChargeConfig::Qualifies(const FVector& Forward, const FVector& ToContact, const FVector& ClosingNormal,
	const FVector& BeforeVelocity, const FVector& OtherBeforeVelocity, float& OutForwardSpeed) const
{
	const FVector Direction = Forward.GetSafeNormal2D();
	const FVector ContactDirection = ToContact.GetSafeNormal2D();
	OutForwardSpeed = FVector::DotProduct(BeforeVelocity, Direction);
	return !Direction.IsNearlyZero() && !ContactDirection.IsNearlyZero()
		&& OutForwardSpeed >= MinimumForwardSpeed
		&& FVector::DotProduct(Direction, ContactDirection) >= FMath::Cos(FMath::DegreesToRadians(FrontHalfAngle))
		&& FVector::DotProduct(BeforeVelocity - OtherBeforeVelocity, ClosingNormal.GetSafeNormal2D()) >= MinimumClosingSpeed;
}

float UHALBullChargeConfig::DamageMultiplier(float ForwardSpeed) const
{
	const float Alpha = FMath::Clamp((ForwardSpeed - MinimumForwardSpeed) / FMath::Max(1.f, FullDamageSpeed - MinimumForwardSpeed), 0.f, 1.f);
	const float Value = DamageMultiplierCurve ? DamageMultiplierCurve->GetFloatValue(Alpha)
		: FMath::Lerp(MinimumDamageMultiplier, MaximumDamageMultiplier, Alpha);
	return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, MaximumDamageMultiplier) : 0.f;
}

float UHALBullChargeConfig::BallSpeed(float ForwardSpeed) const
{
	return FMath::Min(MaximumBallSpeed, BaseBallSpeed + FMath::Max(0.f, ForwardSpeed) * BallInheritedSpeedFraction);
}
#if WITH_EDITOR
EDataValidationResult UHALBullChargeConfig::IsDataValid(FDataValidationContext& Context) const
{
	FString Error;
	if (Validate(Error)) { return EDataValidationResult::Valid; }
	Context.AddError(FText::FromString(Error));
	return EDataValidationResult::Invalid;
}
#endif

UHALBullChargeEffect::UHALBullChargeEffect()
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
	Active.AddTag(HALGameplayTags::State_BullCharge);
	Active.AddTag(HALGameplayTags::State_Visual_BullCharge);
	Active.AddTag(HALGameplayTags::State_Combat_ImpactImmune);
	auto* Granted = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("BullChargeState"));
	GEComponents.Add(Granted);
	Granted->SetAndApplyTargetTagChanges(Active);

	FInheritedTagContainer Scope;
	Scope.AddTag(HALGameplayTags::Effect_Scope_Avatar);
	auto* Asset = CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("BullChargeScope"));
	GEComponents.Add(Asset);
	Asset->SetAndApplyAssetTagChanges(Scope);
}

UHALBullChargeCooldownEffect::UHALBullChargeCooldownEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(1.f);
	FInheritedTagContainer Tags;
	Tags.AddTag(HALGameplayTags::Cooldown_BullCharge);
	auto* Granted = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("BullChargeCooldown"));
	GEComponents.Add(Granted);
	Granted->SetAndApplyTargetTagChanges(Tags);
}

UHALBullChargeAbility::UHALBullChargeAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	CooldownGameplayEffectClass = UHALBullChargeCooldownEffect::StaticClass();
	SetAssetTags(FGameplayTagContainer(HALGameplayTags::Ability_BullCharge));
}

UHALBullChargeConfig* UHALBullChargeAbility::GetSkillConfig() const
{
	return GetClass()->GetDefaultObject<UHALBullChargeAbility>()->SkillConfig;
}

bool UHALBullChargeAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) { return false; }
	const auto* Vehicle = ActorInfo ? Cast<ATestVehiclePawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	const auto* Top = Vehicle ? Vehicle->FindComponentByClass<UHALBullChargeComponent>() : nullptr;
	const auto* Config = GetSkillConfig();
	FString Error;
	const bool bConfigured = Config && Config->Validate(Error);
	const bool bReady = bConfigured && Top && Top->CanStart();
	UE_CLOG(!bReady && HALAbilities::ShouldLog(), LogHALAbility, Log,
		TEXT("BullCharge rejected Avatar=%s Config=%s ConfigValid=%d StateReady=%d %s"),
		*GetNameSafe(Vehicle), *GetNameSafe(Config), bConfigured, Top && Top->CanStart(), *Error);
	if (!bReady && OptionalRelevantTags) { OptionalRelevantTags->AddTag(HALGameplayTags::Ability_Fail_VehicleState); }
	return bReady;
}

void UHALBullChargeAbility::ApplyCooldown(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo) const
{
	const auto* Config = GetSkillConfig();
	if (!Config || Config->CooldownSeconds <= 0) { return; }
	const auto Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo,
		UHALBullChargeCooldownEffect::StaticClass(), GetAbilityLevel(Handle, ActorInfo));
	if (!Spec.IsValid()) { return; }
	Spec.Data->SetDuration(Config->CooldownSeconds, true);
	CommittedCooldown = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

void UHALBullChargeAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData*)
{
	auto* Vehicle = ActorInfo ? Cast<ATestVehiclePawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	auto* ASC = ActorInfo ? Cast<UHALAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	auto* Top = Vehicle ? Vehicle->FindComponentByClass<UHALBullChargeComponent>() : nullptr;
	auto* Config = GetSkillConfig();
	FString Error;
	if (!ActorInfo || !ActorInfo->IsNetAuthority() || !ASC || !Top || !Top->CanStart() || !Config || !Config->Validate(Error))
	{ EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }

	const auto Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo,
		UHALBullChargeEffect::StaticClass(), GetAbilityLevel(Handle, ActorInfo));
	if (!Spec.IsValid()) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	Spec.Data->SetDuration(Config->Motion.DurationSeconds, true);
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
