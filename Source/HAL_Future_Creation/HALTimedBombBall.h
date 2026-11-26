#pragma once

#include "HALSpecialBall.h"
#include "HALTimedBombBall.generated.h"

class ATestVehiclePawn;
namespace Chaos { struct FCollisionEventData; }

UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALTimedBombBallDefinition : public UHALSpecialBallDefinition
{
	GENERATED_BODY()
public:
	/** Starts on first vehicle contact. Every new carrier inherits the same deadline. */
	UPROPERTY(EditAnywhere, Category="Fuse", meta=(ClampMin=".25", ClampMax="120", Units="s")) float FuseSeconds = 8.f;
	UPROPERTY(EditAnywhere, Category="Fuse", meta=(ClampMin="0", ClampMax="120", Units="s")) float UrgentSeconds = 3.f;
	/** Ground pulse reaches the closest point of vehicle bodies inside this radius. */
	UPROPERTY(EditAnywhere, Category="Fuse", meta=(ClampMin="1", ClampMax="5000", Units="cm")) float GroundBlastRadius = 500.f;
	UPROPERTY(EditAnywhere, Category="Patrol", meta=(ClampMin="0", ClampMax="4000", Units="cm/s")) float PatrolSpeed = 650.f;
	UPROPERTY(EditAnywhere, Category="Patrol", meta=(ClampMin="0", ClampMax="180", Units="deg/s")) float PatrolTurnRate = 26.f;
	UPROPERTY(EditAnywhere, Category="Transfer", meta=(ClampMin="1", ClampMax="5000", Units="cm/s")) float MinimumRamSpeed = 220.f;
	UPROPERTY(EditAnywhere, Category="Transfer", meta=(ClampMin="1", ClampMax="5000", Units="cm/s")) float MinimumClosingSpeed = 140.f;
	UPROPERTY(EditAnywhere, Category="Transfer", meta=(ClampMin="0", ClampMax="5", Units="s")) float ReturnLockSeconds = .7f;
	UPROPERTY(EditAnywhere, Category="Transfer|Appearance", meta=(ClampMin=".12", ClampMax="1.5", Units="s")) float ClaspTravelSeconds = .42f;
	/** Maximum outward travel of each jaw as a fraction of the ball radius. */
	UPROPERTY(EditAnywhere, Category="Transfer|Appearance", meta=(ClampMin=".05", ClampMax=".8")) float JawOpening = .32f;
	UPROPERTY(EditAnywhere, Category="Dial", meta=(ClampMin="4", ClampMax="24")) int32 DialSegments = 12;
	UPROPERTY(EditAnywhere, Category="Dial") FLinearColor PoleTipColor = FLinearColor(.98f,.83f,.59f);
	UPROPERTY(EditAnywhere, Category="Dial") TObjectPtr<UStaticMesh> JawMesh;
	UPROPERTY(EditAnywhere, Category="Dial") TObjectPtr<UStaticMesh> DialMesh;
	/** Radius of the visible inner body relative to the physical ball radius. */
	UPROPERTY(EditAnywhere, Category="Cage", meta=(ClampMin=".52", ClampMax=".88")) float InnerCoreRadiusRatio = .77f;
	UPROPERTY(EditAnywhere, Category="Cage") FLinearColor CageSteelColor = FLinearColor(.19f,.175f,.20f);
	/** Clock and magnetic pole projection at the spawn site. An empty slot uses the common arrival pulse. */
	UPROPERTY(EditAnywhere, Category="Arrival|Appearance") TObjectPtr<UMaterialInterface> ClaspArrivalMaterial;
	/** How far each half of the visible cage starts open, in ball radii. Closes during Arrival Seconds. */
	UPROPERTY(EditAnywhere, Category="Arrival|Appearance", meta=(ClampMin="0", ClampMax="1")) float ArrivalJawOpening = .60f;
	/** Painted ignition, smoke and ground pressure front. Empty uses the ball's common cue material. */
	UPROPERTY(EditAnywhere, Category="Detonation|Appearance") TObjectPtr<UMaterialInterface> ClaspBurstMaterial;
	/** Pieces of the 50 cm reference cage, pivoted on their own metal. They share the intact cage's finish. */
	UPROPERTY(EditAnywhere, Category="Detonation|Appearance") TObjectPtr<UStaticMesh> RibFragmentMesh;
	UPROPERTY(EditAnywhere, Category="Detonation|Appearance") TObjectPtr<UStaticMesh> LockFragmentMesh;
	UPROPERTY(EditAnywhere, Category="Detonation|Appearance") TObjectPtr<UStaticMesh> CollarFragmentMesh;
	/** Reference outward reach of the debris, as a fraction of the cue radius. */
	UPROPERTY(EditAnywhere, Category="Detonation|Appearance", meta=(ClampMin=".3", ClampMax=".78")) float ClaspBurstReach = .62f;
	/** Peak half-width of the central smoke as a fraction of the cue radius. */
	UPROPERTY(EditAnywhere, Category="Detonation|Appearance", meta=(ClampMin=".25", ClampMax=".75")) float ClaspSmokeSize = .54f;
	virtual bool Validate(FString& Error) const override;
	UMaterialInstanceDynamic* MakeCageMaterial(UObject* Outer) const;
};

USTRUCT()
struct FHALClaspTransfer
{
	GENERATED_BODY()
	// Absolute time and position also work if the old car disappears before the packet arrives.
	UPROPERTY() double Started = -1.;
	UPROPERTY() FVector_NetQuantize10 From = FVector::ZeroVector;
	UPROPERTY() TObjectPtr<AActor> FromCar;
	UPROPERTY() TObjectPtr<AActor> To;
};

UCLASS()
class HAL_FUTURE_CREATION_API AHALTimedBombBall : public AHALSpecialBall
{
	GENERATED_BODY()
public:
	AHALTimedBombBall();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	virtual bool AllowsAutomaticPickup() const override { return false; }
	virtual bool AllowsNormalLaunch() const override { return false; }
	UFUNCTION(BlueprintPure, Category="Timed Ball") double GetFuseDeadline() const { return FuseEnd; }
	virtual bool ReleaseFromControl(AActor* ExpectedHolder, float Lock) override;
	virtual bool HandleVehicleExit(const AActor* LeavingVehicle) override;
protected:
	virtual bool UsesFreeLife() const override { return FuseEnd < 0.; }
	virtual UHALSpecialBallDefinition* DefaultType() const override;
	virtual void OnAuthorityBallStateChanged(EBasicBallState Previous) override;
private:
	friend class FHALTimedBombRulesTest;
	bool TryMount(ATestVehiclePawn& Target, AActor* Passer);
	void OnVehicleCollisions(const Chaos::FCollisionEventData& Events);
	bool OnRam(ATestVehiclePawn& Driver, ATestVehiclePawn& Target, const FVector& DriverBefore, const FVector& TargetBefore);
	void Patrol();
	void Detonate();
	bool ResolveDueFuse();
	void Paint();
	const UHALTimedBombBallDefinition* Type() const { return Cast<UHALTimedBombBallDefinition>(Definition); }
	UFUNCTION() void OnContact(UPrimitiveComponent* Own, AActor* Other, UPrimitiveComponent* OtherBody, FVector Impulse, const FHitResult& Hit);
	UPROPERTY(Replicated) double FuseEnd = -1.;
	UPROPERTY(Replicated) FHALClaspTransfer Transfer;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Assembly;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Shell;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> LeftJaw;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> RightJaw;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Dial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> JawMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> DialMID;
	FHALAttackSourceSnapshot Credit;
	TMap<TWeakObjectPtr<AActor>, double> ContactLocks;
	FVector Heading = FVector::ForwardVector;
	FCollisionResponseContainer FreeResponses;
	bool bFreeGravity = true;
	FTimerHandle PatrolTimer, FuseTimer;
	double LastPatrolTime = 0.;
	double LastCollisionBatch = -1.;
	bool bWatchingCollisions = false;
};
