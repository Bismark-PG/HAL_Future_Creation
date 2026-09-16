// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "VehicleInputCmd.h"
#include "TestVehiclePawn.generated.h"

class UArcadeVehicleMovementComponent;
class UArrowComponent;
class UBallControlComponent;
class UBoxComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UPhysicsConstraintComponent;
class UPrimitiveComponent;
class USceneComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UVehicleHealthComponent;
struct FInputActionValue;

/** Minimal player vehicle used to validate MVP-A ground handling. */
UCLASS()
class HAL_FUTURE_CREATION_API ATestVehiclePawn : public APawn
{
	GENERATED_BODY()

public:
	ATestVehiclePawn();

	virtual void PawnClientRestart() override;
	virtual void UnPossessed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintPure, Category = "Vehicle|Input")
	FVehicleInputCmd GetCurrentInputCommand() const { return CurrentInputCommand; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Components")
	TObjectPtr<UBoxComponent> CollisionRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Components")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	/** Visible MVP marker for the physical forward / future launch direction. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Components")
	TObjectPtr<UArrowComponent> ForwardArrow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Components")
	TObjectPtr<UArcadeVehicleMovementComponent> ArcadeMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Components")
	TObjectPtr<USceneComponent> BallControlPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Components")
	TObjectPtr<UPhysicsConstraintComponent> BallConstraint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Components")
	TObjectPtr<UBallControlComponent> BallControl;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Components")
	TObjectPtr<UVehicleHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Components")
	TObjectPtr<UCameraComponent> TopDownCamera;

	UPROPERTY(EditDefaultsOnly, Category = "Vehicle|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Vehicle|Input")
	TObjectPtr<UInputAction> SteeringAction;

	UPROPERTY(EditDefaultsOnly, Category = "Vehicle|Input")
	TObjectPtr<UInputAction> ThrottleAction;

	UPROPERTY(EditDefaultsOnly, Category = "Vehicle|Input")
	TObjectPtr<UInputAction> BrakeAction;

	UPROPERTY(EditDefaultsOnly, Category = "Vehicle|Input")
	TObjectPtr<UInputAction> HandbrakeAction;

	UPROPERTY(EditDefaultsOnly, Category = "Vehicle|Input")
	TObjectPtr<UInputAction> LaunchAction;

private:
	void AddDefaultInputContext();
	void RemoveDefaultInputContext();
	void PushInputCommand();
	void ResetInputCommand();

	void OnSteeringInput(const FInputActionValue& Value);
	void OnSteeringCompleted(const FInputActionValue& Value);
	void OnThrottleStarted(const FInputActionValue& Value);
	void OnThrottleCompleted(const FInputActionValue& Value);
	void OnBrakeStarted(const FInputActionValue& Value);
	void OnBrakeCompleted(const FInputActionValue& Value);
	void OnHandbrakeStarted(const FInputActionValue& Value);
	void OnHandbrakeCompleted(const FInputActionValue& Value);
	void OnLaunchStarted(const FInputActionValue& Value);
	void OnLaunchCompleted(const FInputActionValue& Value);

	UFUNCTION()
	void OnCollisionRootHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

	UPROPERTY(VisibleInstanceOnly, Category = "Vehicle|Input")
	FVehicleInputCmd CurrentInputCommand;
};
