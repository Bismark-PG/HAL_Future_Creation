#pragma once

#include "Engine/DataAsset.h"
#include "HALImpactTypes.h"
#include "HALVehicleUltimateAbility.h"
#include "HALWaterCannonAbility.generated.h"

class AHALWaterCannon;
class ATestVehiclePawn;
class UMaterialInterface;
class USoundBase;

UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALWaterCannonConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Jet", meta=(Units="s", ClampMin=".1", ClampMax="30")) float Duration = 3;
	UPROPERTY(EditAnywhere, Category="Jet", meta=(Units="cm", ClampMin="50", ClampMax="3000")) float Range = 800;
	/** Full horizontal opening angle. */
	UPROPERTY(EditAnywhere, Category="Jet", meta=(Units="deg", ClampMin="1", ClampMax="40")) float FanAngle = 12;
	UPROPERTY(EditAnywhere, Category="Jet", meta=(Units="cm", ClampMin="2", ClampMax="100")) float HalfHeight = 24;
	UPROPERTY(EditAnywhere, Category="Jet", meta=(Units="cm", ClampMin="1", ClampMax="50")) float NozzleRadius = 7;
	/** Reference-pose bone. Offset is in vehicle axes, in cm, measured from this bone. None uses vehicle origin. */
	UPROPERTY(EditAnywhere, Category="Nozzle") FName MountBone = TEXT("Chassis_jnt");
	UPROPERTY(EditAnywhere, Category="Nozzle", meta=(Units="cm")) FVector NozzleOffset = FVector(211, 0, 95);
	/** Downward pitch helps the roof-mounted stream reach cars and balls at deck height. */
	UPROPERTY(EditAnywhere, Category="Nozzle", meta=(Units="deg", ClampMin="-20", ClampMax="15")) float AimPitch = -5;
	UPROPERTY(EditAnywhere, Category="Hits", meta=(Units="s", ClampMin=".05", ClampMax="10")) float HitInterval = .5f;
	/** Multiplies the caster's captured attack before the recipient's normal damage modifiers. */
	UPROPERTY(EditAnywhere, Category="Hits", meta=(ClampMin=".01", ClampMax="10")) float DamageMultiplier = .20f;
	UPROPERTY(EditAnywhere, Category="Hits") EHALImpactTier ImpactTier = EHALImpactTier::Light;
	UPROPERTY(EditAnywhere, Category="Hits") EHALImpactPushPreset PushDistance = EHALImpactPushPreset::Short;
	UPROPERTY(EditAnywhere, Category="Hits", meta=(Units="cm/s", ClampMin="100", ClampMax="10000")) float BallLaunchSpeed = 2400;
	UPROPERTY(EditAnywhere, Category="Recoil", meta=(Units="cm/s^2", ClampMin="0", ClampMax="6000")) float RecoilAcceleration = 1050;
	UPROPERTY(EditAnywhere, Category="Recoil", meta=(ClampMin=".05", ClampMax="1")) float ForwardLimitScale = .55f;
	/** Reverse speed at which the jet stops adding its own acceleration. External momentum is preserved. */
	UPROPERTY(EditAnywhere, Category="Recoil", meta=(Units="cm/s", ClampMin="1", ClampMax="10000")) float ReverseLimit = 1400;
	UPROPERTY(EditAnywhere, Category="Water") TObjectPtr<UMaterialInterface> WaterMaterial;
	UPROPERTY(EditAnywhere, Category="Water") FLinearColor ShadowColor { .018f,.18f,.48f,1 };
	UPROPERTY(EditAnywhere, Category="Water") FLinearColor MidColor { .04f,.64f,.94f,1 };
	UPROPERTY(EditAnywhere, Category="Water") FLinearColor LightColor { .83f,.98f,1.25f,1 };
	UPROPERTY(EditAnywhere, Category="Water", meta=(ClampMin=".01", ClampMax=".4")) float ToneSoftness = .11f;
	/** Speed of the flowing highlights, independent of hit cadence. */
	UPROPERTY(EditAnywhere, Category="Water", meta=(Units="cm/s", ClampMin="10", ClampMax="6000")) float FlowSpeed = 1700;
	/** Visual travel time from the nozzle to maximum range. */
	UPROPERTY(EditAnywhere, Category="Water", meta=(Units="s", ClampMin=".03", ClampMax="1")) float SprayInSeconds = .19f;
	/** Time for the last water and foam to drain after emission stops. */
	UPROPERTY(EditAnywhere, Category="Water", meta=(Units="s", ClampMin=".03", ClampMax="1")) float SprayOutSeconds = .26f;
	/** Master size of the terminal foam and flying droplets. Zero disables both sprays. */
	UPROPERTY(EditAnywhere, Category="Water", meta=(Units="cm", ClampMin="0", ClampMax="120")) float SplashSize = 34;
	/** Size of the breaking crest and droplets where water reaches open air. */
	UPROPERTY(EditAnywhere, Category="Water", meta=(ClampMin="0", ClampMax="3")) float FreeSplashScale = 1.f;
	/** Size of the rebounding water sheets and droplets at vehicle and wall contacts. */
	UPROPERTY(EditAnywhere, Category="Water", meta=(ClampMin="0", ClampMax="3")) float ImpactSplashScale = 1.f;
	/** A mono looping Sound Wave or looping cue. */
	UPROPERTY(EditAnywhere, Category="Fire Siren") TObjectPtr<USoundBase> Siren;
	UPROPERTY(EditAnywhere, Category="Fire Siren", meta=(ClampMin="0", ClampMax="4")) float SirenVolume = .65f;
	UPROPERTY(EditAnywhere, Category="Fire Siren", meta=(Units="cm", ClampMin="1", ClampMax="5000")) float SirenInnerRadius = 450;
	UPROPERTY(EditAnywhere, Category="Fire Siren", meta=(Units="cm", ClampMin="1", ClampMax="20000")) float SirenFalloff = 2700;
	bool HasValidRules() const;
	bool ResolveNozzle(const ATestVehiclePawn& Vehicle, FVector& RootLocal) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

UCLASS()
class HAL_FUTURE_CREATION_API UHALWaterCannonAbility : public UHALVehicleUltimateAbility
{
	GENERATED_BODY()
public:
	UHALWaterCannonAbility();
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayTagContainer* Source, const FGameplayTagContainer* Target, FGameplayTagContainer* Failure) const override;
protected:
	virtual EHALUltimateStart StartUltimate(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, bool bReplicate, bool bCancelled) override;
private:
	UHALWaterCannonConfig* StartConfig(const FGameplayAbilityActorInfo* Info, FVector& LocalNozzle) const;
	UFUNCTION() void JetDestroyed(AActor* Actor);
	TWeakObjectPtr<AHALWaterCannon> ActiveJet;
};
