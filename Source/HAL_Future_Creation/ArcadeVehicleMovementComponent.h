// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "VehicleInputCmd.h"
#include "ArcadeVehicleMovementComponent.generated.h"

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

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	void SetUpdatedPrimitive(UPrimitiveComponent* InPrimitive);
	void SetInputCommand(const FVehicleInputCmd& InInputCommand);
	void ResetInputCommand();

	UFUNCTION(BlueprintPure, Category = "Arcade Vehicle|Debug")
	bool IsGrounded() const { return bGrounded; }

	UFUNCTION(BlueprintPure, Category = "Arcade Vehicle|Debug")
	float GetForwardSpeed() const { return ForwardSpeed; }

private:
	bool UpdateGroundContact(FVector& OutGroundNormal) const;
	void ApplyLongitudinalForces(const FVector& ForwardDirection, float CurrentForwardSpeed);
	void ApplyLateralGrip(const FVector& RightDirection, float CurrentLateralSpeed);
	void ApplySteering(const FVector& GroundNormal, float CurrentForwardSpeed);

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> UpdatedPrimitive;

	FVehicleInputCmd InputCommand;

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

	/** Normal yaw-rate damping per second. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Steering", meta = (ClampMin = "0.0"))
	float YawDampingRate = 2.2f;

	/** Reduced yaw-rate damping per second while drifting. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Steering", meta = (ClampMin = "0.0"))
	float HandbrakeYawDampingRate = 0.8f;

	/** Extra trace distance below the physical body's bounds. */
	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Ground", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundTraceExtraDistance = 30.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Ground")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, Category = "Arcade Vehicle|Debug")
	bool bDrawGroundDebug = false;
};
