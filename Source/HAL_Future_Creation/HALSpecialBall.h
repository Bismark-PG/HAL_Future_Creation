#pragma once

#include "CoreMinimal.h"
#include "BasicBallActor.h"
#include "VehicleDefinition.h"
#include "HALMatchBallLifecycle.h"
#include "HALMatchGameState.h"
#include "HALSpecialBall.generated.h"

class UMaterialInstanceDynamic;

/** Ordinary Chaos, control and presentation settings are inherited and live in this same asset. */
UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALSpecialBallDefinition : public UBallDefinition
{
	GENERATED_BODY()
public:
	/** Total game seconds spent Free across every holder and launch. */
	UPROPERTY(EditAnywhere, Category="Lifetime", meta=(ClampMin=".1", ClampMax="600", Units="s")) float FreeLifeSeconds = 20.f;
	UPROPERTY(EditAnywhere, Category="Lifetime", meta=(ClampMin="0", ClampMax="60", Units="s")) float WarningSeconds = 3.f;
	UPROPERTY(EditAnywhere, Category="Effects", meta=(ClampMin=".12", ClampMax="3", Units="s")) float ArrivalSeconds = .52f;
	UPROPERTY(EditAnywhere, Category="Effects", meta=(ClampMin=".12", ClampMax="3", Units="s")) float ExpirationSeconds = .48f;
	UPROPERTY(EditAnywhere, Category="Effects", meta=(ClampMin=".12", ClampMax="3", Units="s")) float ImpactSeconds = .54f;
	/** Reach of the gathering and fading ribbons in multiples of the ball radius. */
	UPROPERTY(EditAnywhere, Category="Effects", meta=(ClampMin="1", ClampMax="6")) float CoronaSize = 2.6f;
	UPROPERTY(EditAnywhere, Category="Effects", meta=(ClampMin="0", ClampMax="10")) float SurfaceFlowSpeed = .65f;
	UPROPERTY(EditAnywhere, Category="Effects", meta=(ClampMin=".1", ClampMax="8")) float Glow = 1.65f;
	UPROPERTY(EditAnywhere, Category="Effects") FLinearColor CoreColor = FLinearColor(.075f,.009f,.023f);
	UPROPERTY(EditAnywhere, Category="Effects") FLinearColor CurrentColor = FLinearColor(.54f,.052f,.012f);
	UPROPERTY(EditAnywhere, Category="Effects") FLinearColor CoronaColor = FLinearColor(1.f,.52f,.075f);
	/** Optional emissive shell on the displayed ball. Shares its colour palette and expiry pulse. */
	UPROPERTY(EditAnywhere, Category="Effects|Body Corona") TObjectPtr<UMaterialInterface> BodyCoronaMaterial;
	/** Visible atmosphere thickness as a fraction of the physical radius. Zero disables the shell. */
	UPROPERTY(EditAnywhere, Category="Effects|Body Corona", meta=(ClampMin="0", ClampMax="1")) float BodyCoronaWidth = .30f;
	/** Emissive intensity of the ball's outer rim; independently adjustable from short-lived cues. */
	UPROPERTY(EditAnywhere, Category="Effects|Body Corona", meta=(ClampMin="0", ClampMax="8")) float BodyCoronaGlow = 2.4f;
	/** Two-sided vertex-colour material for the corona ribbons and embers. */
	UPROPERTY(EditAnywhere, Category="Effects") TObjectPtr<UMaterialInterface> CueMaterial;
	virtual bool Validate(FString& Error) const override;
	virtual float CueSeconds(EHALBallCue Cue) const;
};

UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALExplosiveBallDefinition : public UHALSpecialBallDefinition
{
	GENERATED_BODY()
public:
	/** One world-space radius for the instantaneous hit and its visible wave. Damage and tier use Gameplay. */
	UPROPERTY(EditAnywhere, Category="Explosion", meta=(ClampMin="1", ClampMax="5000", Units="cm")) float ExplosionRadius = 500.f;
	/** Small detached solar flares at the foot of the three main prominences. */
	UPROPERTY(EditAnywhere, Category="Explosion|Effects", meta=(ClampMin="4", ClampMax="20")) int32 FlareCount = 12;
	/** Masked three-tone material for the arched ribbons and compact central smoke. Optional on older ball types. */
	UPROPERTY(EditAnywhere, Category="Explosion|Effects") TObjectPtr<UMaterialInterface> EruptionMaterial;
	/** Peak height of the largest arch, as a fraction of the blast radius. */
	UPROPERTY(EditAnywhere, Category="Explosion|Effects", meta=(ClampMin=".15", ClampMax=".9")) float ProminenceHeight = .67f;
	/** Full width of the largest ribbon, as a fraction of the blast radius. */
	UPROPERTY(EditAnywhere, Category="Explosion|Effects", meta=(ClampMin=".04", ClampMax=".35")) float ProminenceWidth = .29f;
	/** Travel of the painted currents from root to tip, in ribbon lengths per second. Zero holds them still. */
	UPROPERTY(EditAnywhere, Category="Explosion|Effects", meta=(ClampMin="0", ClampMax="5")) float ProminenceFlowSpeed = 1.35f;
	/** Contrast of the moving currents inside the ribbons. Zero restores the underlying three-tone paint. */
	UPROPERTY(EditAnywhere, Category="Explosion|Effects", meta=(ClampMin="0", ClampMax="1")) float ProminenceFlowStrength = .70f;
	/** Extra seconds for the arch afterglow after the impact flash and smoke. Zero uses the base impact duration. */
	UPROPERTY(EditAnywhere, Category="Explosion|Effects", meta=(ClampMin="0", ClampMax="1.5", Units="s")) float ProminenceTailSeconds = 0.f;
	/** Radius of the central cluster relative to the blast radius. Zero omits the smoke. */
	UPROPERTY(EditAnywhere, Category="Explosion|Effects", meta=(ClampMin="0", ClampMax=".45")) float CoreSmokeSize = .27f;
	virtual float CueSeconds(EHALBallCue Cue) const override;
	virtual bool Validate(FString& Error) const override;
};

USTRUCT()
struct FHALSpecialBallClock
{
	GENERATED_BODY()
	UPROPERTY() bool bStarted = false;
	UPROPERTY() double FreeUntil = -1.;
	UPROPERTY() float Remaining = 0.f;
};

UCLASS(Abstract)
class HAL_FUTURE_CREATION_API AHALSpecialBall : public ABasicBallActor
{
	GENERATED_BODY()
public:
	AHALSpecialBall();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PreInitializeComponents() override;
	virtual void PostNetInit() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual float GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, AActor* Viewer,
		AActor* ViewTarget, UActorChannel* Channel, float Time, bool bLowBandwidth) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	float RemainingFreeSeconds() const;
	const UHALSpecialBallDefinition* GetTypeDefinition() const { return Cast<UHALSpecialBallDefinition>(Definition); }
	/** Also resolves the configured type on a class default object before its first spawn. */
	const UHALSpecialBallDefinition* GetSpawnDefinition() const { return Definition ? GetTypeDefinition() : DefaultType(); }
protected:
	virtual UHALSpecialBallDefinition* DefaultType() const { return nullptr; }
	virtual bool CanUseBall() const override;
	virtual bool UsesFreeLife() const { return true; }
	bool HasStarted() const { return Clock.bStarted; }
	double ServerNow() const;
	virtual void OnAuthorityBallStateChanged(EBasicBallState Previous) override;
	virtual void OnAcceptedVehicleHit() override;
	void Expire();
	virtual void PlayCue(EHALBallCue Cue, const FVector& Location, float Radius, const FVector& ContactNormal = FVector::ZeroVector);
	bool bConsumed = false;
private:
	friend class FHALSpecialBallArenaTest;
	void SelectDefinition();
	void CheckArena();
	UFUNCTION() void OnMatchPhaseChanged(EHALMatchPhase Phase);
	UFUNCTION() void OnRep_Clock();
	void UpdateStaging();
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Surface;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BodyCorona;
	/** A deferred server spawn may select a saved DA different from the native class default. */
	UPROPERTY(Replicated) TObjectPtr<UHALSpecialBallDefinition> SpawnType;
	UPROPERTY(ReplicatedUsing=OnRep_Clock) FHALSpecialBallClock Clock;
	TOptional<FHALMatchBallArena> Arena;
	FCollisionResponseContainer SpawnResponses;
	bool bPresentationReady = false;
	bool bStaged = false;
	bool bSpawnGravity = false;
	double OutsideSince = -1.;
	double ArrivalPriorityUntil = -1.;
	FTimerHandle ExpiryTimer, ArenaTimer;
};

/** One shared flight sweep and one authoritative contact for consumable impact balls. */
UCLASS(Abstract)
class HAL_FUTURE_CREATION_API AHALContactSpecialBall : public AHALSpecialBall
{
	GENERATED_BODY()
protected:
	virtual void OnAuthorityBallStateChanged(EBasicBallState Previous) override;
	virtual void ResolveLaunchedContact(AActor* Other, UPrimitiveComponent* Body, FVector Impulse, const FHitResult& Hit) override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	/** Return false if this is not an eligible hit and the shot should keep flying. */
	virtual bool ApplyContact(AActor* Other, const FVector& Origin, const FVector& Normal, const FVector& Travel) PURE_VIRTUAL(AHALContactSpecialBall::ApplyContact, return false;);
private:
	friend class FHALExplosiveBallTest;
	friend class FHALBlackHoleBallTest;
	friend struct FHALSpecialBallNetworkCheck;
	void CheckFlight();
	bool FindFieldHit(const FVector& Start, const FVector& End, FHitResult& Out) const;
	void ConsumeContact(AActor* Other, const FVector& Origin, const FVector& Normal);
	FVector LastFlightCenter = FVector::ZeroVector;
	FTimerHandle FlightTimer;
};

/** Place directly or spawn on the authority using the project's explosive ball type. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALExplosiveBall : public AHALContactSpecialBall
{
	GENERATED_BODY()
protected:
	virtual UHALSpecialBallDefinition* DefaultType() const override;
	virtual bool ApplyContact(AActor* Other, const FVector& Origin, const FVector& Normal, const FVector& Travel) override;
};
