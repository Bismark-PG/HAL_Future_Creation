#pragma once
#include "GameFramework/Actor.h"
#include "HALPolicePursuitTypes.h"
#include "HALPolicePursuit.generated.h"
class UHALPolicePursuitConfig; class UStaticMeshComponent; class UPointLightComponent; class UMaterialInstanceDynamic; class UAudioComponent;
UCLASS(NotBlueprintable)
class HAL_FUTURE_CREATION_API AHALPolicePursuit : public AActor
{
	GENERATED_BODY()
public:
	AHALPolicePursuit();
	bool Start(const UHALPolicePursuitConfig& Config);
	static bool VehicleReady(const ATestVehiclePawn* Vehicle);
	bool OwnerReady() const;
	bool AuthorizesArrest(const AActor* Target) const;
	const FHALPursuitState& GetState() const { return State; }
	virtual void Tick(float Dt) override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	double ServerTime() const;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALPursuitTestAccess;
	friend struct FHALSkillTestDummyTestAccess;
	friend class FHALPursuitRenderTest;
	friend struct FHALPursuitNetworkAccess;
#endif
	void Scan();
	bool InRange(ATestVehiclePawn& Target,bool WasInside) const;
	void UpdatePresentation();
	void StopPresentation();
	UFUNCTION() void OnRep_State();
	UPROPERTY(ReplicatedUsing=OnRep_State) FHALPursuitState State;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Range;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Beacons;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Lights;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Audio;
	FTimerHandle ScanTimer;
	double LastScan=-1;
	TWeakObjectPtr<ATestVehiclePawn> PendingTarget;
};
