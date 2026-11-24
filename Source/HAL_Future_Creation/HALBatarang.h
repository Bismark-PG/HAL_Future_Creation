#pragma once

#include "GameFramework/Actor.h"
#include "HALCombatTypes.h"
#include "HALBatarang.generated.h"

class ATestVehiclePawn;
class UHALBatarangConfig;
class UHALBatarangWidget;
class UDynamicMeshComponent;
class UMaterialInstanceDynamic;
class UMeshComponent;
class UStaticMeshComponent;

UENUM()
enum class EHALDartPhase : uint8 { Flight, Pinned, Burst };
UENUM()
enum class EHALDartSide : uint8 { Front, Rear, Left, Right };

USTRUCT()
struct FHALBatarangState
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UHALBatarangConfig> Config;
	UPROPERTY() EHALDartPhase Phase = EHALDartPhase::Flight;
	UPROPERTY() FVector_NetQuantize10 Origin = FVector::ZeroVector;
	UPROPERTY() float Yaw = 0;
	UPROPERTY() int8 Mirror = 1;
	UPROPERTY() double PhaseStart = 0;
	UPROPERTY() TObjectPtr<ATestVehiclePawn> Target;
	UPROPERTY() FVector_NetQuantize10 LocalAnchor = FVector::ZeroVector;
	UPROPERTY() FVector_NetQuantizeNormal LocalNormal = FVector::ForwardVector;
	UPROPERTY() EHALDartSide Side = EHALDartSide::Front;
	UPROPERTY() FVector_NetQuantize10 BurstPoint = FVector::ZeroVector;
	UPROPERTY() FVector_NetQuantizeNormal BurstDirection = FVector::ForwardVector;
};

namespace HALBatarang
{
	inline constexpr double BurstSeconds = .45;
	FVector Arc(float Fraction, float Reach, float Width, int8 Mirror);
	EHALDartSide HitSide(FVector RelativeToCenter, FVector Extent, FVector OutwardNormal);
	FVector PushDirection(EHALDartSide Side, double TargetYaw);
}

UCLASS(NotBlueprintable)
class HAL_FUTURE_CREATION_API AHALBatarang : public AActor
{
	GENERATED_BODY()
public:
	AHALBatarang();
	bool Start(UHALBatarangConfig& Config, int8 Mirror);
	bool IsManualInputOpen() const;
	bool RequestDetonation();
	const FHALBatarangState& GetState() const { return State; }
	double ServerTime() const;
	FVector FlightPoint(float Fraction) const;
	FVector PushDirection() const;
	virtual void Tick(float Delta) override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
	friend struct FHALBatarangTestAccess;
	UPROPERTY(Replicated) FHALBatarangState State;
	FHALCombatAction Attack;
	float LastFlightFraction = 0;
	bool ParticipantsReady() const;
	void Advance(double Now);
	bool Sweep(const FVector& From, const FVector& To, FCollisionQueryParams& Query, double Now);
	bool Detonate();
	void Present(double Now);
	void BuildGeometry(bool bBurst);
	void PrepareArt();
	void PrepareBurstClouds();
	void PresentArt(float Age, const FTransform& Pin);
	void UpdateWake(float Age);
	void PresentBurstClouds(const FVector& Origin, float Phase);
	void BindPinToBody(const FTransform& Vehicle);
	FTransform PinTransform();
	UPROPERTY() TObjectPtr<UDynamicMeshComponent> Shape;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Blade;
	UPROPERTY() TObjectPtr<UDynamicMeshComponent> Wake;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BladeInk;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> WakeInk;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CueInk;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> BurstClouds;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Accent;
	UPROPERTY(Transient) TObjectPtr<UHALBatarangWidget> OwnerView;
	bool bBuiltGeometry = false;
	bool bBuiltBurst = false;
	bool bPlayedPin = false;
	bool bArtReady = false;
	bool bPinWasConcealed = false;
	bool bBurstConcealed = false;
	bool bHasBurstContactOffset = false;
	TWeakObjectPtr<UMeshComponent> PinnedMesh;
	FName PinBone;
	FVector MeshAnchor = FVector::ZeroVector;
	FVector MeshNormal = FVector::ForwardVector;
	FVector MeshUp = FVector::UpVector;
	/** Cosmetic offset expressed in the horizontal push frame, before the victim is launched. */
	FVector BurstContactOffset = FVector::ZeroVector;
};
