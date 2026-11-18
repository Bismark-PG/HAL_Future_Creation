#include "HALTornadoAbility.h"
#include "HALTornado.h"
#include "HALAbilitySystemComponent.h"
#include "HALGameFlowPermissions.h"
#include "HALGameplayTags.h"
#include "TestVehiclePawn.h"
#include "VehicleHealthComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Misc/DataValidation.h"

bool UHALTornadoConfig::Validate(FString& Error) const
{
	if (FMath::IsFinite(CooldownSeconds) && CooldownSeconds >= 0 && CooldownSeconds <= 120 && Rules.IsValid()) { Error.Reset(); return true; }
	Error = TEXT("Tornado needs finite rules within the documented editor ranges."); return false;
}
#if WITH_EDITOR
EDataValidationResult UHALTornadoConfig::IsDataValid(FDataValidationContext& Context) const
{
	FString Error;
	if (Validate(Error) && FX.IsValid()) { return EDataValidationResult::Valid; }
	Context.AddError(FText::FromString(Error.IsEmpty() ? TEXT("Tornado FX values exceed their editor ranges.") : Error));
	return EDataValidationResult::Invalid;
}
#endif
UHALTornadoCooldownEffect::UHALTornadoCooldownEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration; DurationMagnitude = FScalableFloat(1);
	FInheritedTagContainer Tags; Tags.AddTag(HALGameplayTags::Cooldown_Tornado);
	auto* Granted = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TornadoCooldown"));
	GEComponents.Add(Granted); Granted->SetAndApplyTargetTagChanges(Tags);
}
UHALTornadoAbility::UHALTornadoAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	CooldownGameplayEffectClass = UHALTornadoCooldownEffect::StaticClass();
	SetAssetTags(FGameplayTagContainer(HALGameplayTags::Ability_Tornado));
}
bool UHALTornadoAbility::ResolveStart(const FGameplayAbilityActorInfo* Info, FVector& Ground) const
{
	const auto* Config = GetClass()->GetDefaultObject<UHALTornadoAbility>()->SkillConfig.Get();
	const auto* Vehicle = Info ? Cast<ATestVehiclePawn>(Info->AvatarActor.Get()) : nullptr;
	const auto* ASC = Info ? Cast<UHALAbilitySystemComponent>(Info->AbilitySystemComponent.Get()) : nullptr;
	const auto* Health = Vehicle ? Vehicle->FindComponentByClass<UVehicleHealthComponent>() : nullptr;
	FString Error;
	return Config && Config->Validate(Error) && Vehicle && ASC && ASC->GetAvatarActor() == Vehicle
		&& ASC->IsCombatInitialized() && ASC->IsImpactInitialized() && ASC->IsHealthInitialized() && !ASC->IsStunned()
		&& Health && Health->IsHealthReady() && Health->GetCurrentHP() > 0 && Vehicle->IsConfigurationValid()
		&& Vehicle->GetController() && Vehicle->GetController()->GetPawn() == Vehicle
		&& HALGameFlow::Permissions(Vehicle->GetWorld(), Vehicle).bAbilities
		&& AHALTornado::FindStart(*Vehicle, Config->Rules, Ground);
}
bool UHALTornadoAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayTagContainer* Source, const FGameplayTagContainer* Target, FGameplayTagContainer* Failure) const
{
	if (!Super::CanActivateAbility(Handle, Info, Source, Target, Failure)) { return false; }
	FVector Ground; const bool Ready = !ActiveTornado.IsValid() && ResolveStart(Info, Ground);
	if (!Ready && Failure) { Failure->AddTag(HALGameplayTags::Ability_Fail_VehicleState); } return Ready;
}
void UHALTornadoAbility::ApplyCooldown(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, FGameplayAbilityActivationInfo Activation) const
{
	const auto* Config = GetClass()->GetDefaultObject<UHALTornadoAbility>()->SkillConfig.Get();
	if (!Config || Config->CooldownSeconds <= 0) { return; }
	const auto Spec = MakeOutgoingGameplayEffectSpec(Handle, Info, Activation, UHALTornadoCooldownEffect::StaticClass(), GetAbilityLevel(Handle, Info));
	if (!Spec.IsValid()) { return; } Spec.Data->SetDuration(Config->CooldownSeconds, true);
	CommittedCooldown = ApplyGameplayEffectSpecToOwner(Handle, Info, Activation, Spec);
}
void UHALTornadoAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData*)
{
	FVector Ground; CommittedCooldown.Invalidate();
	if (!Info || !Info->IsNetAuthority() || ActiveTornado.IsValid() || !ResolveStart(Info, Ground) || !CommitAbility(Handle, Info, Activation))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	const auto* Config = GetClass()->GetDefaultObject<UHALTornadoAbility>()->SkillConfig.Get();
	AHALTornado* Wind = nullptr;
	if (Config->CooldownSeconds <= 0 || CommittedCooldown.IsValid())
	{
		FActorSpawnParameters Params; Params.Owner = Info->AvatarActor.Get();
		Wind = Params.Owner->GetWorld()->SpawnActor<AHALTornado>(Params);
	}
	if (!Wind || !Wind->Start(*Config, Ground))
	{
		if (Wind) { Wind->Destroy(); }
		if (CommittedCooldown.IsValid()) { Info->AbilitySystemComponent->RemoveActiveGameplayEffect(CommittedCooldown); }
		EndAbility(Handle, Info, Activation, true, true); return;
	}
	ActiveTornado = Wind; Wind->OnDestroyed.AddDynamic(this, &ThisClass::TornadoDestroyed);
	Wind->OnGameplayEnded.AddUObject(this, &ThisClass::GameplayEnded);
}
void UHALTornadoAbility::TornadoDestroyed(AActor* Actor)
{
	if (ActiveTornado.Get(true) != Actor) { return; } ActiveTornado.Reset();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
void UHALTornadoAbility::GameplayEnded(AHALTornado* Wind)
{
	if (ActiveTornado.Get() != Wind) { return; } ActiveTornado.Reset();
	Wind->OnDestroyed.RemoveDynamic(this, &ThisClass::TornadoDestroyed); Wind->OnGameplayEnded.RemoveAll(this);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
void UHALTornadoAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, bool bReplicate, bool bCancelled)
{
	if (auto* Wind = ActiveTornado.Get())
	{
		ActiveTornado.Reset(); Wind->OnDestroyed.RemoveDynamic(this, &ThisClass::TornadoDestroyed);
		Wind->OnGameplayEnded.RemoveAll(this); Wind->Destroy();
	}
	Super::EndAbility(Handle, Info, Activation, bReplicate, bCancelled);
}
