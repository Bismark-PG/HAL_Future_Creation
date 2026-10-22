#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HALCombatTypes.h"
#include "HALBombActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class AHALBombExplosion;

/** Shared editable explosion payload. No ASC or live source attributes on a bomb. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALBombConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Bomb", meta=(ClampMin="0.1", ClampMax="60", Units="s"))
	float FuseSeconds = 3;
	UPROPERTY(EditAnywhere, Category="Bomb", meta=(ClampMin="1", ClampMax="5000", Units="cm"))
	float ExplosionRadius = 500;
	UPROPERTY(EditAnywhere, Category="Bomb", meta=(ClampMin="0", ClampMax="10000"))
	float Damage = 25;
	UPROPERTY(EditAnywhere, Category="Bomb")
	EHALImpactTier ImpactTier = EHALImpactTier::Heavy;
	UPROPERTY(EditAnywhere, Category="Bomb")
	bool bCanHitInstigator = true;
	/** One instantaneous spherical pulse; no falloff or wall occlusion in the first version. */
	bool IsValid() const;
};

/** Physical prefab base. First valid ground contact arms once; clients only display server time. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALBombActor : public AActor
{
	GENERATED_BODY()
public:
	AHALBombActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	bool TryArmFromGround(const FHitResult& Hit);
	/** Authority-only explicit trigger, shared by ground contact and the development spawn command. */
	bool StartCountdown();
	void Detonate();
	bool IsArmed() const { return bArmed; }
	float GetFuseEndServerTime() const { return FuseEndServerTime; }
	const FHALBombConfig& GetBombConfig() const { return Config; }
	float GetBodyRadius() const;
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bomb") TObjectPtr<USphereComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bomb") TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bomb") TObjectPtr<UTextRenderComponent> Countdown;
	UPROPERTY(EditDefaultsOnly, Category="Bomb") FHALBombConfig Config;
	UPROPERTY(EditDefaultsOnly, Category="Bomb") TSubclassOf<AHALBombExplosion> ExplosionClass;
	/** Ground must have this upward normal; vehicles/other bombs never count as ground. */
	UPROPERTY(EditDefaultsOnly, Category="Bomb", meta=(ClampMin="0.1", ClampMax="1")) float MinimumGroundNormalZ = 0.5f;
private:
	UFUNCTION() void OnBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, FVector Impulse, const FHitResult& Hit);
	UFUNCTION() void OnRep_Fuse();
	void ProbeInitialGround();
	void UpdateCountdown();
	UPROPERTY(ReplicatedUsing=OnRep_Fuse) bool bArmed = false;
	UPROPERTY(ReplicatedUsing=OnRep_Fuse) float FuseEndServerTime = 0;
	bool bDetonated = false;
	FHALAttackSourceSnapshot Attribution;
	FTimerHandle FuseTimer;
	FTimerHandle DisplayTimer;
	FTimerHandle ProbeTimer;
};

/** Separate replicated explosion range survives destruction of the physical bomb. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALBombExplosion : public AActor
{
	GENERATED_BODY()
public:
	AHALBombExplosion();
	void Initialize(const FHALBombConfig& InConfig, const FHALAttackSourceSnapshot& Attribution);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	FHALCombatHitResult ResolveTarget(AActor* Target);
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bomb") TObjectPtr<USphereComponent> Range;
	/** Optional content mesh/effect may be attached to Range by the team. */
	UPROPERTY(EditDefaultsOnly, Category="Bomb", meta=(ClampMin="0.2", ClampMax="5", Units="s")) float DisplaySeconds = 0.5f;
	UPROPERTY(EditDefaultsOnly, Category="Bomb") bool bDebugRange = true;
private:
	UPROPERTY(Replicated) FHALBombConfig Config;
	FHALCombatAction Action;
	bool bInitialized = false;
};
