// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BasicBallActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EBasicBallState : uint8
{
	Free,
	Controlled,
	Launched
};

/** Lightweight Chaos ball with an explicit, authority-owned gameplay state. */
UCLASS()
class HAL_FUTURE_CREATION_API ABasicBallActor : public AActor
{
	GENERATED_BODY()

public:
	ABasicBallActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Ball|State")
	EBasicBallState GetBallState() const { return BallState; }

	UFUNCTION(BlueprintPure, Category = "Ball|State")
	AActor* GetControlledBy() const { return ControlledBy; }

	UFUNCTION(BlueprintPure, Category = "Ball|State")
	AActor* GetLaunchedBy() const { return LaunchedBy; }

	USphereComponent* GetPhysicsRoot() const { return PhysicsRoot; }

	bool CanBeControlledBy(const AActor* CandidateVehicle) const;
	bool BeginControl(AActor* NewHolder);
	bool LaunchFromControl(AActor* ExpectedHolder, AActor* NewLauncher, const FVector& InitialVelocity);
	bool ReleaseFromControl(AActor* ExpectedHolder, float ReacquireLockDuration);
	void AddReleaseImpulse(const FVector& Impulse);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball|Components")
	TObjectPtr<USphereComponent> PhysicsRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball|Components")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	/** Speed must stay below this value before Launched can return to Free. */
	UPROPERTY(EditDefaultsOnly, Category = "Ball|State", meta = (ClampMin = "0.0", Units = "cm/s"))
	float LowSpeedThreshold = 250.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Ball|State", meta = (ClampMin = "0.0", Units = "s"))
	float LowSpeedRequiredDuration = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Ball|State", meta = (ClampMin = "0.01", Units = "s"))
	float LowSpeedCheckInterval = 0.05f;

	/** Brief global lock after Launched becomes Free. */
	UPROPERTY(EditDefaultsOnly, Category = "Ball|Acquisition", meta = (ClampMin = "0.0", Units = "s"))
	float PostLaunchPickupLockDuration = 0.2f;

	/** Fixed damage for the first valid Launched vehicle hit, in HP. */
	UPROPERTY(EditDefaultsOnly, Category = "Ball|Damage", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "100.0"))
	float VehicleHitDamage = 25.0f;

	/** Multiplies impact strength before the global knockback tier is selected. */
	UPROPERTY(EditDefaultsOnly, Category = "Ball|Damage", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
	float KnockbackStrengthMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Ball|Debug")
	bool bLogDamageHits = false;

	UPROPERTY(EditDefaultsOnly, Category = "Ball|Debug")
	bool bLogStateChanges = false;

private:
	UFUNCTION()
	void OnRep_BallState();

	void SetBallState(EBasicBallState NewState);
	void StartLowSpeedMonitor();
	void StopLowSpeedMonitor();
	void CheckLaunchedLowSpeed();
	void FinishLaunchAsFree();

	UFUNCTION()
	void OnPhysicsRootHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

	UPROPERTY(ReplicatedUsing = OnRep_BallState, VisibleInstanceOnly, Category = "Ball|State")
	EBasicBallState BallState = EBasicBallState::Free;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Ball|State")
	TObjectPtr<AActor> ControlledBy;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Ball|State")
	TObjectPtr<AActor> LaunchedBy;

	TWeakObjectPtr<AActor> ReacquireLockedVehicle;
	double ReacquireLockedUntil = 0.0;
	double GlobalPickupLockedUntil = 0.0;
	float AccumulatedLowSpeedTime = 0.0f;
	FTimerHandle LowSpeedTimerHandle;
	bool bResolvingDamageHit = false;
};
