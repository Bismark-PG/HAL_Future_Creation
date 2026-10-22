#pragma once

#include "CoreMinimal.h"
#include "HALVehiclePresentationTypes.generated.h"

UENUM(BlueprintType)
enum class EHALVehicleWheel : uint8 { FL, FR, BL, BR };

UENUM(BlueprintType)
enum class EHALBoneAxis : uint8 { X, Y, Z };

struct FHALChassisPresentationConfig;
struct FHALVisualSuspensionConfig;
struct FHALWheelSuspensionTravel;

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
	HAL_FUTURE_CREATION_API FRotator AxisRotation(EHALBoneAxis Axis, float Degrees);
	HAL_FUTURE_CREATION_API float AdvanceWheel(float Angle, float SpeedCmPerSecond, float RadiusCm, float Sign, float DeltaSeconds);
	HAL_FUTURE_CREATION_API float SmoothSteering(float Current, float Target, float ResponseSeconds, float DeltaSeconds);
	/** Seeds on first/invalid/discontinuous samples; otherwise filters bounded world-acceleration response. */
	HAL_FUTURE_CREATION_API void UpdateChassis(const FHALChassisPresentationConfig& Config,
		const FHALChassisMotionSample& Sample, float DeltaSeconds, FHALChassisPresentationState& State);
	HAL_FUTURE_CREATION_API FRotator ChassisRotation(const FHALChassisPresentationConfig& Config, const FHALChassisPresentationState& State);
	/** Fixed reference geometry, never the animated wheel pose. No world or physics mutation. */
	HAL_FUTURE_CREATION_API bool BuildSuspensionProbe(const FHALVisualSuspensionConfig& Config, const FHALWheelSuspensionTravel& Travel,
		const FVector& Center, const FVector& VehicleUp, float RadiusCm, FVector& Start, FVector& End);
	HAL_FUTURE_CREATION_API FHALWheelSuspensionTarget SuspensionTarget(const FHALVisualSuspensionConfig& Config,
		const FHALWheelSuspensionTravel& Travel, const FVector& Center, const FVector& VehicleUp, float RadiusCm, const FHALWheelSupportSample& Support);
	HAL_FUTURE_CREATION_API FVector SuspensionBoneTranslation(const FTransform& MeshWorld, const FTransform& ReferenceSuspension,
		const FVector& VehicleUp, float OffsetCm);
}
