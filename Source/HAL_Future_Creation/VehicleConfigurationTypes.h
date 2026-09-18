#pragma once
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "VehicleConfigurationTypes.generated.h"

/** Explicit migration source; Legacy preserves serialized MVP-A tuning. */
UENUM(BlueprintType)
enum class EVehicleConfigurationSource : uint8 { Legacy, Definition };

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FArcadeDriveConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Drive", meta = (ClampMin = "0.0", Units = "cm/s^2"))
	float ForwardAcceleration = 1800.0f;

	UPROPERTY(EditAnywhere, Category = "Drive", meta = (ClampMin = "0.0", Units = "cm/s^2"))
	float ReverseAcceleration = 1200.0f;

	UPROPERTY(EditAnywhere, Category = "Drive", meta = (ClampMin = "0.0", Units = "cm/s^2"))
	float BrakeDeceleration = 2800.0f;

	UPROPERTY(EditAnywhere, Category = "Drive", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaxForwardSpeed = 2400.0f;

	UPROPERTY(EditAnywhere, Category = "Drive", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaxReverseSpeed = 900.0f;

	UPROPERTY(EditAnywhere, Category = "Drive", meta = (ClampMin = "0.0", Units = "cm/s"))
	float ReverseEngageSpeed = 80.0f;

	UPROPERTY(EditAnywhere, Category = "Drive", meta = (ClampMin = "0.0"))
	float CoastingDragRate = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Drive", meta = (ClampMin = "0.0"))
	float OverspeedCorrectionRate = 3.0f;

	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FArcadeGripConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Grip", meta = (ClampMin = "0.0"))
	float LateralGripRate = 8.0f;

	UPROPERTY(EditAnywhere, Category = "Grip", meta = (ClampMin = "0.0"))
	float HandbrakeGripRate = 1.8f;

	UPROPERTY(EditAnywhere, Category = "Grip", meta = (ClampMin = "0.0", Units = "cm/s^2"))
	float MaxLateralGripAcceleration = 4500.0f;

	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FArcadeSteeringConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Steering", meta = (ClampMin = "0.0"))
	float SteeringAngularAcceleration = 4.5f;

	UPROPERTY(EditAnywhere, Category = "Steering", meta = (ClampMin = "1.0", Units = "cm/s"))
	float FullSteeringSpeed = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Steering", meta = (ClampMin = "0.0", Units = "cm/s"))
	float HighSpeedSteeringStart = 1600.0f;

	UPROPERTY(EditAnywhere, Category = "Steering", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HighSpeedSteeringScale = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Steering", meta = (ClampMin = "0.0"))
	float HandbrakeSteeringMultiplier = 1.45f;

	UPROPERTY(EditAnywhere, Category = "Steering", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HandbrakeMinimumSteeringAuthority = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Steering", meta = (ClampMin = "0.0", UIMax = "500.0", Units = "cm/s"))
	float HandbrakeReverseSteeringSpeed = 150.0f;

	UPROPERTY(EditAnywhere, Category = "Steering", meta = (ClampMin = "0.0"))
	float YawDampingRate = 2.2f;

	UPROPERTY(EditAnywhere, Category = "Steering", meta = (ClampMin = "0.0"))
	float HandbrakeYawDampingRate = 0.8f;

	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FArcadeWallEscapeConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "WallEscape")
	bool bEnableWallEscape = true;

	UPROPERTY(EditAnywhere, Category = "WallEscape", meta = (ClampMin = "0.0", Units = "cm/s"))
	float WallEscapeEnterSpeed = 300.0f;

	UPROPERTY(EditAnywhere, Category = "WallEscape", meta = (ClampMin = "0.0", Units = "cm/s"))
	float WallEscapeExitSpeed = 450.0f;

	UPROPERTY(EditAnywhere, Category = "WallEscape", meta = (ClampMin = "1.0", ClampMax = "100.0", Units = "cm"))
	float WallEscapeProbeDistance = 15.0f;

	UPROPERTY(EditAnywhere, Category = "WallEscape", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float WallEscapeMinimumSteering = 0.2f;

	UPROPERTY(EditAnywhere, Category = "WallEscape", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WallEscapeIntoWallThrottleScale = 0.05f;

	UPROPERTY(EditAnywhere, Category = "WallEscape", meta = (ClampMin = "0.0", UIMax = "6.0"))
	float WallEscapeTargetYawRate = 2.6f;

	UPROPERTY(EditAnywhere, Category = "WallEscape", meta = (ClampMin = "0.0", UIMax = "30.0"))
	float WallEscapeYawResponseRate = 10.0f;

	UPROPERTY(EditAnywhere, Category = "WallEscape", meta = (ClampMin = "0.0", UIMax = "40.0"))
	float WallEscapeMaxYawAcceleration = 18.0f;

	UPROPERTY(EditAnywhere, Category = "WallEscape", meta = (ClampMin = "0.0", UIMax = "1.0", Units = "s"))
	float WallEscapeBlendInTime = 0.08f;

	UPROPERTY(EditAnywhere, Category = "WallEscape", meta = (ClampMin = "0.0", UIMax = "1.0", Units = "s"))
	float WallEscapeBlendOutTime = 0.2f;

	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FArcadeDebugConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawWallEscapeDebug = false;

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawGroundDebug = false;

	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FArcadeGroundConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Ground", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundTraceExtraDistance = 30.0f;

	UPROPERTY(EditAnywhere, Category = "Ground")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;

	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FArcadeVehicleConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Configuration")
	FArcadeDriveConfig Drive;

	UPROPERTY(EditAnywhere, Category = "Configuration")
	FArcadeGripConfig Grip;

	UPROPERTY(EditAnywhere, Category = "Configuration")
	FArcadeSteeringConfig Steering;

	UPROPERTY(EditAnywhere, Category = "Configuration")
	FArcadeWallEscapeConfig WallEscape;

	UPROPERTY(EditAnywhere, Category = "Configuration")
	FArcadeDebugConfig Debug;

	UPROPERTY(EditAnywhere, Category = "Configuration")
	FArcadeGroundConfig Ground;

	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FBallControlAcquisitionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Acquisition", meta = (ClampMin = "0.0", Units = "cm"))
	float AcquisitionRadius = 350.0f;

	UPROPERTY(EditAnywhere, Category = "Acquisition", meta = (ClampMin = "0.01", Units = "s"))
	float AcquisitionCheckInterval = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Acquisition")
	TEnumAsByte<ECollisionChannel> LineOfSightTraceChannel = ECC_Visibility;

	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FBallControlFollowConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Control", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxControlledDistance = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Control", meta = (ClampMin = "0.0"))
	float ControlPositionStrength = 35.0f;

	UPROPERTY(EditAnywhere, Category = "Control", meta = (ClampMin = "0.0"))
	float ControlVelocityStrength = 8.0f;

	UPROPERTY(EditAnywhere, Category = "Control", meta = (ClampMin = "0.0"))
	float ControlMaxForce = 40000.0f;

	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FBallControlLaunchConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Launch", meta = (ClampMin = "0.0", Units = "cm/s"))
	float LaunchSpeedIncrement = 2200.0f;

	UPROPERTY(EditAnywhere, Category = "Launch", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1000.0", Units = "cm/s"))
	float BaseRecoilDeltaSpeed = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Launch", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "0.5"))
	float MovingRecoilFraction = 0.15f;

	UPROPERTY(EditAnywhere, Category = "Launch", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1500.0", Units = "cm/s"))
	float MaxRecoilDeltaSpeed = 420.0f;

	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FBallControlForcedReleaseConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "ForcedRelease", meta = (ClampMin = "0.0"))
	float DropCollisionImpulseThreshold = 100000.0f;

	UPROPERTY(EditAnywhere, Category = "ForcedRelease", meta = (ClampMin = "0.0", Units = "s"))
	float DropReacquireLockDuration = 0.75f;

	UPROPERTY(EditAnywhere, Category = "ForcedRelease", meta = (ClampMin = "0.0"))
	float DropBallImpulse = 18000.0f;

	UPROPERTY(EditAnywhere, Category = "ForcedRelease", meta = (ClampMin = "0.0"))
	float DropBallUpwardImpulse = 6000.0f;

	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FBallControlDebugConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawAcquisitionDebug = false;

	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FBallControlConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Configuration")
	FBallControlAcquisitionConfig Acquisition;

	UPROPERTY(EditAnywhere, Category = "Configuration")
	FBallControlFollowConfig Control;

	UPROPERTY(EditAnywhere, Category = "Configuration")
	FBallControlLaunchConfig Launch;

	UPROPERTY(EditAnywhere, Category = "Configuration")
	FBallControlForcedReleaseConfig ForcedRelease;

	UPROPERTY(EditAnywhere, Category = "Configuration")
	FBallControlDebugConfig Debug;

	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FBallStateConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "State", meta = (ClampMin = "0.0", Units = "cm/s"))
	float LowSpeedThreshold = 250.0f;

	UPROPERTY(EditAnywhere, Category = "State", meta = (ClampMin = "0.0", Units = "s"))
	float LowSpeedRequiredDuration = 0.5f;

	UPROPERTY(EditAnywhere, Category = "State", meta = (ClampMin = "0.01", Units = "s"))
	float LowSpeedCheckInterval = 0.05f;

	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FBallAcquisitionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Acquisition", meta = (ClampMin = "0.0", Units = "s"))
	float PostLaunchPickupLockDuration = 0.2f;

	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FBallDamageConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Damage", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "100.0"))
	float VehicleHitDamage = 25.0f;

	UPROPERTY(EditAnywhere, Category = "Damage", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
	float KnockbackStrengthMultiplier = 1.0f;

	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FBallDebugConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bLogDamageHits = false;

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bLogStateChanges = false;

	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FBallGameplayConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Configuration")
	FBallStateConfig State;

	UPROPERTY(EditAnywhere, Category = "Configuration")
	FBallAcquisitionConfig Acquisition;

	UPROPERTY(EditAnywhere, Category = "Configuration")
	FBallDamageConfig Damage;

	UPROPERTY(EditAnywhere, Category = "Configuration")
	FBallDebugConfig Debug;

	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FVehicleInitialHealthConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Health", meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "500.0"))
	float MaxHP = 100.0f;

	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FVehicleHealthDebugConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bLogHealthChanges = false;

	bool Validate(FString& Error) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FVehicleHealthConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Configuration")
	FVehicleInitialHealthConfig Health;

	UPROPERTY(EditAnywhere, Category = "Configuration")
	FVehicleHealthDebugConfig Debug;

	bool Validate(FString& Error) const;
};
