// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "BallControlComponent.generated.h"

struct FBallControlConfig;

class ABasicBallActor;
class UPhysicsConstraintComponent;
class UPrimitiveComponent;
class USceneComponent;

/** Vehicle-side ball acquisition, control, launch, and forced-release logic. */
UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent))
class HAL_FUTURE_CREATION_API UBallControlComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBallControlComponent();

	/** Initialization only; never resets gameplay state after BeginPlay. */
	bool ApplyConfiguration(const FBallControlConfig& Config);

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* Property) const override;
#endif

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void SetVehicleComponents(
		UPrimitiveComponent* InVehicleBody,
		USceneComponent* InControlPoint,
		UPhysicsConstraintComponent* InBallConstraint);

	bool LaunchHeldBall();
	void HandleVehicleCollision(const FVector& NormalImpulse, const FHitResult& Hit);

	UFUNCTION(BlueprintPure, Category = "Ball Control|State")
	ABasicBallActor* GetHeldBall() const { return HeldBall; }

	/** Read-only target for the remote ball's collisionless presentation follower. */
	bool GetControlTargetWorldTransform(FTransform& OutTransform) const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Ball Control|Acquisition", meta = (ClampMin = "0.0", Units = "cm"))
	float AcquisitionRadius = 350.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Ball Control|Acquisition", meta = (ClampMin = "0.01", Units = "s"))
	float AcquisitionCheckInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Ball Control|Acquisition")
	TEnumAsByte<ECollisionChannel> LineOfSightTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, Category = "Ball Control|Control", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxControlledDistance = 300.0f;

	/** Constraint drive position strength. */
	UPROPERTY(EditDefaultsOnly, Category = "Ball Control|Control", meta = (ClampMin = "0.0"))
	float ControlPositionStrength = 35.0f;

	/** Constraint drive velocity damping. */
	UPROPERTY(EditDefaultsOnly, Category = "Ball Control|Control", meta = (ClampMin = "0.0"))
	float ControlVelocityStrength = 8.0f;

	/** Maximum force available to the soft control constraint. */
	UPROPERTY(EditDefaultsOnly, Category = "Ball Control|Control", meta = (ClampMin = "0.0"))
	float ControlMaxForce = 40000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Ball Control|Launch", meta = (ClampMin = "0.0", Units = "cm/s"))
	float LaunchSpeedIncrement = 2200.0f;

	/** Minimum backward velocity change applied to the vehicle when launching. */
	UPROPERTY(EditDefaultsOnly, Category = "Ball Control|Launch", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1000.0", Units = "cm/s"))
	float BaseRecoilDeltaSpeed = 180.0f;

	/** At speed, recoil is at least this fraction of the current forward speed. */
	UPROPERTY(EditDefaultsOnly, Category = "Ball Control|Launch", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "0.5"))
	float MovingRecoilFraction = 0.15f;

	/** Caps speed-scaled recoil so launching never behaves like an abrupt wall impact. */
	UPROPERTY(EditDefaultsOnly, Category = "Ball Control|Launch", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1500.0", Units = "cm/s"))
	float MaxRecoilDeltaSpeed = 420.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Ball Control|Forced Release", meta = (ClampMin = "0.0"))
	float DropCollisionImpulseThreshold = 100000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Ball Control|Forced Release", meta = (ClampMin = "0.0", Units = "s"))
	float DropReacquireLockDuration = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category = "Ball Control|Forced Release", meta = (ClampMin = "0.0"))
	float DropBallImpulse = 18000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Ball Control|Forced Release", meta = (ClampMin = "0.0"))
	float DropBallUpwardImpulse = 6000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Ball Control|Debug")
	bool bDrawAcquisitionDebug = false;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FConfigurationPlayerBallTest;
#endif
	void EvaluateBallControl();
	ABasicBallActor* FindBestCandidate() const;
	bool HasLineOfSightTo(const ABasicBallActor* Candidate) const;
	bool AcquireBall(ABasicBallActor* Candidate);
	void ConfigureConstraint(ABasicBallActor* Ball);
	void ReleaseConstraint();
	void ReleaseHeldBall(const FVector& ReleaseDirection, bool bApplyReleaseImpulse);
	void DrawAcquisitionDebug(const ABasicBallActor* BestCandidate) const;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> VehicleBody;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> ControlPoint;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicsConstraintComponent> BallConstraint;

	UPROPERTY(VisibleInstanceOnly, Category = "Ball Control|State")
	TObjectPtr<ABasicBallActor> HeldBall;

	bool bConstraintActive = false;
	FTimerHandle AcquisitionTimerHandle;
};
