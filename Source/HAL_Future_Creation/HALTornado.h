#pragma once
#include "GameFramework/Actor.h"
#include "HALCombatTypes.h"
#include "HALTornadoTypes.h"
#include "HALTornado.generated.h"
class ATestVehiclePawn;
class ABasicBallActor;
class UHALTornadoConfig;
class UNiagaraComponent;
class UMaterialInstanceDynamic;
class AHALTornado;
DECLARE_MULTICAST_DELEGATE_OneParam(FHALTornadoGameplayEnded, AHALTornado*);

/** One authority-driven, non-rigid-body skill volume; cars and balls remain Chaos bodies. */
UCLASS(NotBlueprintable)
class HAL_FUTURE_CREATION_API AHALTornado : public AActor
{
	GENERATED_BODY()
public:
	AHALTornado();
	static bool FindStart(const ATestVehiclePawn& Vehicle, const FHALTornadoRules& Rules, FVector& Ground);
	bool Start(const UHALTornadoConfig& Config, const FVector& Ground);
	bool AuthorizesBallContact(const ABasicBallActor& Ball) const;
	const FHALTornadoState& GetState() const { return State; }
	float DisarmPickupLock() const { return State.Rules.DisarmPickupLockSeconds; }
	uint32 GetVehicleHitCount() const { return VehicleHitCount; }
	uint32 GetBallHitCount() const { return BallHitCount; }
	FHALTornadoGameplayEnded OnGameplayEnded;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	/** Relative swept cylinder, including initial overlap; returns earliest contact in [0,1]. */
	static bool SweptContact(const FVector& RelativeFrom, const FVector& RelativeTo, float Radius, float Height, float& Time);
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALTornadoTestAccess;
	friend struct FHALSkillTestDummyTestAccess;
	friend struct FHALTornadoNetworkAccess;
	friend class FHALTornadoRenderTest;
#endif
	bool OwnerReady() const;
	double ServerTime() const;
	void Simulate(float DeltaSeconds);
	void Contacts(const FVector& From, const FVector& To, float Horizon);
	void FinishGameplay();
	void UpdatePresentation(float DeltaSeconds);
	UFUNCTION() void OnRep_State();
	UPROPERTY(ReplicatedUsing=OnRep_State) FHALTornadoState State;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> Particles;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
	FHALAttackSourceSnapshot Attack;
	TMap<TWeakObjectPtr<AActor>, double> NextHit;
	TWeakObjectPtr<ABasicBallActor> PendingBall;
	TSet<TWeakObjectPtr<ABasicBallActor>> BallContacts;
	double LastAge = 0;
	double ClientTravelTime = -1;
	bool bGameplayEnded = false;
	uint32 VehicleHitCount = 0, BallHitCount = 0;
	float LastAcceptedUpwardSpeed = 0;
};
