#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "HALElectricWallGimmick.generated.h"

class UCapsuleComponent;
class UBoxComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;
class ATestVehiclePawn;
class UHALImpactSourceComponent;

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALElectricWallConfig
{
	GENERATED_BODY()
	/** Pillar center-to-center distance along local X. Actor origin is ground center. */
	UPROPERTY(EditAnywhere, Category="Geometry", meta=(ClampMin="1",Units="cm")) float PillarSpacing = 1000;
	UPROPERTY(EditAnywhere, Category="Geometry", meta=(ClampMin="1",Units="cm")) float PillarRadius = 60;
	UPROPERTY(EditAnywhere, Category="Geometry", meta=(ClampMin="2",Units="cm")) float Height = 300;
	UPROPERTY(EditAnywhere, Category="Geometry", meta=(ClampMin="1",Units="cm")) float WallHalfThickness = 25;
	/** Legacy serialized setting; pillars now use ordinary collision and no electric contact margin. */
	UPROPERTY(EditAnywhere, Category="Contact", meta=(ClampMin="0",Units="cm")) float ContactMargin = 10;
	UPROPERTY(EditAnywhere, Category="Contact", meta=(ClampMin="0.01",Units="s")) float ScanInterval = .05f;
	/** Unobstructed horizontal target distance. Contacts/drag can shorten actual travel. */
	UPROPERTY(EditAnywhere, Category="Bounce Back", meta=(ClampMin="1",ClampMax="1000",Units="cm")) float BounceBackDistance = 100;
	/** Linearly decelerating return, independent of incoming speed. */
	UPROPERTY(EditAnywhere, Category="Bounce Back", meta=(ClampMin="0.05",ClampMax="2",Units="s")) float BounceBackSeconds = .3f;
	UPROPERTY(EditAnywhere, Category="Contact", meta=(ClampMin="0.01",Units="s")) float StunSeconds = 1;
	bool Validate(FString& Error) const;
};

/** Only the middle electrical field owns the per-vehicle electric entry latch.
	* Pillars are solid; the electrical field is a query-only trigger. No ball effects, damage or ASC. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALElectricWallGimmick : public AActor
{
	GENERATED_BODY()
public:
	AHALElectricWallGimmick();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	/** Optional deferred-spawn configuration. Never changes geometry or rules mid-contact. */
	bool Configure(const FHALElectricWallConfig& InConfig);
	uint32 GetAcceptedContactCount() const { return AcceptedContactCount; }
	uint32 GetPhysicsSourceId() const { return PhysicsSourceId; }
protected:
	UPROPERTY(EditDefaultsOnly, ReplicatedUsing=OnRep_Config, Category="Electric Wall") FHALElectricWallConfig Config;
	UPROPERTY(EditDefaultsOnly, Category="Electric Wall|Debug") bool bDrawDebug = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Electric Wall|Components") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Electric Wall|Components") TObjectPtr<UCapsuleComponent> PillarA;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Electric Wall|Components") TObjectPtr<UCapsuleComponent> PillarB;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Electric Wall|Components") TObjectPtr<UBoxComponent> ElectricField;
	/** Meshes are presentation only. Team assigns assets and fits transforms in the Editor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Electric Wall|Components") TObjectPtr<UStaticMeshComponent> PillarVisualA;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Electric Wall|Components") TObjectPtr<UStaticMeshComponent> PillarVisualB;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Electric Wall|Components") TObjectPtr<UStaticMeshComponent> WallVisual;
	UPROPERTY(VisibleAnywhere, Category="Electric Wall|Components") TObjectPtr<UHALImpactSourceComponent> ContactImpactProfile;
private:
	friend struct FHALElectricWallTestAccess;
	UFUNCTION() void OnRep_Config();
	void UpdateGeometry();
	void ScanContacts();
	bool ApplyContact(ATestVehiclePawn* Target,UPrimitiveComponent* Surface,const FVector& PreviousLocation);
	FVector GetPushDirection(UPrimitiveComponent* Surface,const FVector& Current,const FVector& Previous,const FVector& Velocity) const;
	struct FContactRecord
	{
		FVector PreviousLocation = FVector::ZeroVector;
		FVector IncomingPushDirection = FVector::ZeroVector;
		double SampledAt = -1;
		bool bTriggered = false;
		double BounceEndsAt = 0;
		bool bInContact = false;
	};
	TMap<TWeakObjectPtr<ATestVehiclePawn>,FContactRecord> Contacts;
	UPROPERTY(Replicated) uint32 AcceptedContactCount = 0;
	UPROPERTY(Replicated) uint32 PhysicsSourceId = 0;
	FTimerHandle ScanTimer;
	bool bReady = false;
};
