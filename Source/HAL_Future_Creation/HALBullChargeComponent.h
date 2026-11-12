#pragma once

#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "HALCombatTypes.h"
#include "HALBullChargeComponent.generated.h"

class ABasicBallActor;
class UHALAbilitySystemComponent;
class UHALBullChargeConfig;
class UPrimitiveComponent;
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
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALBullChargeTestAccess;
#endif
	struct FContactSample
	{
		FVector BeforeVelocity = FVector::ZeroVector;
		FVector OtherBeforeVelocity = FVector::ZeroVector;
		double SolverTime = 0;
		uint64 GameFrame = 0;
	};
	void CollisionEvents(const Chaos::FCollisionEventData& Event);
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
	TMap<TWeakObjectPtr<AActor>, double> NextTargetHit;
	double LastCollisionBatch = -1;
	bool bCollisionRegistered = false;
};
