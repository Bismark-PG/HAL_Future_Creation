#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/NetSerialization.h"
#include "HALMatchBallLifecycle.generated.h"

class ABasicBallActor;
class UDynamicMeshComponent;
class UMaterialInterface;
class UHALExplosiveBallDefinition;
class UHALBlackHoleBallDefinition;
class UHALMeteorBallDefinition;
class UHALSpecialBallSupplyDefinition;

/** World-space bounds of a circular playing surface. Heights are relative to FloorCenter.Z. */
USTRUCT()
struct HAL_FUTURE_CREATION_API FHALMatchBallArena
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Arena") TSoftObjectPtr<UWorld> MatchMap;
	/** Ordinary ball Blueprint and its DA, also used when a map has no placed primary. */
	UPROPERTY(EditAnywhere, Category="Arena") TSoftClassPtr<ABasicBallActor> PrimaryBallClass;
	UPROPERTY(EditAnywhere, Category="Arena", meta=(Units="cm")) FVector FloorCenter = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category="Arena", meta=(ClampMin="100", Units="cm")) float PlayableRadius = 8250.f;
	UPROPERTY(EditAnywhere, Category="Arena", meta=(ClampMin="10", Units="cm")) float FallDepth = 300.f;
	/** Brief excursions across the perimeter can return naturally. No ceiling limits legal flights. */
	UPROPERTY(EditAnywhere, Category="Recovery", meta=(ClampMin="0", ClampMax="5", Units="s")) float OutsideGraceSeconds = .35f;
	UPROPERTY(EditAnywhere, Category="Recovery", meta=(ClampMin="0.25", ClampMax="5", Units="s")) float ReturnWarningSeconds = 1.f;

	bool Contains(const FVector& Position) const;
	bool IsValid() const;
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="HAL Match Ball"))
class HAL_FUTURE_CREATION_API UHALMatchBallSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	/** Register competition maps here. Practice and test worlds have independent ball rules. */
	UPROPERTY(Config, EditAnywhere, Category="Match Ball") TArray<FHALMatchBallArena> Arenas;
	/** Ground cue material. A vector parameter named Color, when present, receives the cyan cue tint. */
	UPROPERTY(Config, EditAnywhere, Category="Presentation") TSoftObjectPtr<UMaterialInterface> ReturnRingMaterial;
	/** Default type for the placeable explosive ball and its development spawn command. */
	UPROPERTY(Config, EditAnywhere, Category="Special Balls") TSoftObjectPtr<UHALExplosiveBallDefinition> ExplosiveBallType;
	UPROPERTY(Config, EditAnywhere, Category="Special Balls") TSoftObjectPtr<UHALBlackHoleBallDefinition> BlackHoleBallType;
	UPROPERTY(Config, EditAnywhere, Category="Special Balls") TSoftObjectPtr<UHALMeteorBallDefinition> MeteorBallType;
	/** Shared by all special-ball pads placed in a match. */
	UPROPERTY(Config, EditAnywhere, Category="Special Balls") TSoftObjectPtr<UHALSpecialBallSupplyDefinition> SpecialBallSupply;
};

USTRUCT()
struct FHALMatchBallReturnCue
{
	GENERATED_BODY()
	UPROPERTY() bool bVisible = false;
	UPROPERTY() FVector_NetQuantize10 Ground = FVector::ZeroVector;
};

/** The GameState owns the permanent match ball and distributes one-shot ball impact notifications. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALMatchBallLifecycle : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALMatchBallLifecycle();
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void SetPlaying(bool bPlaying);
	bool IsReturnAnnounced() const { return ReturnCue.bVisible; }
	const FVector& GetReturnGround() const { return ReturnCue.Ground; }
	/** An infrequent confirmed hit, delivered over the already-open GameState channel. */
	UFUNCTION(NetMulticast, Reliable) void MulticastExplosiveImpact(UHALExplosiveBallDefinition* Type, FVector_NetQuantize10 Origin,
		float Radius, FVector_NetQuantizeNormal Normal, double ServerStarted);

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FHALMatchBallLifecycleTest;
	friend class FHALMatchBallCueRenderTest;
#endif
	void PrepareArena(const FHALMatchBallArena& Settings);
	void CheckPrimaryBall();
	bool FindReturnGround(const FVector& Around, FVector& OutGround) const;
	bool IsSafeGround(const FVector& XY, FVector& OutGround) const;
	void SetReturnCue(bool bVisible, const FVector& Ground = FVector::ZeroVector);
	UFUNCTION() void OnRep_ReturnCue();

	FHALMatchBallArena Arena;
	UPROPERTY(Transient) TSubclassOf<ABasicBallActor> BallClass;
	TWeakObjectPtr<ABasicBallActor> PrimaryBall;
	FVector Home = FVector::ZeroVector;
	float BallRadius = 0.f;
	double OutsideSince = -1.;
	double ReturnAt = 0.;
	double NextSearchAt = 0.;
	FTimerHandle CheckTimer;
	UPROPERTY(ReplicatedUsing=OnRep_ReturnCue) FHALMatchBallReturnCue ReturnCue;
	UPROPERTY(Transient) TObjectPtr<UDynamicMeshComponent> WarningRing;
};
