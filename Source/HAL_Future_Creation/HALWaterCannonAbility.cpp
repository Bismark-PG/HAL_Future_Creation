#include "HALWaterCannonAbility.h"

#include "ArcadeVehicleMovementComponent.h"
#include "Engine/SkeletalMesh.h"
#include "HALAbilitySystemComponent.h"
#include "HALGameFlowPermissions.h"
#include "HALGameplayTags.h"
#include "HALHookComponent.h"
#include "HALVehiclePresentationConfig.h"
#include "HALWaterCannon.h"
#include "Misc/DataValidation.h"
#include "Sound/SoundBase.h"
#include "TestVehiclePawn.h"
#include "VehicleDefinition.h"
#include "VehicleRecoveryComponent.h"

bool UHALWaterCannonConfig::HasValidRules() const
{
	return FMath::IsWithinInclusive(Duration,.1f,30.f) && FMath::IsWithinInclusive(Range,50.f,3000.f)
		&& FMath::IsWithinInclusive(FanAngle,1.f,40.f) && FMath::IsWithinInclusive(HalfHeight,2.f,100.f)
		&& FMath::IsWithinInclusive(NozzleRadius,1.f,50.f) && FMath::IsWithinInclusive(AimPitch,-20.f,15.f)
		&& !NozzleOffset.ContainsNaN() && NozzleOffset.GetAbsMax() <= 2000
		&& FMath::IsWithinInclusive(HitInterval,.05f,10.f) && FMath::IsWithinInclusive(DamageMultiplier,.01f,10.f)
		&& ImpactTier >= EHALImpactTier::Light && ImpactTier <= EHALImpactTier::Heavy && PushDistance <= EHALImpactPushPreset::ExtraLong
		&& FMath::IsWithinInclusive(BallLaunchSpeed,100.f,10000.f) && FMath::IsWithinInclusive(RecoilAcceleration,0.f,6000.f)
		&& FMath::IsWithinInclusive(ForwardLimitScale,.05f,1.f) && FMath::IsWithinInclusive(ReverseLimit,1.f,10000.f);
}
bool UHALWaterCannonConfig::ResolveNozzle(const ATestVehiclePawn& Vehicle, FVector& RootLocal) const
{
	RootLocal = NozzleOffset;
	if (MountBone.IsNone()) { return true; }
	const auto* Def = Vehicle.GetVehicleDefinition();
	const auto* Look = Def ? Def->Presentation.Get() : nullptr;
	if (!Look || !Look->Mesh) { return false; }
	const auto& Ref = Look->Mesh->GetRefSkeleton();
	const int32 Bone = Ref.FindBoneIndex(MountBone);
	if (Bone == INDEX_NONE) { return false; }
	// Fixed reference pose for the authority. Camera distance and animation LOD cannot move the hit volume.
	RootLocal += Look->RelativeTransform.TransformPosition(Ref.GetBoneAbsoluteTransform(Bone).GetLocation()) * Vehicle.GetActorScale3D();
	return !RootLocal.ContainsNaN();
}
#if WITH_EDITOR
EDataValidationResult UHALWaterCannonConfig::IsDataValid(FDataValidationContext& Context) const
{
	bool bColorsValid = true;
	for (const auto& Color : { ShadowColor, MidColor, LightColor })
	{
		bColorsValid &= FMath::IsWithinInclusive(Color.R,0.f,16.f) && FMath::IsWithinInclusive(Color.G,0.f,16.f) && FMath::IsWithinInclusive(Color.B,0.f,16.f);
	}
	if (HasValidRules() && WaterMaterial && Siren && Siren->IsLooping() && bColorsValid && FMath::IsWithinInclusive(ToneSoftness,.01f,.4f)
		&& FMath::IsWithinInclusive(FlowSpeed,10.f,6000.f) && FMath::IsWithinInclusive(SplashSize,0.f,120.f)
		&& FMath::IsWithinInclusive(SirenVolume,0.f,4.f) && FMath::IsWithinInclusive(SirenInnerRadius,1.f,5000.f)
		&& FMath::IsWithinInclusive(SirenFalloff,1.f,20000.f)) { return EDataValidationResult::Valid; }
	Context.AddError(FText::FromString(TEXT("Water Cannon needs valid rules, presentation ranges, a water material and a looping siren.")));
	return EDataValidationResult::Invalid;
}
#endif
UHALWaterCannonAbility::UHALWaterCannonAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	SetAssetTags(FGameplayTagContainer(HALGameplayTags::Ability_WaterCannon));
}
UHALWaterCannonConfig* UHALWaterCannonAbility::StartConfig(const FGameplayAbilityActorInfo* Info, FVector& LocalNozzle) const
{
	const auto* Car = Info ? Cast<ATestVehiclePawn>(Info->AvatarActor.Get()) : nullptr;
	const auto* ASC = Info ? Cast<UHALAbilitySystemComponent>(Info->AbilitySystemComponent.Get()) : nullptr;
	if (!Car || !ASC || ASC->GetAvatarActor() != Car || ASC->IsStunned() || !ASC->IsCombatInitialized() || !ASC->IsImpactInitialized()
		|| !Car->CanReceiveSkillEffects() || !HALGameFlow::Permissions(Car->GetWorld(),Car).bAbilities || UHALHookComponent::BlocksActionsOn(Car)) { return nullptr; }
	const auto* Movement = Car->FindComponentByClass<UArcadeVehicleMovementComponent>();
	const auto* Recovery = Car->FindComponentByClass<UVehicleRecoveryComponent>();
	const auto Pose = Movement ? Movement->GetStabilityObservation() : FVehicleStabilityObservation{};
	if (!Pose.bValid || !Pose.bGrounded || !Pose.bDrivingAllowed || Pose.Up.Z <= .35 || Pose.Recovery.bActive || (Recovery && Recovery->IsExecuting())) { return nullptr; }
	for (const auto& Grant : Car->GetVehicleAbilityConfig().Grants)
	{
		if (Grant.Ability != GetClass()) { continue; }
		auto* C = Cast<UHALWaterCannonConfig>(Grant.SkillConfig);
		if (C && C->HasValidRules() && C->ResolveNozzle(*Car,LocalNozzle)) { return C; }
	}
	return nullptr;
}
bool UHALWaterCannonAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayTagContainer* Source, const FGameplayTagContainer* Target, FGameplayTagContainer* Failure) const
{
	if (!Super::CanActivateAbility(Handle,Info,Source,Target,Failure)) { return false; }
	FVector Nozzle;
	if (!ActiveJet.IsValid() && StartConfig(Info,Nozzle)) { return true; }
	if (Failure) { Failure->AddTag(HALGameplayTags::Ability_Fail_VehicleState); }
	return false;
}
EHALUltimateStart UHALWaterCannonAbility::StartUltimate(FGameplayAbilitySpecHandle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo, const FGameplayEventData*)
{
	FVector Nozzle;
	auto* C = StartConfig(Info,Nozzle);
	if (!C || ActiveJet.IsValid()) { return EHALUltimateStart::Failed; }
	FActorSpawnParameters Spawn;
	Spawn.Owner = Info->AvatarActor.Get();
	auto* Jet = Spawn.Owner->GetWorld()->SpawnActor<AHALWaterCannon>(Spawn);
	if (!Jet || !Jet->Start(*C, Nozzle))
	{
		if (Jet) { Jet->Destroy(); }
		return EHALUltimateStart::Failed;
	}
	ActiveJet = Jet;
	Jet->OnDestroyed.AddDynamic(this,&ThisClass::JetDestroyed);
	return EHALUltimateStart::Active;
}
void UHALWaterCannonAbility::JetDestroyed(AActor* Actor)
{
	if (ActiveJet.Get(true) != Actor) { return; }
	ActiveJet.Reset();
	EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,true,false);
}
void UHALWaterCannonAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo ActivationInfo, bool bReplicate, bool bCancelled)
{
	if (auto* Jet = ActiveJet.Get())
	{
		ActiveJet.Reset();
		Jet->OnDestroyed.RemoveDynamic(this,&ThisClass::JetDestroyed);
		Jet->Destroy();
	}
	Super::EndAbility(Handle,Info,ActivationInfo,bReplicate,bCancelled);
}
