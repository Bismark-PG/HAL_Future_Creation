#pragma once
#include "CoreMinimal.h"
#include "HALVehicleUltimateAbility.h"
#include "Engine/DataAsset.h"
#include "GameplayEffect.h"
#include "HALSideShiftMotion.h"
#include "HALVehicleFXMount.h"
#include "HALVehiclePresentationConfig.h"
#include "HALSideShiftAbility.generated.h"

class UNiagaraSystem;
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALSideShiftFX
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Rendering") bool bEnabled = false;
	UPROPERTY(EditAnywhere, Category="Tire Marks") FHALTireSkidConfig TireMarks;
	/** Per-component Niagara time scale; ordinary drift effects retain their own playback rate. */
	UPROPERTY(EditAnywhere, Category="Tire Marks", meta=(ClampMin="0.1", ClampMax="10")) float TireFXPlayRate = 3.333333f;
	UPROPERTY(EditAnywhere, Category="Side Jets") TObjectPtr<UNiagaraSystem> Sparks;
	UPROPERTY(EditAnywhere, Category="Side Jets") TObjectPtr<UNiagaraSystem> Smoke;
	/** Vehicle-left exhausts. Used for a rightward shift. Niagara velocity is world-space outward. */
	UPROPERTY(EditAnywhere, Category="Side Jets") TArray<FHALVehicleFXMount> LeftMounts;
	/** Vehicle-right exhausts. Used for a leftward shift. */
	UPROPERTY(EditAnywhere, Category="Side Jets") TArray<FHALVehicleFXMount> RightMounts;
	UPROPERTY(EditAnywhere, Category="Sparks", meta=(ClampMin="0", ClampMax="2000")) float SparkRate = 666.6667f;
	UPROPERTY(EditAnywhere, Category="Sparks", meta=(Units="cm/s", ClampMin="100", ClampMax="12000")) float SparkSpeed = 8000;
	UPROPERTY(EditAnywhere, Category="Sparks", meta=(Units="s", ClampMin="0.01", ClampMax="1")) float SparkLifetime = .048f;
	UPROPERTY(EditAnywhere, Category="Sparks", meta=(Units="cm", ClampMin="1", ClampMax="100")) FVector2D SparkSize = FVector2D(12,34);
	UPROPERTY(EditAnywhere, Category="Sparks", meta=(Units="deg", ClampMin="1", ClampMax="60")) float SpreadDegrees = 16;
	UPROPERTY(EditAnywhere, Category="Smoke", meta=(ClampMin="0", ClampMax="150")) float SmokeRate = 116.6667f;
	UPROPERTY(EditAnywhere, Category="Smoke", meta=(Units="cm/s", ClampMin="10", ClampMax="2000")) float SmokeSpeed = 2000;
	UPROPERTY(EditAnywhere, Category="Smoke", meta=(Units="s", ClampMin="0.1", ClampMax="2")) float SmokeLifetime = .12f;
	UPROPERTY(EditAnywhere, Category="Smoke", meta=(Units="cm", ClampMin="1", ClampMax="200")) float SmokeSize = 34;
	UPROPERTY(EditAnywhere, Category="Style") FLinearColor SparkColor = FLinearColor(1,.54f,.1f);
	UPROPERTY(EditAnywhere, Category="Style") FLinearColor SmokeColor = FLinearColor(.7f,.75f,.8f);
	UPROPERTY(EditAnywhere, Category="Style", meta=(ClampMin="0", ClampMax="3")) float Glow = 1;
	UPROPERTY(EditAnywhere, Category="Lifecycle", meta=(Units="s", ClampMin="0.01", ClampMax="0.3")) float FadeSeconds = .018f;
	bool IsValid() const;
};

UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALSideShiftConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY() int32 MaximumCharges = 4;
	UPROPERTY() float RechargeSeconds = 5;
	UPROPERTY(EditAnywhere, Category="Input", meta=(ClampMin="0.05", ClampMax="1")) float DirectionDeadZone = .2f;
	UPROPERTY(EditAnywhere, Category="Motion") FHALSideShiftMotion Motion;
	UPROPERTY(EditAnywhere, Category="Presentation") FHALSideShiftFX FX;
	bool Validate(FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
UCLASS()
class HAL_FUTURE_CREATION_API UHALSideShiftChargeEffect : public UGameplayEffect { GENERATED_BODY() public: UHALSideShiftChargeEffect(); };
UCLASS()
class HAL_FUTURE_CREATION_API UHALSideShiftStateEffect : public UGameplayEffect { GENERATED_BODY() public: UHALSideShiftStateEffect(); };

UCLASS()
class HAL_FUTURE_CREATION_API UHALSideShiftAbility : public UHALVehicleUltimateAbility
{
	GENERATED_BODY()
public:
	UHALSideShiftAbility();
	virtual float GetUltimateCost() const override { return HALUltimate::SideShiftCost; }
	UPROPERTY(EditDefaultsOnly, Category="Side Shift") TObjectPtr<UHALSideShiftConfig> SkillConfig;
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo,const FGameplayAbilitySpec& Spec) override;
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo,const FGameplayAbilitySpec& Spec) override;
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,const FGameplayTagContainer* TargetTags,FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual EHALUltimateStart StartUltimate(FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,const FGameplayEventData* TriggerEventData) override;
};
