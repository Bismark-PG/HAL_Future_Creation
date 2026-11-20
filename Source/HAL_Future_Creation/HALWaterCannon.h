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

/** Quantized reach for each slice of the fan. Distances and splash bits arrive together. */
USTRUCT()
struct FHALWaterJetView
{
	GENERATED_BODY()
	UPROPERTY() TArray<uint8> Reach;
	UPROPERTY() uint32 ContactMask = 0;
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

	UPROPERTY(Replicated) TObjectPtr<UHALWaterCannonConfig> Config;
	UPROPERTY(Replicated) FVector LocalNozzle = FVector::ZeroVector;
	UPROPERTY(Replicated) double StartTime = 0;
	UPROPERTY(Replicated) FHALWaterJetView View;
	UPROPERTY() TObjectPtr<UDynamicMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UAudioComponent> Alarm;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> WaterMID;
	FHALWaterJetView LastDrawn;
	double LastSplashTime = -1;
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
