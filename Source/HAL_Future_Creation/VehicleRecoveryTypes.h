#pragma once

#include "CoreMinimal.h"
#include "HALImpactTypes.h"
#include "VehicleRecoveryTypes.generated.h"

/** Shared, frozen once per world. Angles are degrees; lengths cm; all times game seconds. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FVehicleRecoveryConfig
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, Category = "Pose") bool bEnabled = true;
	UPROPERTY(EditDefaultsOnly, Category = "Pose", meta = (ClampMin = "0", ClampMax = "80", Units = "deg")) float MaximumGroundSlope = 45;
	UPROPERTY(EditDefaultsOnly, Category = "Pose", meta = (ClampMin = "1", ClampMax = "85", Units = "deg")) float DriveEnterTilt = 50;
	UPROPERTY(EditDefaultsOnly, Category = "Pose", meta = (ClampMin = "1", ClampMax = "89", Units = "deg")) float DriveExitTilt = 65;
	UPROPERTY(EditDefaultsOnly, Category = "Pose", meta = (ClampMin = "0", ClampMax = "1", Units = "s")) float DriveStableSeconds = .1f;
	/** Additional distance below the projected physical proxy bounds. */
	UPROPERTY(EditDefaultsOnly, Category = "Pose", meta = (ClampMin = "0", ClampMax = "100", Units = "cm")) float SupportDistance = 30;
	/** Set equal to DriveExitTilt to avoid a no-drive/no-recovery angle gap. Saved tuning is preserved. */
	UPROPERTY(EditDefaultsOnly, Category = "Eligibility", meta = (ClampMin = "1", ClampMax = "180", Units = "deg")) float MinimumRecoveryTilt = 70;
	UPROPERTY(EditDefaultsOnly, Category = "Eligibility", meta = (ClampMin = "0", ClampMax = "10", Units = "s")) float FlippedSeconds = 1;
	UPROPERTY(EditDefaultsOnly, Category = "Eligibility", meta = (ClampMin = "1", ClampMax = "1000", Units = "cm/s")) float MaximumLinearSpeed = 150;
	UPROPERTY(EditDefaultsOnly, Category = "Eligibility", meta = (ClampMin = "1", ClampMax = "360", Units = "deg/s")) float MaximumAngularSpeed = 60;
	UPROPERTY(EditDefaultsOnly, Category = "Eligibility", meta = (ClampMin = ".1", ClampMax = "10", Units = "s")) float HoldSeconds = 1.5f;
	UPROPERTY(EditDefaultsOnly, Category = "Execution", meta = (ClampMin = ".1", ClampMax = "5", Units = "s")) float MaximumExecutionSeconds = 2;
	UPROPERTY(EditDefaultsOnly, Category = "Execution", meta = (ClampMin = "0", ClampMax = "30", Units = "s")) float CooldownSeconds = 3;
	UPROPERTY(EditDefaultsOnly, Category = "Execution") EHALImpactTier MinimumInterruptTier = EHALImpactTier::Medium;
	UPROPERTY(EditDefaultsOnly, Category = "Execution", meta = (ClampMin = "1", ClampMax = "40", Units = "deg")) float SuccessTilt = 15;
	/** PD gains; torque is mass-independent angular acceleration, never a rotation teleport. */
	UPROPERTY(EditDefaultsOnly, Category = "Response", meta = (ClampMin = "1", ClampMax = "100")) float AngularPositionGain = 25;
	UPROPERTY(EditDefaultsOnly, Category = "Response", meta = (ClampMin = "0", ClampMax = "30")) float AngularDampingGain = 7;
	/** Maximum torque acceleration, radians/second squared. */
	UPROPERTY(EditDefaultsOnly, Category = "Response", meta = (ClampMin = "1", ClampMax = "100")) float MaximumAngularAcceleration = 30;
	UPROPERTY(EditDefaultsOnly, Category = "Response", meta = (ClampMin = ".1", ClampMax = "10", Units = "rad/s")) float MaximumRecoveryAngularSpeed = 4;
	bool Validate(FString& Error) const;
	FString SignatureKey() const;
	bool GroundIsValid(bool bNear, const FVector& Normal) const;
	bool CanDrive(bool bPreviouslyAllowed, const FVector& Up, const FVector& Normal) const;
	bool IsEligible(bool bNear, const FVector& Up, const FVector& Normal, float Speed, float AngularRadians) const;
	FName EligibilityBlockReason(bool bNear, const FVector& Up, const FVector& Normal, float Speed, float AngularRadians) const;
};

/** Only physics memory is replayed. Permission timers, GAS and ball side effects stay on authority GT. */
USTRUCT()
struct HAL_FUTURE_CREATION_API FVehicleRecoveryPhysicsState
{
	GENERATED_BODY()
	UPROPERTY() uint32 Sequence = 0;
	UPROPERTY() bool bActive = false;
	UPROPERTY() FVector TargetUp = FVector::UpVector;
	UPROPERTY() FVector FallbackAxis = FVector::ForwardVector;
	UPROPERTY() float ElapsedSeconds = 0;
	bool IsValid() const;
	bool Equals(const FVehicleRecoveryPhysicsState& Other) const;
	void Serialize(FArchive& Ar);
};

/** Thread-safe observation, not writable gameplay authority. */
struct FVehicleStabilityObservation
{
	int32 PhysicsFrame = INDEX_NONE;
	bool bValid = false, bNearGround = false, bDrivingAllowed = false, bRecoveryHeld = false;
	FVector Up = FVector::UpVector, Forward = FVector::ForwardVector, GroundNormal = FVector::UpVector;
	float Speed = 0, AngularSpeed = 0, PoseStableSeconds = 0;
	FVehicleRecoveryPhysicsState Recovery;
};

UENUM(BlueprintType)
enum class EVehicleRecoveryPhase : uint8 { Idle, Eligible, Holding, Executing, Cooldown };

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FVehicleRecoveryStatus
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) EVehicleRecoveryPhase Phase = EVehicleRecoveryPhase::Idle;
	UPROPERTY(BlueprintReadOnly) bool bDrivingAllowed = false;
	UPROPERTY(BlueprintReadOnly) bool bNearGround = false;
	UPROPERTY(BlueprintReadOnly) float HoldProgress = 0;
	UPROPERTY(BlueprintReadOnly) float CooldownRemaining = 0;
	UPROPERTY(BlueprintReadOnly) int64 Sequence = 0;
	UPROPERTY(BlueprintReadOnly) FName LastOutcome;
	/** Authority permission feedback; None while executing or ready to accumulate hold. */
	UPROPERTY(BlueprintReadOnly) FName BlockReason;
};

/** Pure authority permission clock; never read/replayed by the physics thread. */
struct HAL_FUTURE_CREATION_API FVehicleRecoveryClock
{
	float Flipped = 0, Hold = 0, Cooldown = 0;
	bool Advance(const FVehicleRecoveryConfig& Config, bool bEligible, bool bHeld, float DeltaTime);
	void Finish(const FVehicleRecoveryConfig& Config) { Flipped = Hold = 0; Cooldown = Config.CooldownSeconds; }
};

namespace HALVehicleRecovery
{
	HAL_FUTURE_CREATION_API FVector AngularAcceleration(const FVehicleRecoveryConfig& Config,
		const FVehicleRecoveryPhysicsState& State, const FVector& Up, const FVector& AngularVelocity, float DeltaTime);
}
