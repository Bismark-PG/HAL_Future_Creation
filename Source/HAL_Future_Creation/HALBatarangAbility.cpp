#include "HALBatarangAbility.h"
#include "ArcadeVehicleMovementComponent.h"
#include "HALAbilitySystemComponent.h"
#include "HALBatarang.h"
#include "HALGameFlowPermissions.h"
#include "HALGameplayTags.h"
#include "HALHookComponent.h"
#include "Misc/DataValidation.h"
#include "TestVehiclePawn.h"
#include "VehicleRecoveryComponent.h"

float UHALBatarangConfig::FusePulse(float RemainingSeconds) const
{
	const float Left = FMath::Clamp(RemainingSeconds, 0.f, FuseSeconds);
	const float Window = FMath::Max(UE_SMALL_NUMBER, FMath::Min(FuseSeconds, FuseWarningSeconds));
	const float IntoWarning = FMath::Max(0.f, Window - Left);
	// Integrate the changing rate so there is no jump at the start of the warning.
	const float Cycles = (FuseSeconds-Left)*FusePulseRate + .5f*(FinalFusePulseRate-FusePulseRate)*IntoWarning*IntoWarning/Window;
	return FMath::Square(.5f + .5f*FMath::Cos(UE_TWO_PI*Cycles));
}

bool UHALBatarangConfig::HasValidRules() const
{
	return FMath::IsWithinInclusive(ForwardReach,150.f,3000.f) && FMath::IsWithinInclusive(SideReach,80.f,1500.f)
		&& FMath::IsWithinInclusive(FlightSeconds,.25f,5.f) && FMath::IsWithinInclusive(SweepRadius,2.f,60.f)
		&& !LaunchOffset.ContainsNaN() && LaunchOffset.GetAbsMax() < 1000
		&& FMath::IsWithinInclusive(FuseSeconds,.2f,30.f) && FMath::IsWithinInclusive(DamageMultiplier,0.f,10.f)
		&& ImpactTier <= EHALImpactTier::Heavy && PushDistance <= EHALImpactPushPreset::ExtraLong
		&& FMath::IsWithinInclusive(WingSpan,15.f,150.f);
}
#if WITH_EDITOR
EDataValidationResult UHALBatarangConfig::IsDataValid(FDataValidationContext& Context) const
{
	if (!FMath::IsWithinInclusive(PinDepthPercent,0.f,45.f) || !FMath::IsWithinInclusive(BurstReach,80.f,480.f)
		|| !FMath::IsWithinInclusive(FusePulseRate,.25f,2.5f) || !FMath::IsWithinInclusive(FinalFusePulseRate,FusePulseRate,2.5f)
		|| !FMath::IsWithinInclusive(FuseWarningSeconds,.1f,5.f))
	{
		Context.AddError(FText::FromString(TEXT("Check pin depth, visual burst reach and rising fuse pulse rates.")));
		return EDataValidationResult::Invalid;
	}
	if (HasValidRules() && (BladeMesh || DartMaterial) && PinSignal) { return EDataValidationResult::Valid; }
	Context.AddError(FText::FromString(TEXT("Check batarang tuning, its blade mesh or fallback wing material, and the capture signal.")));
	return EDataValidationResult::Invalid;
}
#endif
UHALBatarangAbility::UHALBatarangAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	SetAssetTags(FGameplayTagContainer(HALGameplayTags::Ability_Batarang));
}
UHALBatarangConfig* UHALBatarangAbility::ConfigFor(const ATestVehiclePawn* Car)
{
	if (Car)
	{
		for (const auto& Grant : Car->GetVehicleAbilityConfig().Grants)
		{
			if (Grant.InputTag == HALGameplayTags::Input_Ability_Primary && Grant.Ability && Grant.Ability->IsChildOf(StaticClass()))
			{ return Cast<UHALBatarangConfig>(Grant.SkillConfig); }
		}
	}
	return nullptr;
}
bool UHALBatarangAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayTagContainer* Source, const FGameplayTagContainer* Target, FGameplayTagContainer* Failure) const
{
	if (!Super::CanActivateAbility(Handle,Info,Source,Target,Failure)) { return false; }
	const auto* Car = Info ? Cast<ATestVehiclePawn>(Info->AvatarActor.Get()) : nullptr;
	const auto* Config = ConfigFor(Car);
	const auto* ASC = Info ? Cast<UHALAbilitySystemComponent>(Info->AbilitySystemComponent.Get()) : nullptr;
	if (Config && Config->HasValidRules() && !Car->GetActiveBatarang() && ASC && ASC->GetAvatarActor() == Car
		&& ASC->IsCombatInitialized() && ASC->IsImpactInitialized() && !ASC->IsStunned()
		&& Car->CanReceiveSkillEffects() && HALGameFlow::Permissions(Car->GetWorld(),Car).bAbilities && !UHALHookComponent::BlocksActionsOn(Car))
	{
		const auto* Move = Car->FindComponentByClass<UArcadeVehicleMovementComponent>();
		const auto* Recovery = Car->FindComponentByClass<UVehicleRecoveryComponent>();
		const auto Pose = Move ? Move->GetStabilityObservation() : FVehicleStabilityObservation{};
		if (Pose.bValid && Pose.bGrounded && Pose.bDrivingAllowed && Pose.Up.Z > .35 && !Pose.Recovery.bActive
			&& !(Recovery && Recovery->IsExecuting())) { return true; }
	}
	if (Failure) { Failure->AddTag(HALGameplayTags::Ability_Fail_VehicleState); }
	return false;
}
EHALUltimateStart UHALBatarangAbility::StartUltimate(FGameplayAbilitySpecHandle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo, const FGameplayEventData*)
{
	auto* Car = Cast<ATestVehiclePawn>(Info->AvatarActor.Get());
	auto* Config = ConfigFor(Car);
	if (!Config || Car->GetActiveBatarang()) { return EHALUltimateStart::Failed; }
	FActorSpawnParameters Params;
	Params.Owner = Car;
	auto* Dart = Car->GetWorld()->SpawnActor<AHALBatarang>(Params);
	if (!Dart || !Dart->Start(*Config,Car->BatarangThrowMirror))
	{
		if (Dart) { Dart->Destroy(); }
		return EHALUltimateStart::Failed;
	}
	ActiveDart = Dart;
	Dart->OnDestroyed.AddDynamic(this,&ThisClass::DartDestroyed);
	return EHALUltimateStart::Active;
}
void UHALBatarangAbility::DartDestroyed(AActor* Actor)
{
	if (ActiveDart.Get(true) == Actor)
	{
		ActiveDart.Reset();
		EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,true,false);
	}
}
void UHALBatarangAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, bool bReplicate, bool bCancelled)
{
	if (auto* Dart = ActiveDart.Get())
	{
		ActiveDart.Reset();
		Dart->OnDestroyed.RemoveDynamic(this,&ThisClass::DartDestroyed);
		Dart->Destroy();
	}
	Super::EndAbility(Handle,Info,Activation,bReplicate,bCancelled);
}
