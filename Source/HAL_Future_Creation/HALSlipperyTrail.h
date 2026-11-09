#pragma once

#include "Engine/NetSerialization.h"
#include "GameFramework/Actor.h"
#include "HALMovementAreaTypes.h"
#include "HALSlipperyTrail.generated.h"

class ATestVehiclePawn;
class UHALMovementAreaReceiver;

USTRUCT(BlueprintType)
struct FHALSlipperyTrailShape
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Timing", meta=(ClampMin="0.1", ClampMax="30", Units="s")) float LifetimeSeconds = 7.f;
	UPROPERTY(EditAnywhere, Category="Shape", meta=(ClampMin="20", ClampMax="1000", Units="cm")) float InitialHalfWidth = 70.f;
	UPROPERTY(EditAnywhere, Category="Shape", meta=(ClampMin="20", ClampMax="1200", Units="cm")) float MaximumHalfWidth = 280.f;
	UPROPERTY(EditAnywhere, Category="Shape", meta=(ClampMin="0.05", ClampMax="30", Units="s")) float ExpansionSeconds = 1.6f;
	UPROPERTY(EditAnywhere, Category="Shape", meta=(ClampMin="5", ClampMax="200", Units="cm")) float GroundHeightTolerance = 65.f;
	UPROPERTY(EditAnywhere, Category="Outline") bool bDrawOutline = true;
	UPROPERTY(EditAnywhere, Category="Outline") FColor OutlineColor = FColor::Cyan;
	UPROPERTY(EditAnywhere, Category="Outline", meta=(ClampMin="0", ClampMax="30", Units="cm")) float OutlineLift = 5.f;
	UPROPERTY(EditAnywhere, Category="Outline", meta=(ClampMin="0.5", ClampMax="10")) float OutlineThickness = 2.f;

	float HalfWidthAt(float PointAge) const;
};

USTRUCT(BlueprintType)
struct FHALSlipperyTrailEmission
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Timing", meta=(ClampMin="0.05", ClampMax="29", Units="s")) float EmissionSeconds = 3.f;
	UPROPERTY(EditAnywhere, Category="Sampling", meta=(ClampMin="0.02", ClampMax="0.2", Units="s")) float UpdateInterval = .04f;
	UPROPERTY(EditAnywhere, Category="Sampling", meta=(ClampMin="10", ClampMax="250", Units="cm")) float SampleSpacing = 65.f;
	UPROPERTY(EditAnywhere, Category="Sampling", meta=(ClampMin="20", ClampMax="1500", Units="cm")) float MaximumLinkDistance = 440.f;
	UPROPERTY(EditAnywhere, Category="Sampling", meta=(ClampMin="1", ClampMax="256")) int32 MaximumPoints = 128;
	UPROPERTY(EditAnywhere, Category="Sampling", meta=(ClampMin="0", ClampMax="300", Units="cm")) float TailOffset = 20.f;
	UPROPERTY(EditAnywhere, Category="Ground", meta=(ClampMin="20", ClampMax="350", Units="cm")) float GroundProbeDepth = 170.f;
	UPROPERTY(EditAnywhere, Category="Ground", meta=(ClampMin="0.1", ClampMax="1")) float MinimumGroundNormalZ = .55f;
};

USTRUCT(BlueprintType)
struct FHALSlipperyTrailHandling
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Grip", meta=(ClampMin="0", ClampMax="1")) float LateralGripMultiplier = .30f;
	UPROPERTY(EditAnywhere, Category="Grip", meta=(ClampMin="0", ClampMax="10", Units="s")) float RecoverySeconds = .35f;
	UPROPERTY(EditAnywhere, Category="Sway") FHALMovementAreaSway Sway;
};

USTRUCT()
struct FHALSlipperyTrailPoint
{
	GENERATED_BODY()
	UPROPERTY() FVector_NetQuantize10 Ground = FVector::ZeroVector;
	UPROPERTY() float EmissionAge = 0;
	UPROPERTY() bool bLinked = false;
};

/** One world-fixed source and one common expiry per cast. Only the server evaluates membership. */
UCLASS(NotBlueprintable)
class AHALSlipperyTrail : public AActor
{
	GENERATED_BODY()
public:
	AHALSlipperyTrail();
	static bool CanLay(const ATestVehiclePawn& Vehicle);
	static bool FindTailGround(const ATestVehiclePawn& Vehicle, const FHALSlipperyTrailEmission& Rules,
		const FHALSlipperyTrailShape& InShape, FVector& Out);
	bool Start(const FHALSlipperyTrailShape& InShape, const FHALSlipperyTrailEmission& InEmission,
		const FHALSlipperyTrailHandling& Handling, const FVector& FirstPoint);
	static bool Intersects(TConstArrayView<FHALSlipperyTrailPoint> Path, const FHALSlipperyTrailShape& InShape,
		float TrailAge, const FVector& From, const FVector& To);
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALSlipperyTrailNetworkAccess;
#endif
	double ServerTime() const;
	void UpdateTrail();
	void AddPoint(const FVector& Ground, float Age);
	void UpdateReceivers(float Age);

	UPROPERTY(Replicated) FHALSlipperyTrailShape Shape;
	UPROPERTY(Replicated) double StartTime = -1;
	UPROPERTY(Replicated) TArray<FHALSlipperyTrailPoint> Points;
	FHALSlipperyTrailEmission Emission;
	FHALMovementAreaConfig Area;
	FTimerHandle UpdateHandle;
	TMap<TWeakObjectPtr<ATestVehiclePawn>, FVector> LastGround;
	TSet<TWeakObjectPtr<UHALMovementAreaReceiver>> Occupants;
	bool bLinkNext = false;
};
