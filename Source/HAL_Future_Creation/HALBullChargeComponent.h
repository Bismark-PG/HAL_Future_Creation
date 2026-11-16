#pragma once

#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "GameplayEffectTypes.h"
#include "HALCombatTypes.h"
#include "HALBullChargeFXTypes.h"
#include "HALBullChargeComponent.generated.h"

class ABasicBallActor;
class UHALAbilitySystemComponent;
class UHALBullChargeConfig;
class UPrimitiveComponent;
class UNiagaraComponent;
class UMaterialInstanceDynamic;
struct FOnAttributeChangeData;
namespace Chaos { struct FCollisionEventData; }

/** One Avatar's charge lifetime and authority-only collision samples/target cooldowns. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALBullChargeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALBullChargeComponent();
	bool CanStart() const;
	bool Start(UHALAbilitySystemComponent& ASC, UHALBullChargeConfig& Config, FActiveGameplayEffectHandle Effect);
	void Stop();
	bool IsActive() const { return bActive; }
	static bool IsActiveOn(const AActor* Actor);
	static bool HandleVehiclePair(UPrimitiveComponent& Body, UPrimitiveComponent& Other,
		const FVector& NormalImpulse, const FHitResult& Hit);
	bool HandleFreeBall(ABasicBallActor& Ball, const FHitResult& Hit);
	/** Reserve charge contacts until all Chaos event handlers have delivered this frame's samples. */
	bool DeferContact(UPrimitiveComponent* Body, UPrimitiveComponent* OtherBody, const FVector& Impulse, const FHitResult& Hit);
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALBullChargeTestAccess;
	friend struct FHALBullChargeFXTestAccess;
	friend struct FHALBullChargeFXNetworkAccess;
	friend struct FHALBullChargeFXRenderAccess;
#endif
	void PublishDrivingVisual();
	UFUNCTION() void OnRep_VisualState();
	void UpdateParticles(float DeltaTime);
	void ClearParticles();
	void ConsumeImpactFX(const FHALBullChargeImpactFXEvent& Event);
	void SpawnImpactFX(const FHALBullChargeImpactFXEvent& Event);
	FHALBullChargeImpactFXEvent MakeImpactFX(const FHALPreparedCombatHit& Hit);
	UFUNCTION(NetMulticast, Unreliable) void MulticastImpactFX(FHALBullChargeImpactFXEvent Event);
	UPROPERTY(ReplicatedUsing=OnRep_VisualState) FHALBullChargeVisualState VisualState;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> FrontWindComponent;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FrontMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PressureMaterial;
	float SmoothedWind = 0;
	UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraComponent>> RearSparkComponents;
	struct FImpactParticle { TWeakObjectPtr<UNiagaraComponent> Component; double Expires = 0; };
	TArray<FImpactParticle> ImpactParticles;
	TArray<FHALBullChargeImpactFXEvent> PendingImpactFX;
	FGuid DrawActivation;
	uint32 NextImpactFXSequence = 0, LastImpactFXSequence = 0, ReceivedImpactFXCount = 0;
	TSet<FName> WarnedMounts;
	struct FContactSample
	{
		FVector BeforeVelocity = FVector::ZeroVector;
		FVector OtherBeforeVelocity = FVector::ZeroVector;
		double SolverTime = 0;
		uint64 GameFrame = 0;
	};
	void CollisionEvents(const Chaos::FCollisionEventData& Event);
	void ResolveDeferredContacts(UWorld* World, ELevelTick TickType, float DeltaSeconds);
	bool CanResolveContacts() const;
	FHALPreparedCombatHit PrepareVehicleHit(UPrimitiveComponent& Source, UPrimitiveComponent& Target,
		const FVector& NormalImpulse, const FHitResult& Hit);
	void ActiveTagChanged(FGameplayTag Tag, int32 Count);
	void HealthChanged(const FOnAttributeChangeData& Change);
	UPROPERTY(Replicated) bool bActive = false;
	UPROPERTY(Transient) TObjectPtr<UHALBullChargeConfig> Config;
	TWeakObjectPtr<UHALAbilitySystemComponent> BoundASC;
	FActiveGameplayEffectHandle ActiveEffect;
	FDelegateHandle TagChangedHandle, HealthChangedHandle;
	FHALAttackSourceSnapshot AttackSnapshot;
	TMap<TWeakObjectPtr<UPrimitiveComponent>, FContactSample> ContactSamples;
	struct FDeferredContact
	{
		TWeakObjectPtr<UPrimitiveComponent> Body, OtherBody;
		FVector Impulse;
		FHitResult Hit;
		uint64 Frame = 0;
	};
	TArray<FDeferredContact> DeferredContacts;
	FDelegateHandle PostActorTickHandle;
	uint64 SampleGameFrame = MAX_uint64;
	bool bResolvingContacts = false;
	TMap<TWeakObjectPtr<AActor>, double> NextTargetHit;
	double LastCollisionBatch = -1;
	bool bCollisionRegistered = false;
	FString LastBallDiagnostic;
	double LastBallDiagnosticTime = -1;
};
