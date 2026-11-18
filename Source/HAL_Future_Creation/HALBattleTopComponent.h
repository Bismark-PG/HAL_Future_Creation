#pragma once

#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "HALCombatTypes.h"
#include "HALBattleTopFXTypes.h"
#include "HALBattleTopComponent.generated.h"

class ABasicBallActor;
class UHALAbilitySystemComponent;
class UHALBattleTopConfig;
class UMeshComponent;
class UPrimitiveComponent;
class UNiagaraComponent;
class UMaterialInstanceDynamic;
struct FOnAttributeChangeData;
struct FVehicleContactStabilityRules;

/** Replicate the activation time and speed together so observers use the same rotation. */
USTRUCT()
struct FHALBattleTopVisualState
{
	GENERATED_BODY()

	UPROPERTY() double StartServerTime = -1.0;
	UPROPERTY() float SpinRate = 0.f;
	UPROPERTY() float DurationSeconds = 0.f;
	UPROPERTY() float EnterSeconds = 0.f;
	UPROPERTY() float ExitSeconds = 0.f;
	UPROPERTY() FGuid Activation;
	float AngleAt(double Elapsed) const;
	float AngularSpeedAt(double Elapsed) const;
	float EnvelopeAt(double Elapsed) const;
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FHALBattleTopVisualState> : TStructOpsTypeTraitsBase2<FHALBattleTopVisualState>
{
	enum { WithNetSerializer = true };
};

/** Owns one Avatar's authoritative contact action and replicated cosmetic activation. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALBattleTopComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALBattleTopComponent();
	bool CanStart() const;
	bool Start(UHALAbilitySystemComponent& ASC, UHALBattleTopConfig& Config, FActiveGameplayEffectHandle Effect);
	void Stop();
	bool IsActive() const { return VisualState.StartServerTime >= 0.0; }
	float GetVisualSpinRate() const { return VisualState.SpinRate; }
	bool GetUnspunMeshTransform(const UMeshComponent& Mesh, FTransform& Out) const;
	static bool IsActiveOn(const AActor* Actor);
	static FHALPreparedCombatHit PrepareVehicleHit(UPrimitiveComponent& Source, UPrimitiveComponent& Target,
		const FVector& Impulse, const FHitResult& Hit);
	/** Evaluates both attack directions before either hit can eliminate an Avatar. */
	static bool HandleVehiclePair(UPrimitiveComponent& Body, UPrimitiveComponent& OtherBody,
		const FVector& NormalImpulse, const FHitResult& Hit);
	bool HandleFreeBall(ABasicBallActor& Ball);
	/** Cosmetic contact observation is independent from the target damage cooldown. */
	void ObserveContact(UPrimitiveComponent& Body, UPrimitiveComponent& Other, const FVector& NormalImpulse, const FHitResult& Hit);
	/** True for a qualifying ordinary wall even when this contact needs no supplemental rebound. */
	bool ResolveWallContact(UPrimitiveComponent& Body, UPrimitiveComponent& Wall,
		const FVector& NormalImpulse, const FHitResult& Hit, const FVehicleContactStabilityRules& Rules, FVector& OutDeltaVelocity);
	static FVector CalculateWallDelta(const UHALBattleTopConfig& Config, const FVector& Normal,
		const FVector& AfterVelocity, const FVector& NaturalDelta);
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALBattleTopTestAccess;
#endif
	bool CanResolveContacts() const;
	void ActiveTagChanged(FGameplayTag Tag, int32 Count);
	void HealthChanged(const FOnAttributeChangeData& Change);
	UFUNCTION() void OnRep_VisualState();
	void RestoreVisual();
	void RefreshContacts();
	void TrackContact(UPrimitiveComponent& Other, const FHitResult& Hit);
	void UpdateParticles(float DeltaTime, float Angle, float Envelope);
	void ClearParticles();
	void SpawnSparks(bool bGround, const FVector& Point, const FVector& Direction, const FVector& Normal = FVector::UpVector);
	UFUNCTION(NetMulticast, Unreliable) void MulticastContactSpark(FGuid Activation, FVector_NetQuantize Point,
		FVector_NetQuantizeNormal Direction, FVector_NetQuantizeNormal Normal);
	UPROPERTY(ReplicatedUsing=OnRep_VisualState) FHALBattleTopVisualState VisualState;
	UPROPERTY(ReplicatedUsing=OnRep_VisualState) FHALBattleTopFXConfig VisualFX;
	UPROPERTY(Transient) TObjectPtr<UHALBattleTopConfig> Config;
	TWeakObjectPtr<UHALAbilitySystemComponent> BoundASC;
	FActiveGameplayEffectHandle ActiveEffect;
	FDelegateHandle TagChangedHandle;
	FDelegateHandle HealthChangedHandle;
	TSharedPtr<FHALCombatAction> Action;
	struct FTrackedContact { FHitResult Hit; double NextSpark = -1; };
	TMap<TWeakObjectPtr<UPrimitiveComponent>, FTrackedContact> TrackedContacts;
	TMap<TWeakObjectPtr<AActor>, double> NextTargetHit;
	FTimerHandle ContactTimer;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> WindComponent;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> WindMaterial;
	struct FActiveSpark { TWeakObjectPtr<UNiagaraComponent> Component; double Expires = 0; };
	TArray<FActiveSpark> SparkComponents;
	double LastGroundSpark = -1;
	double LastWallContact = -1.0;
	TWeakObjectPtr<UMeshComponent> RotatingMesh;
	FTransform VisualBase = FTransform::Identity;
	FTransform LastVisualTransform = FTransform::Identity;
};
