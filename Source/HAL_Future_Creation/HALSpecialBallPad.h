#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "HALSpecialBallPad.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EHALPadBall : uint8 { None UMETA(Hidden), Explosive, BlackHole, Meteor };

/** One supply clock and one shuffled pool for all pads in a match. */
UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALSpecialBallSupplyDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	/** Time from entering Playing to the first ball, including its warning. */
	UPROPERTY(EditAnywhere, Category="Supply", meta=(ClampMin=".1", ClampMax="600", Units="s")) float FirstDropSeconds = 15.f;
	/** Game seconds between deliveries across the whole arena. */
	UPROPERTY(EditAnywhere, Category="Supply", meta=(ClampMin=".1", ClampMax="600", Units="s")) float DropInterval = 25.f;
	UPROPERTY(EditAnywhere, Category="Supply", meta=(ClampMin=".1", ClampMax="20", Units="s")) float WarningSeconds = 1.5f;
	/** Wait this long for an occupied site before skipping the turn. */
	UPROPERTY(EditAnywhere, Category="Supply", meta=(ClampMin="0", ClampMax="10", Units="s")) float BlockedGrace = 2.f;
	/** Each distinct enabled ball appears once per bag of successful drops. */
	UPROPERTY(EditAnywhere, Category="Supply") TArray<EHALPadBall> BallPool = {EHALPadBall::Explosive, EHALPadBall::BlackHole, EHALPadBall::Meteor};
	/** Distance between the bottom of the spawned sphere and the traced drive surface. */
	UPROPERTY(EditAnywhere, Category="Placement", meta=(ClampMin="3", ClampMax="50", Units="cm")) float SpawnClearance = 8.f;
	UPROPERTY(EditAnywhere, Category="Appearance") TObjectPtr<UStaticMesh> PadMesh;
	/** Visible diameter of the fixture. Gameplay clearance uses the selected ball's radius. */
	UPROPERTY(EditAnywhere, Category="Appearance", meta=(ClampMin="50", ClampMax="800", Units="cm")) float PadDiameter = 350.f;
	/** Total relief above the local deck finish. */
	UPROPERTY(EditAnywhere, Category="Appearance", meta=(ClampMin="1", ClampMax="10", Units="cm")) float PadThickness = 3.f;
};

USTRUCT()
struct FHALSpecialBallPadWarning
{
	GENERATED_BODY()
	UPROPERTY() EHALPadBall Ball = EHALPadBall::None;
	UPROPERTY() double StartedAt = 0.;
	UPROPERTY() float Duration = 0.f;
};

/** Place the root on the driving floor. Only the spawned ball has collision. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALSpecialBallPad : public AActor
{
	GENERATED_BODY()
public:
	AHALSpecialBallPad();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	bool FindSpawnCenter(float BallRadius, float Clearance, FVector& OutCenter) const;
	const FHALSpecialBallPadWarning& GetWarning() const { return Warning; }
	/** Lower values get the first opportunity when a new match starts. */
	UPROPERTY(EditInstanceOnly, Category="Special Ball Pad", meta=(ClampMin="0", ClampMax="999")) int32 RotationOrder = 0;
	/** Local deck trim above the driving plane. Only lifts the artwork; leave the actor root on the collision floor. */
	UPROPERTY(EditInstanceOnly, Category="Special Ball Pad", meta=(ClampMin="0", ClampMax="20", Units="cm")) float DeckFinishOffset = 0.f;
private:
	friend class UHALSpecialBallSupply;
	void SetWarning(EHALPadBall Ball, float Duration = 0.f);
	UFUNCTION() void OnRep_Warning();
	void ApplyMesh();
	UPROPERTY(VisibleAnywhere, Category="Special Ball Pad") TObjectPtr<UStaticMeshComponent> Surface;
	UPROPERTY(ReplicatedUsing=OnRep_Warning) FHALSpecialBallPadWarning Warning;
};

/** Server-only timers on the match GameState. Pads replicate just their current warning. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALSpecialBallSupply : public UActorComponent
{
	GENERATED_BODY()
public:
	void SetPlaying(bool bPlaying);
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
	friend class FHALSpecialBallSupplyTest;
#endif
	void ScheduleCycle(float Delay);
	void BeginCycle();
	void Announce();
	void Deliver();
	void FinishCycle();
	bool CanSupply() const;
	float WarningDuration = 0.f;
	float SpawnRadius = 0.f;
	double RetryUntil = 0.;
	int32 NextPad = 0;
	EHALPadBall LastDelivered = EHALPadBall::None;
	TArray<EHALPadBall> Bag;
	TArray<TWeakObjectPtr<AHALSpecialBallPad>> Pads;
	TWeakObjectPtr<AHALSpecialBallPad> AnnouncedPad;
	UPROPERTY(Transient) TObjectPtr<UHALSpecialBallSupplyDefinition> Settings;
	FTimerHandle Timer;
};
