#pragma once

#include "CoreMinimal.h"
#include "HALImpactTypes.h"
#include "Engine/NetSerialization.h"
#include "HALVehiclePresentationTypes.generated.h"

UENUM(BlueprintType)
enum class EHALVehicleWheel : uint8 { FL, FR, BL, BR };

UENUM(BlueprintType)
enum class EHALBoneAxis : uint8 { X, Y, Z };

struct FHALImpactReboundConfig;
struct FHALImpactReboundTier;
struct FHALCombatHitResult;
struct FHALTireSkidConfig;

/** Authority result, already reduced for resistance. Direction is captured in target-local space. */
USTRUCT()
struct FHALImpactVisualEvent
{
	GENERATED_BODY()
	UPROPERTY() uint32 Sequence = 0;
	UPROPERTY() float ServerTimeSeconds = 0;
	UPROPERTY() EHALImpactTier Tier = EHALImpactTier::None;
	UPROPERTY() FVector_NetQuantizeNormal LocalDirection = FVector::ZeroVector;
};

struct FHALImpactVisualPulse
{
	EHALImpactTier Tier = EHALImpactTier::None;
	FVector2D Direction = FVector2D::ZeroVector;
	float Age = 0, FadeAge = -1, FadeStart = 0;
};

struct FHALImpactVisualState
{
	TArray<FHALImpactVisualPulse> Pulses;
	uint32 LastSeenSequence = 0, StartedCount = 0;
};

struct FHALImpactVisualOutput
{
	FVector2D Degrees = FVector2D::ZeroVector;
	FVector2D Weights = FVector2D::ZeroVector;
};

struct FHALChassisPresentationConfig;
struct FHALVisualSuspensionConfig;
struct FHALWheelSuspensionTravel;
struct FHALLaunchReboundConfig;

/** Bounded shot identities only; no gameplay authority or physics replay state. */
struct FHALLaunchVisualPulse
{
	uint32 Sequence = 0;
	float Age = 0, RejectAge = -1, RejectStart = 0;
	bool bPredicted = false, bConfirmed = false;
};

struct FHALLaunchVisualState
{
	// Eight overlapping shots already exceed normal one-ball firing; cap memory without an event bus.
	TArray<FHALLaunchVisualPulse, TInlineAllocator<8>> Pulses;
	uint32 LastSeenSequence = 0;
	uint32 PredictedCount = 0, StartedCount = 0, ConfirmedCount = 0, RejectedCount = 0;
};

/** Read-only source boundary. Future physical suspension can publish equivalent cosmetic targets. */
struct FHALWheelSupportSample
{
	FVector Point = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	bool bHit = false;
	bool bStartPenetrating = false;
};

struct FHALWheelSuspensionTarget
{
	float OffsetCm = 0;
	bool bSupported = false;
};

/** External/game-thread motion only; no internal Chaos handles are retained. */
struct FHALChassisMotionSample
{
	FVector Position = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	bool bDrivingAllowed = false;
};

/** Cosmetic filter/history. Not replicated or included in gameplay physics rewind state. */
struct FHALChassisPresentationState
{
	FHALChassisMotionSample Previous;
	float ForwardAcceleration = 0, RightAcceleration = 0;
	float PitchDegrees = 0, RollDegrees = 0;
	bool bHasSample = false;
};

/** Copied under movement's lock. Never an alias to physics-thread state. */
struct FHALVehiclePresentationInput
{
	float Steering = 0;
	int32 PhysicsFrame = INDEX_NONE;
	bool bGrounded = false;
	bool bValid = false;
	/** Accepted driving demand only: handbrake=1, brake/reverse=2, throttle=4. */
	uint8 TireSkidInputs = 0;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALWheelPresentationPose
{
	GENERATED_BODY()
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation")
	FRotator SteeringRotation = FRotator::ZeroRotator;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation")
	FRotator SpinRotation = FRotator::ZeroRotator;
	/** Add to Existing, Bone Space, Translation only on the suspension bone. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation")
	FVector SuspensionTranslation = FVector::ZeroVector;
	/** Positive compression / negative extension in world cm along vehicle up. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation")
	float SuspensionOffsetCm = 0;
	/** Accepted support within extension reach; this is never a gameplay grounded flag. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation")
	bool bSuspensionSupported = false;
};

/** Game-thread result. AnimInstance copies this once; AnimGraph only reads the copy. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALVehiclePresentationFrame
{
	GENERATED_BODY()
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation")
	bool bValid = false;
	/** Cosmetic decisions, also available for headless diagnostics. Bits use EHALVehicleWheel slots. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation") uint8 TireSkidWheelMask = 0;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation") uint8 TireSkidReasons = 0;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation")
	FHALWheelPresentationPose FL;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation")
	FHALWheelPresentationPose FR;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation")
	FHALWheelPresentationPose BL;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation")
	FHALWheelPresentationPose BR;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation")
	float SteeringDegrees = 0;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation")
	float ForwardSpeedCmPerSecond = 0;
	/** Add to Existing, Bone Space on ChassisBone; translation and scale remain Ignore. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation")
	FRotator ChassisRotation = FRotator::ZeroRotator;
	/** Signed acceleration response before model-specific axis/sign mapping. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation")
	float ChassisPitchDegrees = 0;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation")
	float ChassisRollDegrees = 0;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation")
	bool bDrivingLeanActive = false;
	/** Per-shot sum before final chassis composition limits; positive follows calibrated PitchSign. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation") float LaunchPitchDegrees = 0;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation") float CombinedChassisPitchDegrees = 0;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation") float CombinedChassisRollDegrees = 0;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation") float ImpactPitchDegrees = 0;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation") float ImpactRollDegrees = 0;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation")
	bool bSuspensionActive = false;
	/** Actual visual scene queries this update: zero when disabled/tilted/recovering, otherwise four. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation")
	int32 SuspensionTraceCount = 0;
	/** Game-thread query block timing only; not GPU/render cost or a four-PC performance guarantee. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Presentation")
	float SuspensionQueryMicroseconds = 0;
	FHALWheelPresentationPose& Wheel(EHALVehicleWheel Id);
};

namespace HALVehiclePresentation
{
	HAL_FUTURE_CREATION_API uint8 TireSkidInputs(float Brake, float Throttle, bool bHandbrake);
	/** Reasons: drift=1, counter-direction braking=2. Passive deceleration cannot trigger this. */
	HAL_FUTURE_CREATION_API uint8 TireSkidReasons(const FHALTireSkidConfig& Config, uint8 Inputs,
		float ForwardSpeed, float LateralSpeed, uint8 PreviousReasons, bool bAllowed);
	HAL_FUTURE_CREATION_API bool IsImpactReboundEligible(const FHALCombatHitResult& Result);
	HAL_FUTURE_CREATION_API bool ImpactLocalDirection(const FVector& WorldDirection, const FQuat& TargetRotation,
		float MinimumHorizontal, FVector& OutLocal);
	HAL_FUTURE_CREATION_API float ImpactEnvelope(const FHALImpactReboundTier& Tier, float Age);
	HAL_FUTURE_CREATION_API bool AcceptImpact(const FHALImpactReboundConfig& Config, const FHALImpactVisualEvent& Event,
		float Age, bool bPoseAllowed, FHALImpactVisualState& State);
	HAL_FUTURE_CREATION_API void FadeImpact(const FHALImpactReboundConfig& Config, FHALImpactVisualState& State);
	HAL_FUTURE_CREATION_API FHALImpactVisualOutput AdvanceImpact(const FHALImpactReboundConfig& Config, float DeltaSeconds, FHALImpactVisualState& State);
	HAL_FUTURE_CREATION_API FRotator ComposeImpactRotation(const FHALChassisPresentationConfig& Chassis,
		const FHALChassisPresentationState& Driving, const FHALLaunchReboundConfig& LaunchConfig, float LaunchPitch,
		const FHALImpactVisualOutput& Impact, float& OutPitch, float& OutRoll);
	HAL_FUTURE_CREATION_API FRotator AxisRotation(EHALBoneAxis Axis, float Degrees);
	HAL_FUTURE_CREATION_API float AdvanceWheel(float Angle, float SpeedCmPerSecond, float RadiusCm, float Sign, float DeltaSeconds);
	HAL_FUTURE_CREATION_API float SmoothSteering(float Current, float Target, float ResponseSeconds, float DeltaSeconds);
	/** Seeds on first/invalid/discontinuous samples; otherwise filters bounded world-acceleration response. */
	HAL_FUTURE_CREATION_API void UpdateChassis(const FHALChassisPresentationConfig& Config,
		const FHALChassisMotionSample& Sample, float DeltaSeconds, FHALChassisPresentationState& State);
	HAL_FUTURE_CREATION_API FRotator ChassisRotation(const FHALChassisPresentationConfig& Config, const FHALChassisPresentationState& State);
	HAL_FUTURE_CREATION_API FRotator ComposeChassisRotation(const FHALChassisPresentationConfig& Config,
		const FHALChassisPresentationState& Driving, const FHALLaunchReboundConfig& LaunchConfig, float LaunchPitch, float& OutCombinedPitch);
	HAL_FUTURE_CREATION_API float LaunchEnvelope(const FHALLaunchReboundConfig& Config, float Age);
	HAL_FUTURE_CREATION_API bool PredictLaunch(const FHALLaunchReboundConfig& Config, uint32 Sequence, FHALLaunchVisualState& State);
	HAL_FUTURE_CREATION_API bool AcceptLaunch(const FHALLaunchReboundConfig& Config, uint32 Sequence, float Age, FHALLaunchVisualState& State);
	HAL_FUTURE_CREATION_API bool RejectLaunch(const FHALLaunchReboundConfig& Config, uint32 Sequence, FHALLaunchVisualState& State);
	HAL_FUTURE_CREATION_API float AdvanceLaunch(const FHALLaunchReboundConfig& Config, float DeltaSeconds, FHALLaunchVisualState& State);
	/** Fixed reference geometry, never the animated wheel pose. No world or physics mutation. */
	HAL_FUTURE_CREATION_API bool BuildSuspensionProbe(const FHALVisualSuspensionConfig& Config, const FHALWheelSuspensionTravel& Travel,
		const FVector& Center, const FVector& VehicleUp, float RadiusCm, FVector& Start, FVector& End);
	HAL_FUTURE_CREATION_API FHALWheelSuspensionTarget SuspensionTarget(const FHALVisualSuspensionConfig& Config,
		const FHALWheelSuspensionTravel& Travel, const FVector& Center, const FVector& VehicleUp, float RadiusCm, const FHALWheelSupportSample& Support);
	HAL_FUTURE_CREATION_API FVector SuspensionBoneTranslation(const FTransform& MeshWorld, const FTransform& ReferenceSuspension,
		const FVector& VehicleUp, float OffsetCm);
}
