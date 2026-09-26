// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VehicleConfigurationTypes.h"
#include "BasicBallActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;
class UNetworkPhysicsSettingsComponent;

class UBallDefinition;

UENUM(BlueprintType)
enum class EBasicBallState : uint8
{
	Free,
	Controlled,
	Launched
};

/** One authority-written snapshot; movement replication remains independent of gameplay ownership. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FBallRepState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ball|State")
	EBasicBallState State = EBasicBallState::Free;

	UPROPERTY(BlueprintReadOnly, Category = "Ball|State")
	TObjectPtr<AActor> Holder;

	/** The pawn is optional after disconnect; PlayerId remains the stable attribution for this launch. */
	UPROPERTY(BlueprintReadOnly, Category = "Ball|State")
	TObjectPtr<AActor> LastLauncherPawn;

	UPROPERTY(BlueprintReadOnly, Category = "Ball|State")
	int32 LastLauncherPlayerId = INDEX_NONE;

	UPROPERTY()
	uint32 StateSequence = 0;

	/** INDEX_NONE when no vehicle physics frame is available at the transition. */
	UPROPERTY(BlueprintReadOnly, Category = "Ball|State")
	int32 ServerPhysicsFrame = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Ball|State")
	TObjectPtr<AActor> ReacquireLockedVehicle;

	UPROPERTY(BlueprintReadOnly, Category = "Ball|State")
	float ReacquireLockEndServerTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ball|State")
	float GlobalPickupLockEndServerTime = 0.0f;
};

/** Lightweight Chaos ball with an explicit, authority-owned gameplay state. */
UCLASS()
class HAL_FUTURE_CREATION_API ABasicBallActor : public AActor
{
	GENERATED_BODY()

public:
	ABasicBallActor();

	virtual void PreInitializeComponents() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	EVehicleConfigurationSource GetConfigurationSource() const { return ConfigurationSource; }
	bool IsConfigurationValid() const { return bConfigurationValid; }
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* Property) const override;
#endif

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Ball|State")
	EBasicBallState GetBallState() const { return RepState.State; }

	UFUNCTION(BlueprintPure, Category = "Ball|State")
	AActor* GetControlledBy() const { return RepState.Holder; }

	UFUNCTION(BlueprintPure, Category = "Ball|State")
	AActor* GetLaunchedBy() const { return RepState.LastLauncherPawn; }

	UFUNCTION(BlueprintPure, Category = "Ball|State")
	FBallRepState GetBallRepState() const { return RepState; }
	uint32 GetBallStateSequence() const { return RepState.StateSequence; }

	USphereComponent* GetPhysicsRoot() const { return PhysicsRoot; }

	bool CanBeControlledBy(const AActor* CandidateVehicle) const;
	bool BeginControl(AActor* NewHolder);
	bool LaunchFromControl(AActor* ExpectedHolder, AActor* NewLauncher, const FVector& InitialVelocity);
	bool ReleaseFromControl(AActor* ExpectedHolder, float ReacquireLockDuration);
	void AddReleaseImpulse(const FVector& Impulse);

protected:
	UPROPERTY(EditAnywhere, Category = "Ball|Configuration")
	EVehicleConfigurationSource ConfigurationSource = EVehicleConfigurationSource::Legacy;

	UPROPERTY(EditAnywhere, Category = "Ball|Configuration", meta = (EditCondition = "ConfigurationSource == EVehicleConfigurationSource::Definition"))
	TObjectPtr<UBallDefinition> Definition;

	UPROPERTY(VisibleInstanceOnly, Category = "Ball|Configuration")
	bool bConfigurationValid = true;

	bool ApplyDefinition(bool bPreview);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball|Components")
	TObjectPtr<USphereComponent> PhysicsRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball|Components")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	/** Optional per-ball PI tuning asset. Its mode override applies to simulated proxies. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball|Components")
	TObjectPtr<UNetworkPhysicsSettingsComponent> NetworkPhysicsSettings;

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
	void OnRep_BallRepState();

	void CommitRepState(const FBallRepState& NewState, const AActor* PhysicsFrameSource);
	void ApplyReplicatedPresentation();
	void StopControlledPresentation();
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

	UPROPERTY(ReplicatedUsing = OnRep_BallRepState, VisibleInstanceOnly, Category = "Ball|State")
	FBallRepState RepState;
	UPROPERTY(Transient)
	FBallRepState LastAppliedRepState;

	ECollisionEnabled::Type AuthoredCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	bool bAuthoredSimulatePhysics = true;
	bool bControlledPresentationActive = false;
	bool bHasControlledPresentationTarget = false;
	FVector LastControlledPresentationLocation = FVector::ZeroVector;
	FQuat ControlledPresentationRotation = FQuat::Identity;
	bool bHasAppliedRepState = false;
	uint32 LastAppliedStateSequence = 0;
	float AccumulatedLowSpeedTime = 0.0f;
	FTimerHandle LowSpeedTimerHandle;
	bool bResolvingDamageHit = false;
};
