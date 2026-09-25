// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "VehicleInputCmd.h"
#include "VehicleNetPhysicsData.h"
#include "VehicleConfigurationTypes.h"
#include "TestVehiclePawn.generated.h"

class UArcadeVehicleMovementComponent;
class UArrowComponent;
class UBallControlComponent;
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
class UVehicleDefinition;

UCLASS()
class HAL_FUTURE_CREATION_API ATestVehiclePawn : public APawn
{
	GENERATED_BODY()

public:
	ATestVehiclePawn();

	virtual void PreInitializeComponents() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	EVehicleConfigurationSource GetConfigurationSource() const { return ConfigurationSource; }
	bool IsConfigurationValid() const { return bConfigurationValid; }
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* Property) const override;
#endif

	virtual void BeginPlay() override;
	virtual void PawnClientRestart() override;
	virtual void UnPossessed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintPure, Category = "Vehicle|Input")
	FVehicleInputCmd GetCurrentInputCommand() const { return CurrentInputCommand; }

protected:
	UPROPERTY(EditAnywhere, Category = "Vehicle|Configuration")
	EVehicleConfigurationSource ConfigurationSource = EVehicleConfigurationSource::Legacy;

	UPROPERTY(EditAnywhere, Category = "Vehicle|Configuration", meta = (EditCondition = "ConfigurationSource == EVehicleConfigurationSource::Definition"))
	TObjectPtr<UVehicleDefinition> Definition;

	UPROPERTY(VisibleInstanceOnly, Category = "Vehicle|Configuration")
	bool bConfigurationValid = true;

	bool ApplyDefinition(bool bPreview);

	/** Hidden physics mesh. Assign a centered, low-profile mesh with simple convex collision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Components")
	TObjectPtr<UStaticMeshComponent> CollisionRoot;

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

	/** Unreliable client input snapshots per second until Network Physics history is wired in phase 4. */
	UPROPERTY(EditDefaultsOnly, Category = "Vehicle|Network", meta = (ClampMin = "1.0", ClampMax = "60.0", Units = "Hz"))
	float ClientInputSendRateHz = 30.0f;

	/** Server releases a held input after this many seconds without a fresh snapshot. */
	UPROPERTY(EditDefaultsOnly, Category = "Vehicle|Network", meta = (ClampMin = "0.1", ClampMax = "1.0", Units = "s"))
	float RemoteInputTimeoutSeconds = 0.25f;

private:
	void AddDefaultInputContext();
	void RemoveDefaultInputContext();
	void PushInputCommand();
	void ResetInputCommand();
	void SendInputSnapshot();
	void ExpireRemoteInput();
	void ProcessLaunchRequest(const FVehicleLaunchRequest& Request);

	UFUNCTION(Server, Unreliable)
	void ServerSubmitVehicleInput(const FVehicleNetInputData& Input);

	UFUNCTION(Server, Reliable)
	void ServerRequestLaunch(const FVehicleLaunchRequest& Request);

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

	FTimerHandle InputSendTimerHandle;
	FTimerHandle RemoteInputTimeoutHandle;
	uint32 NextClientInputSequence = 0;
	uint32 LastAcceptedServerInputSequence = 0;
	uint32 NextLaunchSequence = 0;
	uint32 LastProcessedLaunchSequence = 0;
};
