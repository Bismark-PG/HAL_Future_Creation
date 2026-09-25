// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "HAL/CriticalSection.h"
#include "Templates/Atomic.h"
#include "VehicleInputCmd.h"
#include "VehicleNetPhysicsData.h"
#include "ArcadeVehicleMovementComponent.generated.h"

struct FArcadeVehicleConfig;
struct FBodyInstance;

class UPrimitiveComponent;

/**
 * Applies arcade driving forces to a Chaos-simulated primitive.
 *
 * This first MVP-A pass intentionally covers flat-ground driving, grip,
 * steering, and handbrake drifting only. Suspension and landing assistance
 * remain separate future systems.
 */
UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent))
class HAL_FUTURE_CREATION_API UArcadeVehicleMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArcadeVehicleMovementComponent();

	/** Initialization only; never resets gameplay state after BeginPlay. */
	bool ApplyConfiguration(const FArcadeVehicleConfig& Config);

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* Property) const override;
#endif

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;
	virtual void AsyncPhysicsTickComponent(float DeltaTime, float SimTime) override;

	void SetUpdatedPrimitive(UPrimitiveComponent* InPrimitive);
	void SetInputCommand(const FVehicleInputCmd& InInputCommand);
	void SetNetworkInput(const FVehicleNetInputData& InInputData);
	void ResetInputCommand();
	void QueueRecoil(const FVector& DeltaVelocity);
	int32 GetLastPhysicsFrame() const { return LastPhysicsFrame.Load(); }
	bool CaptureNetState(FVehicleNetStateData& OutState) const;
	bool RestoreNetState(const FVehicleNetStateData& State);

	UFUNCTION(BlueprintPure, Category = "Arcade Vehicle|Debug")
	bool IsGrounded() const { return bGrounded; }

	UFUNCTION(BlueprintPure, Category = "Arcade Vehicle|Debug")
	float GetForwardSpeed() const { return ForwardSpeed; }

private:
	friend class FVehicleWallEscapeContactTest;
	void SimulateVehicleStep(const FVehicleNetInputData& StepInput, float DeltaTime);
	bool UpdateGroundContact(const FVector& BodyLocation, FVector& OutGroundNormal) const;
	void UpdateWallEscape(const FTransform& BodyTransform, const FVector& GroundNormal, const FVector& ForwardDirection,
		const FVector& Velocity, float DeltaTime);
	void ResetWallEscape();
	void ApplyLongitudinalForces(FBodyInstance& Body, const FVector& ForwardDirection, float CurrentForwardSpeed);
	void ApplyLateralGrip(FBodyInstance& Body, const FVector& RightDirection, float CurrentLateralSpeed);
	void ApplySteering(FBodyInstance& Body, const FVector& GroundNormal, float CurrentForwardSpeed,
		float CurrentYawRate, float DeltaTime);

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> UpdatedPrimitive;

	FVehicleInputCmd PendingInputCommand;
	uint32 PendingNetworkInputSequence = 0;
	mutable FCriticalSection PendingPhysicsDataLock;
	FVehicleInputCmd InputCommand;
	FVehicleNetInputData LastPhysicsInput;
	TAtomic<int32> LastPhysicsFrame{INDEX_NONE};
	uint32 NextInputSequence = 0;
	FVector PendingRecoilDeltaVelocity = FVector::ZeroVector;
#if !UE_BUILD_SHIPPING
	bool bLoggedFirstPhysicsStep = false;
#endif

	UPROPERTY(VisibleInstanceOnly, Category = "Arcade Vehicle|Debug")
	bool bGrounded = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Arcade Vehicle|Debug", meta = (Units = "cm/s"))
	float ForwardSpeed = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Drive", meta = (ClampMin = "0.0", Units = "cm/s^2"))
	float ForwardAcceleration = 1800.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Drive", meta = (ClampMin = "0.0", Units = "cm/s^2"))
	float ReverseAcceleration = 1200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Drive", meta = (ClampMin = "0.0", Units = "cm/s^2"))
	float BrakeDeceleration = 2800.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Drive", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaxForwardSpeed = 2400.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Drive", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaxReverseSpeed = 900.0f;

	/** Below this forward speed, Brake begins applying reverse acceleration. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Drive", meta = (ClampMin = "0.0", Units = "cm/s"))
	float ReverseEngageSpeed = 80.0f;

	/** Velocity-proportional coasting drag, in inverse seconds. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Drive", meta = (ClampMin = "0.0"))
	float CoastingDragRate = 0.35f;

	/** Extra correction applied only after a configured speed limit is exceeded. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Drive", meta = (ClampMin = "0.0"))
	float OverspeedCorrectionRate = 3.0f;

	/** Lateral velocity removed per second while driving normally. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Grip", meta = (ClampMin = "0.0"))
	float LateralGripRate = 8.0f;

	/** Lateral velocity removed per second while the handbrake is held. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Grip", meta = (ClampMin = "0.0"))
	float HandbrakeGripRate = 1.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Grip", meta = (ClampMin = "0.0", Units = "cm/s^2"))
	float MaxLateralGripAcceleration = 4500.0f;

	/** Maximum yaw angular acceleration in radians per second squared. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Steering", meta = (ClampMin = "0.0"))
	float SteeringAngularAcceleration = 4.5f;

	/** Vehicle speed at which normal steering reaches full authority. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Steering", meta = (ClampMin = "1.0", Units = "cm/s"))
	float FullSteeringSpeed = 800.0f;

	/** Steering fades to HighSpeedSteeringScale above this speed. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Steering", meta = (ClampMin = "0.0", Units = "cm/s"))
	float HighSpeedSteeringStart = 1600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Steering", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HighSpeedSteeringScale = 0.45f;

	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Steering", meta = (ClampMin = "0.0"))
	float HandbrakeSteeringMultiplier = 1.45f;

	/** Minimum speed-derived steering authority while handbraking; allows grounded pivots at rest. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Steering", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HandbrakeMinimumSteeringAuthority = 0.5f;

	/** While handbraking, reverse steering starts only below this backward speed to ignore small rebounds. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Steering", meta = (ClampMin = "0.0", UIMax = "500.0", Units = "cm/s"))
	float HandbrakeReverseSteeringSpeed = 150.0f;

	/** Normal yaw-rate damping per second. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Steering", meta = (ClampMin = "0.0"))
	float YawDampingRate = 2.2f;

	/** Reduced yaw-rate damping per second while drifting. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Steering", meta = (ClampMin = "0.0"))
	float HandbrakeYawDampingRate = 0.8f;

	/** Assist grounded, upright, low-speed throttle + handbrake + steering near static walls. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Wall Escape")
	bool bEnableWallEscape = true;

	/** Maximum planar speed for entering the assist (includes lateral sliding). */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Wall Escape", meta = (ClampMin = "0.0", Units = "cm/s"))
	float WallEscapeEnterSpeed = 300.0f;

	/** Exit threshold; treated as at least EnterSpeed to avoid threshold chatter. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Wall Escape", meta = (ClampMin = "0.0", Units = "cm/s"))
	float WallEscapeExitSpeed = 450.0f;

	/** Sweep the actual simple collision geometry this far forward to detect a nearby wall. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Wall Escape", meta = (ClampMin = "1.0", ClampMax = "100.0", Units = "cm"))
	float WallEscapeProbeDistance = 15.0f;

	/** Ignore small steering-stick noise. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Wall Escape", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float WallEscapeMinimumSteering = 0.2f;

	/** Fraction of throttle acceleration retained INTO a wall at full assist; tangent is preserved. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Wall Escape", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WallEscapeIntoWallThrottleScale = 0.05f;

	/** Desired yaw rate at full steering, in radians per second; not a hard physics velocity lock. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Wall Escape", meta = (ClampMin = "0.0", UIMax = "6.0"))
	float WallEscapeTargetYawRate = 2.6f;

	/** Yaw-rate tracking gain in inverse seconds. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Wall Escape", meta = (ClampMin = "0.0", UIMax = "30.0"))
	float WallEscapeYawResponseRate = 10.0f;

	/** Maximum assisted yaw correction, in radians per second squared. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Wall Escape", meta = (ClampMin = "0.0", UIMax = "40.0"))
	float WallEscapeMaxYawAcceleration = 18.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Wall Escape", meta = (ClampMin = "0.0", UIMax = "1.0", Units = "s"))
	float WallEscapeBlendInTime = 0.08f;

	/** Restore regular driving smoothly after releasing the combo, losing the wall or gaining speed. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Wall Escape", meta = (ClampMin = "0.0", UIMax = "1.0", Units = "s"))
	float WallEscapeBlendOutTime = 0.2f;

	UPROPERTY(VisibleInstanceOnly, Category = "Arcade Vehicle|Debug")
	bool bWallEscapeActive = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Arcade Vehicle|Debug")
	float WallEscapeBlend = 0.0f;

	TArray<FVector> WallEscapeNormals;

	/** Cyan: probe; green normals: active; orange normals: fading out. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Debug")
	bool bDrawWallEscapeDebug = false;

	/** Extra trace distance below the physical body's bounds. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Ground", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundTraceExtraDistance = 30.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Ground")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Debug")
	bool bDrawGroundDebug = false;
};
