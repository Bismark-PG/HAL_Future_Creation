#pragma once

#include "HALSpecialBall.h"
#include "HALHomingBall.generated.h"

/** A powered arcade ball. All distances are in world centimetres. */
UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALHomingBallDefinition : public UHALSpecialBallDefinition
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Flight", meta=(ClampMin="100", ClampMax="10000", Units="cm/s")) float InitialSpeed = 1600.f;
	UPROPERTY(EditAnywhere, Category="Flight", meta=(ClampMin="100", ClampMax="12000", Units="cm/s")) float CruiseSpeed = 4000.f;
	UPROPERTY(EditAnywhere, Category="Flight", meta=(ClampMin=".1", ClampMax="15", Units="s")) float AccelerationSeconds = 1.8f;
	/** Independent of the cumulative Free lifetime; a completed shot expires harmlessly. */
	UPROPERTY(EditAnywhere, Category="Flight", meta=(ClampMin=".1", ClampMax="30", Units="s")) float FlightSeconds = 4.f;
	UPROPERTY(EditAnywhere, Category="Steering", meta=(ClampMin="0", ClampMax="720", Units="deg/s")) float InitialTurnRate = 180.f;
	UPROPERTY(EditAnywhere, Category="Steering", meta=(ClampMin="0", ClampMax="720", Units="deg/s")) float CruiseTurnRate = 35.f;
	/** Minimum fraction of the speed gain at which a close pass can break the lock. */
	UPROPERTY(EditAnywhere, Category="Steering", meta=(ClampMin="0", ClampMax="1")) float BreakLockSpeedFraction = .72f;
	/** Maximum distance between vehicle and ball centres at a high-speed pass. */
	UPROPERTY(EditAnywhere, Category="Steering", meta=(ClampMin="50", ClampMax="2500", Units="cm")) float PassDistance = 700.f;
	UPROPERTY(EditAnywhere, Category="Explosion", meta=(ClampMin="1", ClampMax="5000", Units="cm")) float ExplosionRadius = 250.f;
	/** Nozzle mesh pointing towards local -X, authored at one ball radius long. */
	UPROPERTY(EditAnywhere, Category="Effects|Cruise") TObjectPtr<UStaticMesh> ExhaustMesh;
	/** Exhaust length at maximum speed as a multiple of the ball radius. */
	UPROPERTY(EditAnywhere, Category="Effects|Cruise", meta=(ClampMin=".25", ClampMax="5")) float ExhaustLength = 2.25f;
	UPROPERTY(EditAnywhere, Category="Warning", meta=(ClampMin=".015", ClampMax=".12")) float WarningScreenFraction = .047f;
	/** Height reserved for the bottom-centre HUD as a fraction of viewport height. */
	UPROPERTY(EditAnywhere, Category="Warning", meta=(ClampMin="0", ClampMax=".30")) float WarningBottomInset = .10f;
	/** Seconds of travel at the current speed within which the offscreen flash quickens. */
	UPROPERTY(EditAnywhere, Category="Warning", meta=(ClampMin=".2", ClampMax="5", Units="s")) float WarningApproachSeconds = 1.6f;
	float SpeedFraction(double Age) const;
	virtual bool Validate(FString& Error) const override;
};

USTRUCT()
struct FHALHomingFlight
{
	GENERATED_BODY()
	UPROPERTY() double Started = -1.;
	UPROPERTY() TObjectPtr<ATestVehiclePawn> Target;
};

UCLASS()
class HAL_FUTURE_CREATION_API AHALHomingBall : public AHALContactSpecialBall
{
	GENERATED_BODY()
public:
	AHALHomingBall();
	virtual bool UsesStandardFlight() const override { return false; }
	virtual FVector GetVisualFacingDirection() const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	const ATestVehiclePawn* GetLockedVehicle() const;
	const UHALHomingBallDefinition* GetHomingType() const { return Cast<UHALHomingBallDefinition>(Definition); }
protected:
	virtual UHALSpecialBallDefinition* DefaultType() const override;
	virtual bool DeliverLaunch(const FHALAttackSourceSnapshot& Attack, AActor* Launcher, const FVector& Velocity, const AActor* FrameSource) override;
	virtual void OnAuthorityBallStateChanged(EBasicBallState Previous) override;
	virtual void UpdateFlightMovement() override;
	virtual bool ApplyContact(AActor* Other, const FVector& Origin, const FVector& Normal, const FVector& Travel) override;
private:
	friend class FHALHomingBallTest;
	UFUNCTION() void RefreshFlightVisual();
	UPROPERTY(VisibleAnywhere, Category="Presentation") TObjectPtr<UStaticMeshComponent> Exhaust;
	UPROPERTY(ReplicatedUsing=RefreshFlightVisual) FHALHomingFlight Flight;
	FVector Heading = FVector::ForwardVector;
	FVector PreviousOffset = FVector::ZeroVector;
	double LastUpdate = 0.;
	bool bCrossedTarget = false;
};
