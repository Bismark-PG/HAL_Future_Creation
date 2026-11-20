#pragma once

#include "GameFramework/Actor.h"
#include "HALCombatTypes.h"
#include "HALWaterCannon.generated.h"

class ABasicBallActor;
class ATestVehiclePawn;
class UAudioComponent;
class UDynamicMeshComponent;
class UMaterialInstanceDynamic;
class UHALWaterCannonConfig;

/** One actual surface sample for each contiguous group of blocked lanes, in nozzle space. */
USTRUCT()
struct FHALWaterJetContact
{
	GENERATED_BODY()
	UPROPERTY() FVector_NetQuantize10 Point=FVector::ZeroVector;
	UPROPERTY() FVector_NetQuantizeNormal Normal=FVector::BackwardVector;
	UPROPERTY() uint8 FirstLane=0;
	UPROPERTY() uint8 LastLane=0;
	bool operator==(const FHALWaterJetContact& Other) const = default;
};

/** Quantized reach for each slice of the fan. Distances and splash bits arrive together. */
USTRUCT()
struct FHALWaterJetView
{
	GENERATED_BODY()
	UPROPERTY() TArray<uint8> Reach;
	UPROPERTY() uint32 ContactMask = 0;
	UPROPERTY() TArray<FHALWaterJetContact> Contacts;
	UPROPERTY() bool bFlowing = false;
	bool operator==(const FHALWaterJetView& Other) const = default;
};

UCLASS(NotBlueprintable)
class HAL_FUTURE_CREATION_API AHALWaterCannon : public AActor
{
	GENERATED_BODY()
public:
	AHALWaterCannon();
	bool Start(UHALWaterCannonConfig& Config, const FVector& LocalNozzle);
	bool AuthorizesBallContact(const ABasicBallActor& Ball) const;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	static constexpr int32 LaneCount = 17;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALWaterCannonTestAccess;
	friend struct FHALWaterCannonNetworkTestAccess;
	friend class FHALWaterRenderTest;
#endif
	bool OwnerReady() const;
	double ServerTime() const;
	FTransform Frame(bool bVisual) const;
	FVector LanePoint(float Side, float Fraction) const;
	void SampleJet(double Now);
	void SampleCrossingBalls(const FTransform& Aim, const FHALWaterJetView& Sample, TSet<TWeakObjectPtr<ABasicBallActor>>& Contacts);
	void HitVehicle(ATestVehiclePawn& Vehicle, const FVector& Point, const FVector& Direction, double Now);
	void Present();
	void BuildWaterMesh(float Age);
	void ReleaseVisual();

	UPROPERTY(Replicated) TObjectPtr<UHALWaterCannonConfig> Config;
	UPROPERTY(Replicated) FVector LocalNozzle = FVector::ZeroVector;
	UPROPERTY(Replicated) double StartTime = 0;
	UPROPERTY(Replicated) FHALWaterJetView View;
	UPROPERTY() TObjectPtr<UDynamicMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UAudioComponent> Alarm;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> WaterMID;
	FHALWaterJetView LastDrawn;
	float LastMeshAge = -1;
	float FlowStartAge = 0;
	bool bRestartFlow = false;
	bool bSirenStarted = false;
	FGuid Activation;
	FHALAttackSourceSnapshot Attack;
	TMap<TWeakObjectPtr<ATestVehiclePawn>, double> NextHit;
	TSet<TWeakObjectPtr<ABasicBallActor>> LaunchedContacts;
	TWeakObjectPtr<ABasicBallActor> PendingBall;
	struct FBallSample
	{
		FVector Center;
		uint32 Sequence;
	};
	TMap<TWeakObjectPtr<ABasicBallActor>, FBallSample> PreviousBalls;
	FTransform PreviousAim = FTransform::Identity;
};

/** Local, collisionless water already in flight after the authoritative jet closes. */
UCLASS(Transient, NotBlueprintable)
class AHALWaterCannonTail : public AActor
{
	GENERATED_BODY()
public:
	AHALWaterCannonTail();
	void Start(UHALWaterCannonConfig& Settings,const FHALWaterJetView& LastView,float Age,float Head,UMaterialInstanceDynamic& Material);
	virtual void Tick(float DeltaSeconds) override;
private:
	UPROPERTY() TObjectPtr<UDynamicMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UHALWaterCannonConfig> Config;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> WaterMID;
	FHALWaterJetView View;
	float InitialAge=0, InitialHead=1, Elapsed=0;
};
