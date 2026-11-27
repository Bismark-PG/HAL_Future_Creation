// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VehicleConfigurationTypes.h"
#include "HALCombatTypes.h"
#include "BasicBallActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;
class UNetworkPhysicsSettingsComponent;
class UHALBallPresentationComponent;
class UHALMagnetBeamPresentationComponent;
class UHALBallComicFXComponent;
class AHALTornado;
class AHALElectricWallGimmick;

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
	/** Captured on authority launch; travels with the atomic ball gameplay state. */
	UPROPERTY(BlueprintReadOnly, Category = "Ball|State")
	FHALAttackSourceSnapshot AttackSnapshot;
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
	virtual float GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, AActor* Viewer,
		AActor* ViewTarget, UActorChannel* InChannel, float Time, bool bLowBandwidth) override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "Ball|State")
	EBasicBallState GetBallState() const { return RepState.State; }

	UFUNCTION(BlueprintPure, Category = "Ball|State")
	AActor* GetControlledBy() const { return RepState.Holder; }

	UFUNCTION(BlueprintPure, Category = "Ball|State")
	AActor* GetLaunchedBy() const { return RepState.LastLauncherPawn; }

	UFUNCTION(BlueprintPure, Category = "Ball|State")
	FBallRepState GetBallRepState() const { return RepState; }
	uint32 GetBallStateSequence() const { return RepState.StateSequence; }

	/** Authority-only, ordinary Launched balls, accepted wall query; no ASC is added to the ball. */
	bool TryElectrifyFromWall(const AHALElectricWallGimmick& Wall);
	UFUNCTION(BlueprintPure, Category="Ball|Electric") bool IsElectrified() const { return ElectricCharge.bActive; }
	UFUNCTION(BlueprintPure, Category="Ball|Electric") FHALBallElectricCharge GetElectricCharge() const { return ElectricCharge; }
	USphereComponent* GetPhysicsRoot() const { return PhysicsRoot; }
	/** Radius of a fresh actor of this authored class, including the configured physics scale. */
	float GetConfiguredSphereRadius() const;
	UStaticMeshComponent* GetVisualMesh() const { return VisualMesh; }
	/** Optional world heading for a directional mesh. Zero keeps the normal rolling-ball presentation. */
	virtual FVector GetVisualFacingDirection() const { return FVector::ZeroVector; }
	bool IsPrimaryMatchBall() const { return bPrimaryCameraTarget; }
	void SetPrimaryMatchBall(bool bPrimary);
	bool IsPrimaryCameraTarget() const { return IsPrimaryMatchBall(); }
	void SetPrimaryCameraTarget(bool bPrimary) { SetPrimaryMatchBall(bPrimary); }

	bool CanBeControlledBy(const AActor* CandidateVehicle) const;
	virtual bool AllowsNormalLaunch() const { return true; }
	/** Ballistic shots use gravity, the low-speed return to Free, and the common ink trail. */
	virtual bool UsesStandardFlight() const { return true; }
	virtual bool AllowsAutomaticPickup() const { return true; }
	bool BeginControl(AActor* NewHolder);
	bool LaunchFromControl(AActor* ExpectedHolder, AActor* NewLauncher, const FVector& InitialVelocity);
	bool LaunchFreeFromBattleTop(AActor* NewLauncher, const FVector& InitialVelocity);
	bool LaunchFreeFromVehicleSkill(AActor* NewLauncher, const FVector& InitialVelocity);
	/** Narrow authority capability: only the tornado's current accepted contact may call this. */
	bool DeflectByTornado(AHALTornado& Source, const FVector& InitialVelocity);
	bool LaunchFromWaterCannon(class AHALWaterCannon& Source, const FVector& Velocity);
	virtual bool ReleaseFromControl(AActor* ExpectedHolder, float ReacquireLockDuration);
	void AddReleaseImpulse(const FVector& Impulse);
	/** Server cleanup before a vehicle Pawn is destroyed; preserves a launched ball's stable PlayerId. */
	virtual bool HandleVehicleExit(const AActor* LeavingVehicle);

protected:
	// Contact-carried balls change holder directly after both vehicle slots are ready.
	void CommitContactHolder(AActor& Holder);
	bool PassesPickupLocks(const AActor* CandidateVehicle) const;
	/** Marks the permanent match ball and camera focus. Keep this property name for existing maps. */
	UPROPERTY(EditInstanceOnly, Replicated, Category="Ball|Camera") bool bPrimaryCameraTarget = false;
	UPROPERTY(EditAnywhere, Category = "Ball|Configuration")
	EVehicleConfigurationSource ConfigurationSource = EVehicleConfigurationSource::Legacy;

	UPROPERTY(EditAnywhere, Category = "Ball|Configuration", meta = (EditCondition = "ConfigurationSource == EVehicleConfigurationSource::Definition"))
	TObjectPtr<UBallDefinition> Definition;

	UPROPERTY(VisibleInstanceOnly, Category = "Ball|Configuration")
	bool bConfigurationValid = true;

	bool ApplyDefinition(bool bPreview);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool CanUseBall() const { return true; }
	virtual bool AllowsElectricWallCharge() const { return true; }
	/** Pure presentation; called on authority and when the charge snapshot replicates. */
	UFUNCTION(BlueprintImplementableEvent, Category="Ball|Electric") void OnElectricChargeChanged(bool bCharged);
	/** Called once after a formal shot has captured its source. False keeps the ball with its current holder. */
	virtual bool DeliverLaunch(const FHALAttackSourceSnapshot& Attack, AActor* Launcher, const FVector& Velocity, const AActor* FrameSource);
	virtual void OnAuthorityBallStateChanged(EBasicBallState Previous) {}
	virtual void ResolveLaunchedContact(AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
	virtual void OnAcceptedVehicleHit();
	FHALCombatAction& GetLaunchAction() { return LaunchAction; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball|Components")
	TObjectPtr<USphereComponent> PhysicsRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball|Components")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	/** Optional per-ball PI tuning asset. Its mode override applies to simulated proxies. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball|Components")
	TObjectPtr<UNetworkPhysicsSettingsComponent> NetworkPhysicsSettings;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ball|Components")
	TObjectPtr<UHALBallPresentationComponent> BallPresentation;

	/** Ownership presentation: holder-coloured roof beam and shell pulse. Visual only. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ball|Components")
	TObjectPtr<UHALMagnetBeamPresentationComponent> MagnetBeam;

	/** Doodle readability layers: launched ink trail and held aim arrow. Presentation only. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball|Presentation")
	TObjectPtr<UHALBallComicFXComponent> ComicFX;

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
#if WITH_DEV_AUTOMATION_TESTS
	friend class FHALImpactBallPipelineTest;
	friend class FControlledBallProxyPresentationTest;
	friend class FHALMatchBallGroundRollTest;
	friend class FHALCombatBallConsumptionTest;
	friend class FHALChargedBallCombatTest;
	friend struct FHALBallPresentationTestAccess;
#endif
	UFUNCTION()
	void OnRep_BallRepState();

	void CommitRepState(const FBallRepState& NewState, const AActor* PhysicsFrameSource);
	bool CommitLaunch(AActor* NewLauncher, const FVector& InitialVelocity, const AActor* FrameSource);
	void ApplyReplicatedPresentation();
	void StopControlledPresentation();
	void StartLowSpeedMonitor();
	void StopLowSpeedMonitor();
	void CheckLaunchedLowSpeed();
	bool HasGroundSupport() const;
	void FinishLaunchAsFree(float PickupLockSeconds);

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

	FCollisionResponseContainer AuthoredCollisionResponses;
	bool bAuthoredGravityEnabled = true;
	bool bControlledPresentationActive = false;
	bool bHasAppliedRepState = false;
	uint32 LastAppliedStateSequence = 0;
	float AccumulatedLowSpeedTime = 0.0f;
	double GroundRollStartedAt = -1.;
	FTimerHandle LowSpeedTimerHandle;
	bool bResolvingDamageHit = false;
	uint32 LastResolvedHitEventSequence = 0;
	uint64 LastBecameFreeGameFrame = MAX_uint64;
	// Net driver clock, shared with UActorChannel::LastUpdateTime. Each observer gets its own release update.
	double LastControlEndNetSeconds = -1.0;
	UFUNCTION() void OnRep_ElectricCharge();
	void ConsumeElectricCharge();
	void ProbeElectricWalls(const FVector& End);
	FVector LastElectricProbePosition=FVector::ZeroVector;
	UPROPERTY(ReplicatedUsing=OnRep_ElectricCharge) FHALBallElectricCharge ElectricCharge;
	FBallDamageConfig RuntimeDamageConfig;
	FHALAuthoredImpactConfig RuntimeImpactConfig;
	FHALCombatAction LaunchAction;
};
