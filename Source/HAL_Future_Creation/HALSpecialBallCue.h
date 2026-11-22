#pragma once

#include "HALSpecialBall.h"
#include "HALSpecialBallCue.generated.h"

class UDynamicMeshComponent;

USTRUCT()
struct FHALSpecialBallCueState
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UHALSpecialBallDefinition> Type;
	UPROPERTY() EHALBallCue Kind = EHALBallCue::Arrival;
	UPROPERTY() float Radius = 0;
	UPROPERTY() FVector_NetQuantizeNormal SurfaceNormal = FVector::ZeroVector;
	UPROPERTY() double Started = 0;
};

/** A short-lived world-space event; it survives consumption of the physical ball. */
UCLASS(NotPlaceable)
class HAL_FUTURE_CREATION_API AHALSpecialBallCue : public AActor
{
	GENERATED_BODY()
public:
	AHALSpecialBallCue();
	static void Spawn(UWorld& World, EHALBallCue Kind, UHALSpecialBallDefinition* Type, const FVector& Origin, float Radius, const FVector& Normal);
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	const FHALSpecialBallCueState& GetCueState() const { return State; }
private:
	friend class UHALMatchBallLifecycle;
	static void CreateCue(UWorld& World, EHALBallCue Kind, UHALSpecialBallDefinition* Type, const FVector& Origin,
		float Radius, const FVector& Normal, double Started, bool bReplicate);
	UFUNCTION() void OnRep_State();
	void PrepareWave();
	void PrepareEruption(const UHALExplosiveBallDefinition& Type);
	void Draw(float Progress);
	UPROPERTY(ReplicatedUsing=OnRep_State) FHALSpecialBallCueState State;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UDynamicMeshComponent> Ribbons;
	UPROPERTY(Transient) TObjectPtr<UDynamicMeshComponent> Eruption;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> EruptionMID;
	struct FWaveRing
	{
		TArray<FVector> Points;
		TArray<float> Open;
		bool bGround = false;
	};
	TArray<FWaveRing> Rings;
	bool bPrepared = false;
};
