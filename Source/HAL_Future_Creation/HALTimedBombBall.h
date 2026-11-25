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
	UPROPERTY(EditAnywhere, Category="Patrol|Appearance") TObjectPtr<UMaterialInterface> PatrolWakeMaterial;
	virtual bool Validate(FString& Error) const override;
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
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Wake;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> DialMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> WakeMID;
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
