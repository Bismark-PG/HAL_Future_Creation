#pragma once

#include "Engine/DataAsset.h"
#include "HALImpactTypes.h"
#include "HALVehicleUltimateAbility.h"
#include "HALBatarangAbility.generated.h"

class AHALBatarang;
class ATestVehiclePawn;
class UMaterialInterface;
class USoundBase;
class UStaticMesh;

UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALBatarangConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	/** Maximum distance forward of the release point. The return finishes behind that point. */
	UPROPERTY(EditAnywhere, Category="Flight", meta=(Units="cm", ClampMin="150", ClampMax="3000")) float ForwardReach = 1050.f;
	/** Lateral radius of the loop, mirrored by the steering input at release. */
	UPROPERTY(EditAnywhere, Category="Flight", meta=(Units="cm", ClampMin="80", ClampMax="1500")) float SideReach = 480.f;
	UPROPERTY(EditAnywhere, Category="Flight", meta=(Units="s", ClampMin=".25", ClampMax="5")) float FlightSeconds = 1.45f;
	UPROPERTY(EditAnywhere, Category="Flight", meta=(Units="cm", ClampMin="2", ClampMax="60")) float SweepRadius = 18.f;
	/** From the front centre of the car's collision hull, in vehicle axes (cm). */
	UPROPERTY(EditAnywhere, Category="Flight", meta=(Units="cm")) FVector LaunchOffset = FVector(12,0,3);
	UPROPERTY(EditAnywhere, Category="Combat", meta=(Units="s", ClampMin=".2", ClampMax="30")) float FuseSeconds = 5.f;
	/** Vehicle attack multiplier captured at release. 0.4 x 25 attack = 10 base damage. */
	UPROPERTY(EditAnywhere, Category="Combat", meta=(ClampMin="0", ClampMax="10")) float DamageMultiplier = .4f;
	UPROPERTY(EditAnywhere, Category="Combat") EHALImpactTier ImpactTier = EHALImpactTier::Medium;
	UPROPERTY(EditAnywhere, Category="Combat") EHALImpactPushPreset PushDistance = EHALImpactPushPreset::Long;
	UPROPERTY(EditAnywhere, Category="Presentation", meta=(Units="cm", ClampMin="15", ClampMax="150")) float WingSpan = 68.f;
	/** Legacy procedural wings for configurations without an authored blade. */
	UPROPERTY(EditAnywhere, Category="Presentation") TObjectPtr<UMaterialInterface> DartMaterial;
	UPROPERTY(EditAnywhere, Category="Presentation") TObjectPtr<USoundBase> PinSignal;
	/** Nose along +X, wingspan along Y. Material slot zero accepts SeatColor and Signal. */
	UPROPERTY(EditAnywhere, Category="Presentation|Art") TObjectPtr<UStaticMesh> BladeMesh;
	/** Short ribbon along the actual flight path. UV.x is newest at 1. */
	UPROPERTY(EditAnywhere, Category="Presentation|Art") TObjectPtr<UMaterialInterface> WakeMaterial;
	/** Depth-tested contact pulse and directional shock front. Local +X points along the push. */
	UPROPERTY(EditAnywhere, Category="Presentation|Art") TObjectPtr<UMaterialInterface> ResonanceMaterial;
	/** Seconds of travelled path visible behind the blade. Zero disables the ribbon. */
	UPROPERTY(EditAnywhere, Category="Presentation|Art", meta=(Units="s", ClampMin="0", ClampMax="0.35")) float WakeSeconds = .14f;
	/** Visual distance from the pin to the leading shock front. */
	UPROPERTY(EditAnywhere, Category="Presentation|Art", meta=(Units="cm", ClampMin="80", ClampMax="480")) float BurstReach = 240.f;
	bool HasValidRules() const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

UCLASS()
class HAL_FUTURE_CREATION_API UHALBatarangAbility : public UHALVehicleUltimateAbility
{
	GENERATED_BODY()
public:
	UHALBatarangAbility();
	static UHALBatarangConfig* ConfigFor(const ATestVehiclePawn* Car);
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayTagContainer* Source, const FGameplayTagContainer* Target, FGameplayTagContainer* Failure) const override;
protected:
	virtual EHALUltimateStart StartUltimate(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, bool bReplicate, bool bCancelled) override;
private:
	UFUNCTION() void DartDestroyed(AActor* Actor);
	TWeakObjectPtr<AHALBatarang> ActiveDart;
};
