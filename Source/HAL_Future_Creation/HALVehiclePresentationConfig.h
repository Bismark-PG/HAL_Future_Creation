#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HALVehiclePresentationTypes.h"
#include "HALVehiclePresentationConfig.generated.h"

class USkeletalMesh;
class UHALVehicleAnimInstance;
struct FReferenceSkeleton;

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALWheelPresentationConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Wheel")
	EHALVehicleWheel Wheel = EHALVehicleWheel::FL;
	UPROPERTY(EditAnywhere, Category = "Bones")
	FName SuspensionBone;
	UPROPERTY(EditAnywhere, Category = "Bones")
	FName SteeringBone;
	UPROPERTY(EditAnywhere, Category = "Bones")
	FName SpinBone;
	/** Actual world-space radius in cm, including imported/component scale. Zero requires calibration. */
	UPROPERTY(EditAnywhere, Category = "Geometry", meta = (ClampMin = "0", Units = "cm"))
	float RadiusCm = 0;
	UPROPERTY(EditAnywhere, Category = "Steering")
	bool bSteers = false;
	UPROPERTY(EditAnywhere, Category = "Steering")
	EHALBoneAxis SteeringAxis = EHALBoneAxis::Z;
	/** Must be +1 or -1, checked against the imported bone's local axis. */
	UPROPERTY(EditAnywhere, Category = "Steering", meta = (ClampMin = "-1", ClampMax = "1"))
	float SteeringSign = 1;
	UPROPERTY(EditAnywhere, Category = "Rolling")
	EHALBoneAxis SpinAxis = EHALBoneAxis::Y;
	UPROPERTY(EditAnywhere, Category = "Rolling", meta = (ClampMin = "-1", ClampMax = "1"))
	float SpinSign = 1;
};

/** Optional, visual-only. Invalid configuration falls back without invalidating VehicleDefinition gameplay. */
UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALVehiclePresentationConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UHALVehiclePresentationConfig();
	/** Enable only after importing and calibrating this model; defaults deliberately stay unready. */
	UPROPERTY(EditAnywhere, Category = "Status")
	bool bReadyForUse = false;
	UPROPERTY(EditAnywhere, Category = "Assets")
	TObjectPtr<USkeletalMesh> Mesh;
	UPROPERTY(EditAnywhere, Category = "Assets")
	TSubclassOf<UHALVehicleAnimInstance> AnimClass;
	UPROPERTY(EditAnywhere, Category = "Assets")
	FTransform RelativeTransform = FTransform::Identity;
	UPROPERTY(EditAnywhere, Category = "Bones")
	FName RootBone = TEXT("Root_jnt");
	UPROPERTY(EditAnywhere, Category = "Bones")
	FName ChassisBone = TEXT("Chassis_jnt");
	/** Exactly FL/FR/BL/BR, in any order. Names do not rebind AnimBP Modify Bone nodes. */
	UPROPERTY(EditAnywhere, Category = "Wheels")
	TArray<FHALWheelPresentationConfig> Wheels;
	/** Visual wheel angle only; does not alter driving torque. */
	UPROPERTY(EditAnywhere, Category = "Steering", meta = (ClampMin = "0", ClampMax = "60", Units = "deg"))
	float MaxSteeringDegrees = 25;
	/** Exponential time constant. Zero snaps directly to target. */
	UPROPERTY(EditAnywhere, Category = "Steering", meta = (ClampMin = "0", ClampMax = "2", Units = "s"))
	float SteeringResponseSeconds = 0.12f;
	UPROPERTY(EditAnywhere, Category = "Rolling", meta = (ClampMin = "0", ClampMax = "100", Units = "cm/s"))
	float RollingDeadZoneCmPerSecond = 1;
	/** Longer gaps reset the pose instead of integrating a suspended/offscreen interval. */
	UPROPERTY(EditAnywhere, Category = "Sampling", meta = (ClampMin = "0.034", ClampMax = "0.25", Units = "s"))
	float MaxSampleDeltaSeconds = 0.1f;
	UPROPERTY(EditAnywhere, Category = "Sampling", meta = (ClampMin = "1", Units = "cm"))
	float TeleportResetDistanceCm = 1000;

	bool ValidateSettings(FString& Error) const;
	bool ValidateRig(const FReferenceSkeleton& Skeleton, FString& Error) const;
	bool Validate(FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
