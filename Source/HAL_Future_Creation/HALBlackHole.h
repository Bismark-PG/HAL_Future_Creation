#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HALBlackHole.generated.h"

class USplineComponent;
class ATestVehiclePawn;

UENUM(BlueprintType)
enum class EHALBlackHoleStage : uint8 { Expanding, PreparingPulse, Ended };

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALBlackHoleConfig
{
	GENERATED_BODY()
	/** Mass-independent inward acceleration, sampled every 0.02 game seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attraction", meta=(ClampMin="0", ClampMax="10000", Units="cm/s^2")) float ContinuousAcceleration = 120.f;
	/** Weak pull stops adding inward speed above this value; never clamps driving or yellow pulse velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attraction|Continuous Pull", meta=(ClampMin="1", ClampMax="2000", Units="cm/s")) float ContinuousSpeedLimit = 200.f;
	/** Within this distance, weak acceleration and target inward speed taper linearly to zero. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attraction|Continuous Pull", meta=(ClampMin="1", ClampMax="1000", Units="cm")) float CenterSofteningRadius = 100.f;
	/** Outward speed where escape assistance smoothly ends. This never imposes a minimum velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attraction|Escape Assistance", meta=(ClampMin="1", ClampMax="2000", Units="cm/s")) float EscapeRecoverySpeed = 150.f;
	/** Weak attraction retained at zero outward speed with full outward throttle; 1 disables assistance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attraction|Escape Assistance", meta=(ClampMin="0", ClampMax="1")) float LowSpeedAttractionScale = .3f;
	/** Planar speed retained once on entry. 0.4 retains 40%; 1 disables entry slowdown. Z and rotation are unchanged. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attraction|Entry", meta=(ClampMin="0", ClampMax="1")) float EntrySpeedMultiplier = .4f;
	/** Minimum time between entry slowdowns from this field; prevents edge jitter from repeatedly braking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attraction|Entry", meta=(ClampMin="0", ClampMax="10", Units="s")) float EntryRetriggerDelay = 1.f;
	/** Instantaneous inward speed increment; not a force sustained over time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attraction", meta=(ClampMin="0", ClampMax="10000", Units="cm/s")) float PulseDeltaSpeed = 700.f;
	/** First pulse occurs this long after expansion; further pulses use the same interval. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0.1", ClampMax="120", Units="s")) float PulseInterval = 2.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0", ClampMax="60", Units="s")) float ExpansionSeconds = 2.f;
	/** Hold time AFTER expansion. Total gameplay lifetime is expansion + duration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0.01", ClampMax="120", Units="s")) float DurationSeconds = 8.f;
	/** World XY radius, independent of Actor rotation/scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shape", meta=(ClampMin="1", ClampMax="10000", Units="cm")) float MaximumRadius = 700.f;
	/** Vertical tolerance about the Actor origin; not an infinite-height field. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category="Shape", meta=(ClampMin="1", ClampMax="1000", Units="cm")) float HalfHeight = 200.f;
	bool IsValid() const;
	float RadiusAt(double Age) const;
	EHALBlackHoleStage StageAt(double Age) const;
	int32 PulseIndexAt(double Age) const;
	FVector InwardDirection(const FVector& Center, const FVector& Position, double Age) const;
	FVector ContinuousDeltaVelocity(const FVector& Center, const FVector& Position, const FVector& Velocity, double Age, float StepSeconds,
		const FVector& Forward = FVector::ZeroVector, float Throttle = 0.f) const;
};

/** Spawnable server-owned planar attraction field. Create a Blueprint child for tuning and presentation. */
UCLASS(Blueprintable)
class HAL_FUTURE_CREATION_API AHALBlackHole : public AActor
{
	GENERATED_BODY()
public:
	AHALBlackHole();
	/** Authority-only deferred spawn configuration, before FinishSpawning. */
	bool ConfigureBlackHole(const FHALBlackHoleConfig& Config);
	UFUNCTION(BlueprintPure, Category="Black Hole") float GetCurrentRadius() const;
	UFUNCTION(BlueprintPure, Category="Black Hole") EHALBlackHoleStage GetStage() const;
	/** 0..1 from last pulse (or expansion end) to the next pulse. */
	UFUNCTION(BlueprintPure, Category="Black Hole") float GetPreparationProgress() const;
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_Lifecycle, Category="Black Hole") FHALBlackHoleConfig Settings;
	UPROPERTY(EditAnywhere, Replicated, Category="Black Hole|Debug", meta=(DisplayName="Debug Draw")) bool bDebugDraw = false;
	/** Cosmetic only; each peer evaluates synchronized age. Ended also fires on early removal. */
	UFUNCTION(BlueprintImplementableEvent, Category="Black Hole|Presentation") void OnStageChanged(EHALBlackHoleStage NewStage);
	/** Cosmetic update, at most once per rendered frame, about 50 Hz. Never apply gameplay here. */
	UFUNCTION(BlueprintImplementableEvent, Category="Black Hole|Presentation") void OnFieldUpdated(float RadiusCm, float PreparationProgress);
	/** One cosmetic notification for the newest observed pulse; late join does not replay old pulses. */
	UFUNCTION(BlueprintImplementableEvent, Category="Black Hole|Presentation") void OnAttractionPulse(int32 NewPulseIndex);
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FHALBlackHoleLifecycleTest;
	friend class FHALBlackHoleDebugTest;
#endif
	double Age() const;
	void UpdateField();
	void ApplyAttraction(double Elapsed, float StepSeconds, bool bPulse);
	void DrawFieldDebug(double Elapsed) const;
#if WITH_EDITORONLY_DATA
	/** Editor-only circular bounds; never participate in collision or gameplay. */
	UPROPERTY() TArray<TObjectPtr<USplineComponent>> DebugBounds;
#endif
	UFUNCTION() void OnRep_Lifecycle();
	UPROPERTY(ReplicatedUsing=OnRep_Lifecycle) double StartedAt = -1;
	UPROPERTY(ReplicatedUsing=OnRep_Pulse) int32 PulseIndex = 0;
	UFUNCTION() void OnRep_Pulse();
	FTimerHandle UpdateTimer;
	double LastUpdateAge = 0;
	EHALBlackHoleStage PresentedStage = EHALBlackHoleStage::Ended;
	bool bPresented = false;
	bool bPresentationReady = false;
	int32 PresentedPulse = 0;
	uint32 ActionSequence = 0;
	TSet<TWeakObjectPtr<ATestVehiclePawn>> Occupants;
	TMap<TWeakObjectPtr<ATestVehiclePawn>, double> LastEntryAt;
};
