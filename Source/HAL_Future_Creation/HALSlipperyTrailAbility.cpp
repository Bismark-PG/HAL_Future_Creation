#include "HALSlipperyTrailAbility.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "HALAbilitySystemComponent.h"
#include "HALAbilityTypes.h"
#include "HALGameFlowPermissions.h"
#include "HALGameplayTags.h"
#include "Misc/DataValidation.h"
#include "TestVehiclePawn.h"
#include "VehicleHealthComponent.h"

UHALSlipperyTrailConfig::UHALSlipperyTrailConfig()
{
	Handling.Sway.Amplitude = 5.f;
}

bool UHALSlipperyTrailConfig::Validate(FString& Error) const
{
	const auto InRange = [](float Value, float Min, float Max) { return Value >= Min && Value <= Max; };
	if (!InRange(Emission.EmissionSeconds, .05f, 29.f)
		|| !InRange(Shape.LifetimeSeconds, .1f, 30.f) || Shape.LifetimeSeconds <= Emission.EmissionSeconds)
	{
		Error = TEXT("Use finite timing values in the editor ranges. Lifetime must exceed Emission Seconds.");
		return false;
	}
	if (!InRange(Emission.UpdateInterval, .02f, .2f) || !InRange(Emission.SampleSpacing, 10, 250)
		|| !InRange(Emission.MaximumLinkDistance, Emission.SampleSpacing, 1500)
		|| !InRange(Emission.MaximumPoints, 1, 256) || !InRange(Emission.TailOffset, 0, 300)
		|| !InRange(Emission.GroundProbeDepth, 20, 350) || !InRange(Emission.MinimumGroundNormalZ, .1f, 1.f)
		|| !InRange(Shape.GroundHeightTolerance, 5, 200))
	{
		Error = TEXT("Use finite ground/sampling values in the editor ranges. Maximum Link Distance must be at least Sample Spacing.");
		return false;
	}
	if (!InRange(Shape.InitialHalfWidth, 20, 1000) || !InRange(Shape.MaximumHalfWidth, Shape.InitialHalfWidth, 1200)
		|| !InRange(Shape.ExpansionSeconds, .05f, 30.f) || !InRange(Shape.OutlineLift, 0, 30)
		|| !InRange(Shape.OutlineThickness, .5f, 10.f) || !InRange(Handling.LateralGripMultiplier, 0, 1)
		|| !InRange(Handling.RecoverySeconds, 0, 10) || !Handling.Sway.IsValid())
	{
		Error = TEXT("Use finite shape/handling/outline values in the editor ranges. Maximum Half Width must be at least Initial Half Width.");
		return false;
	}
	if (!InRange(Presentation.DissolveSeconds, 0, 5))
	{
		Error = TEXT("Presentation Dissolve Seconds must be finite and between 0 and 5 seconds.");
		return false;
	}
	Error.Reset();
	return true;
}

#if WITH_EDITOR
EDataValidationResult UHALSlipperyTrailConfig::IsDataValid(FDataValidationContext& Context) const
{
	FString Error;
	if (!Validate(Error))
	{
		Context.AddError(FText::FromString(Error));
		return EDataValidationResult::Invalid;
	}
	if (Presentation.GroundMaterial && !Presentation.HasUsableMaterial())
	{
		Context.AddError(FText::FromString(TEXT("Trail Ground Material must use the Surface domain. Use Ground Lift 0.1–20 cm, Texture Tile Size 20–2000 cm and valid Deformation settings (even Width Subdivisions, Wave Amplitude at or below Bulge Height).")));
		return EDataValidationResult::Invalid;
	}
	return EDataValidationResult::Valid;
}
#endif

UHALSlipperyTrailCooldownEffect::UHALSlipperyTrailCooldownEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(1.f);
	FInheritedTagContainer Tags;
	Tags.AddTag(HALGameplayTags::Cooldown_SlipperyTrail);
	auto* Granted = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("SlipperyTrailCooldown"));
	GEComponents.Add(Granted);
	Granted->SetAndApplyTargetTagChanges(Tags);
}

UHALSlipperyTrailAbility::UHALSlipperyTrailAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	SetAssetTags(FGameplayTagContainer(HALGameplayTags::Ability_SlipperyTrail));
}

const UHALSlipperyTrailConfig* UHALSlipperyTrailAbility::GetSkillConfig() const
{
	return GetClass()->GetDefaultObject<UHALSlipperyTrailAbility>()->SkillConfig;
}

bool UHALSlipperyTrailAbility::ResolveStart(const FGameplayAbilityActorInfo* ActorInfo,
	const UHALSlipperyTrailConfig* Config, FVector& Ground, FVector* GroundNormal) const
{
	const auto* Vehicle = ActorInfo ? Cast<ATestVehiclePawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	const auto* ASC = ActorInfo ? Cast<UHALAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	const auto* Health = Vehicle ? Vehicle->FindComponentByClass<UVehicleHealthComponent>() : nullptr;
	FString Error;
	if (!Config || !Config->Validate(Error))
	{
		UE_CLOG(HALAbilities::ShouldLog(), LogHALAbility, Log, TEXT("SlipperyTrail invalid Skill Config %s: %s"),
			*GetNameSafe(Config), *Error);
		return false;
	}
	return Vehicle && ASC && ASC->GetAvatarActor() == Vehicle && ASC->IsHealthInitialized() && !ASC->IsStunned()
		&& Vehicle->IsConfigurationValid() && Vehicle->GetController() && Vehicle->GetController()->GetPawn() == Vehicle
		&& Health && Health->IsHealthReady() && Health->GetCurrentHP() > 0
		&& HALGameFlow::Permissions(Vehicle->GetWorld(), Vehicle).bAbilities
		&& AHALSlipperyTrail::FindTailGround(*Vehicle, Config->Emission, Config->Shape, Ground, GroundNormal);
}

bool UHALSlipperyTrailAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) { return false; }
	FVector Ground;
	const bool bReady = !ActiveTrail.IsValid() && ResolveStart(ActorInfo, GetSkillConfig(), Ground);
	if (!bReady && OptionalRelevantTags) { OptionalRelevantTags->AddTag(HALGameplayTags::Ability_Fail_VehicleState); }
	return bReady;
}


EHALUltimateStart UHALSlipperyTrailAbility::StartUltimate(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData*)
{
	const auto* Config = GetClass()->GetDefaultObject<UHALSlipperyTrailAbility>()->SkillConfig.Get();
	FVector Ground;
	FVector GroundNormal;
	if (ActiveTrail.IsValid() || !Config || !ResolveStart(Info, Config, Ground, &GroundNormal)) { return EHALUltimateStart::Failed; }
	FActorSpawnParameters Params;
	Params.Owner = Info->AvatarActor.Get();
	auto* Actor = Params.Owner->GetWorld()->SpawnActor<AHALSlipperyTrail>(Params);
	if (!Actor || !Actor->Start(Config->Shape, Config->Emission, Config->Handling, Ground, GroundNormal, Config->Presentation))
	{
		if (Actor) { Actor->Destroy(); }
		return EHALUltimateStart::Failed;
	}
	ActiveTrail = Actor;
	Actor->OnDestroyed.AddDynamic(this, &ThisClass::TrailDestroyed);
	Actor->OnGameplayEnded.AddUObject(this, &ThisClass::TrailGameplayEnded);
	return EHALUltimateStart::Active;
}

void UHALSlipperyTrailAbility::TrailDestroyed(AActor* Actor)
{
	if (ActiveTrail.Get(true) != Actor) { return; }
	ActiveTrail.Reset();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UHALSlipperyTrailAbility::TrailGameplayEnded(AHALSlipperyTrail* Trail)
{
	if (ActiveTrail.Get() != Trail) { return; }
	ActiveTrail.Reset();
	Trail->OnDestroyed.RemoveDynamic(this, &ThisClass::TrailDestroyed);
	Trail->OnGameplayEnded.RemoveAll(this);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UHALSlipperyTrailAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (auto* Trail = ActiveTrail.Get())
	{
		ActiveTrail.Reset();
		Trail->OnDestroyed.RemoveDynamic(this, &ThisClass::TrailDestroyed);
		Trail->OnGameplayEnded.RemoveAll(this);
		Trail->Destroy();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
