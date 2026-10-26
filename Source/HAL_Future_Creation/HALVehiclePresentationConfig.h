#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HALVehiclePresentationTypes.h"
#include "HALVehiclePresentationConfig.generated.h"

class USkeletalMesh;
class UHALVehicleAnimInstance;
struct FReferenceSkeleton;

/** Per-effective-tier cosmetic envelope, independent of gameplay impact strength. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALImpactReboundTier
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Amplitude", meta = (ClampMin = "0", ClampMax = "30", Units = "deg")) float PitchDegrees = 3;
	UPROPERTY(EditAnywhere, Category = "Amplitude", meta = (ClampMin = "0", ClampMax = "30", Units = "deg")) float RollDegrees = 3;
	UPROPERTY(EditAnywhere, Category = "Envelope", meta = (ClampMin = "0.01", ClampMax = "0.3", Units = "s")) float RiseSeconds = .04f;
	UPROPERTY(EditAnywhere, Category = "Envelope", meta = (ClampMin = "0.05", ClampMax = "2", Units = "s")) float ReturnSeconds = .18f;
	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALImpactReboundConfig
{
	GENERATED_BODY()
	FHALImpactReboundConfig();
	UPROPERTY(EditAnywhere, Category = "Impact Rebound") bool bEnabled = false;
	UPROPERTY(EditAnywhere, Category = "Tiers") FHALImpactReboundTier Light;
	UPROPERTY(EditAnywhere, Category = "Tiers") FHALImpactReboundTier Medium;
	UPROPERTY(EditAnywhere, Category = "Tiers") FHALImpactReboundTier Heavy;
	/** Additional artistic direction relative to the already calibrated Chassis axes/signs. */
	UPROPERTY(EditAnywhere, Category = "Direction", meta = (ClampMin = "-1", ClampMax = "1")) float PitchSign = 1;
	UPROPERTY(EditAnywhere, Category = "Direction", meta = (ClampMin = "-1", ClampMax = "1")) float RollSign = 1;
	/** Reject almost vertical world impulses rather than inventing a horizontal direction. */
	UPROPERTY(EditAnywhere, Category = "Direction", meta = (ClampMin = "0.01", ClampMax = "1")) float MinimumHorizontalDirection = .1f;
	/** Normal airborne hits remain visible; severe tilt and recovery fade existing envelopes permanently. */
	UPROPERTY(EditAnywhere, Category = "Pose", meta = (ClampMin = "0.25", ClampMax = "1")) float MinimumVehicleUpZ = .5f;
	UPROPERTY(EditAnywhere, Category = "Pose", meta = (ClampMin = "0.01", ClampMax = "0.5", Units = "s")) float PoseFadeSeconds = .08f;
	UPROPERTY(EditAnywhere, Category = "Network", meta = (ClampMin = "0.05", ClampMax = "2", Units = "s")) float MaximumEventAgeSeconds = .5f;
	const FHALImpactReboundTier* GetTier(EHALImpactTier Tier) const;
	bool Validate(FString& Error) const;
};

/** Cosmetic shot envelope, independent of BallControl's physical recoil. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALLaunchReboundConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Launch Rebound") bool bEnabled = false;
	/** Positive uses Chassis PitchAxis/PitchSign (normally nose up); negative reverses it. */
	UPROPERTY(EditAnywhere, Category = "Launch Rebound", meta = (ClampMin = "-30", ClampMax = "30", Units = "deg")) float PitchDegrees = 8;
	UPROPERTY(EditAnywhere, Category = "Envelope", meta = (ClampMin = "0.01", ClampMax = "0.3", Units = "s")) float RiseSeconds = .05f;
	UPROPERTY(EditAnywhere, Category = "Envelope", meta = (ClampMin = "0.05", ClampMax = "2", Units = "s")) float ReturnSeconds = .25f;
	UPROPERTY(EditAnywhere, Category = "Prediction", meta = (ClampMin = "0.01", ClampMax = "0.3", Units = "s")) float RejectionFadeSeconds = .08f;
	/** Unknown accepted events older than this are discarded; never replayed on late join. */
	UPROPERTY(EditAnywhere, Category = "Network", meta = (ClampMin = "0.05", ClampMax = "2", Units = "s")) float MaximumEventAgeSeconds = .5f;
	bool Validate(FString& Error) const;
};

/** Query/animation settings only. Distances are world cm, not physical spring parameters. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALVisualSuspensionConfig
{
	GENERATED_BODY()
	/** Add the four suspension AnimGraph nodes before enabling on a saved presentation DA. */
	UPROPERTY(EditAnywhere, Category = "Suspension") bool bEnabled = false;
	/** Exponential time constant. Zero follows measured travel immediately. */
	UPROPERTY(EditAnywhere, Category = "Suspension", meta = (ClampMin = "0", ClampMax = "2", Units = "s")) float ResponseSeconds = .08f;
	/** Extra clearance above maximum compression and below maximum droop. */
	UPROPERTY(EditAnywhere, Category = "Probe", meta = (ClampMin = "0.1", ClampMax = "50", Units = "cm")) float ProbeMarginCm = 2;
	/** Normal must face both world +Z and the probe's vehicle-up axis by at least this cosine. */
	UPROPERTY(EditAnywhere, Category = "Probe", meta = (ClampMin = "0.25", ClampMax = "1")) float MinimumSupportNormalDot = .5f;
	/** Beyond this tilt, fade to neutral without tracing. Also neutral during active self-righting. */
	UPROPERTY(EditAnywhere, Category = "Probe", meta = (ClampMin = "0.25", ClampMax = "1")) float MinimumVehicleUpZ = .5f;
	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALWheelSuspensionTravel
{
	GENERATED_BODY()
	/** Cosmetic travel relative to the calibrated reference wheel center; not an auto-fit value. */
	UPROPERTY(EditAnywhere, Category = "Suspension", meta = (ClampMin = "0", ClampMax = "100", Units = "cm")) float MaxCompressionCm = 10;
	UPROPERTY(EditAnywhere, Category = "Suspension", meta = (ClampMin = "0", ClampMax = "100", Units = "cm")) float MaxExtensionCm = 15;
	bool Validate(FString& Error) const;
};

/** Additional local chassis animation only. Never changes the physical vehicle pose. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALChassisPresentationConfig
{
	GENERATED_BODY()
	/** Enable after calibrating Chassis local axes and adding its AnimGraph node. */
	UPROPERTY(EditAnywhere, Category = "Chassis") bool bEnabled = false;
	UPROPERTY(EditAnywhere, Category = "Axes") EHALBoneAxis PitchAxis = EHALBoneAxis::Y;
	UPROPERTY(EditAnywhere, Category = "Axes", meta = (ClampMin = "-1", ClampMax = "1")) float PitchSign = -1;
	UPROPERTY(EditAnywhere, Category = "Axes") EHALBoneAxis RollAxis = EHALBoneAxis::X;
	UPROPERTY(EditAnywhere, Category = "Axes", meta = (ClampMin = "-1", ClampMax = "1")) float RollSign = 1;
	UPROPERTY(EditAnywhere, Category = "Lean", meta = (ClampMin = "0", ClampMax = "30", Units = "deg")) float MaxPitchDegrees = 4;
	UPROPERTY(EditAnywhere, Category = "Lean", meta = (ClampMin = "0", ClampMax = "30", Units = "deg")) float MaxRollDegrees = 4;
	/** Final additive model angles, shared by driving lean and discrete visual rebounds. */
	UPROPERTY(EditAnywhere, Category = "Composition", meta = (ClampMin = "0", ClampMax = "45", Units = "deg")) float TotalPitchLimitDegrees = 30;
	UPROPERTY(EditAnywhere, Category = "Composition", meta = (ClampMin = "0", ClampMax = "45", Units = "deg")) float TotalRollLimitDegrees = 30;
	/** Magnitude of longitudinal acceleration (cm/s squared) that reaches MaxPitchDegrees. */
	UPROPERTY(EditAnywhere, Category = "Lean", meta = (ClampMin = "1", ClampMax = "10000")) float FullPitchAcceleration = 1800;
	/** Magnitude of lateral acceleration (cm/s squared) that reaches MaxRollDegrees. */
	UPROPERTY(EditAnywhere, Category = "Lean", meta = (ClampMin = "1", ClampMax = "10000")) float FullRollAcceleration = 1800;
	/** Exponential time constant, also used when fading back to neutral in the air/recovery. */
	UPROPERTY(EditAnywhere, Category = "Lean", meta = (ClampMin = "0.01", ClampMax = "2", Units = "s")) float ResponseSeconds = .15f;
	/** Small acceleration noise (cm/s squared) produces no additional lean. */
	UPROPERTY(EditAnywhere, Category = "Lean", meta = (ClampMin = "0", ClampMax = "500")) float AccelerationDeadZone = 20;
	/** Position error from expected motion; resets only chassis sampling, leaving wheel rolling intact. */
	UPROPERTY(EditAnywhere, Category = "Sampling", meta = (ClampMin = "1", ClampMax = "1000", Units = "cm")) float CorrectionResetDistanceCm = 100;
	/** Larger acceleration spikes (cm/s squared) are discarded, not converted into an impact animation. */
	UPROPERTY(EditAnywhere, Category = "Sampling", meta = (ClampMin = "1000", ClampMax = "100000")) float MaximumSampleAcceleration = 20000;
	bool Validate(FString& Error) const;
};

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
	/** Independent travel per wheel. All distances include model/component scale. */
	UPROPERTY(EditAnywhere, Category = "Suspension")
	FHALWheelSuspensionTravel Suspension;
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
	UPROPERTY(EditAnywhere, Category = "Chassis")
	FHALChassisPresentationConfig Chassis;
	UPROPERTY(EditAnywhere, Category = "Launch Rebound")
	FHALLaunchReboundConfig LaunchRebound;
	UPROPERTY(EditAnywhere, Category = "Impact Rebound")
	FHALImpactReboundConfig ImpactRebound;
	/** Visual traces only; later physical wheel observations replace this source, not add another spring. */
	UPROPERTY(EditAnywhere, Category = "Suspension")
	FHALVisualSuspensionConfig Suspension;
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
