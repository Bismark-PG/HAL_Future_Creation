#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HALMovementAreaTypes.h"
#include "HALMovementArea.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class ATestVehiclePawn;

/** Reusable authority volume. Its box is a query, never an obstacle or a physical material override. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALMovementArea : public AActor
{
	GENERATED_BODY()
public:
	AHALMovementArea();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	/** Runtime gimmicks may toggle the area on authority; enabling scans vehicles already inside. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Movement Area") void SetAreaEnabled(bool bInEnabled);
	bool ConfigureArea(const FHALMovementAreaConfig& Config);
	UBoxComponent* GetAreaBounds() const { return Bounds; }
private:
	void ReconcileOverlaps();
	void RegisterVehicle(ATestVehiclePawn* Vehicle);
	void ClearVehicles();
	UFUNCTION() void OverlapBegan(UPrimitiveComponent* Component, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		int32 BodyIndex, bool bFromSweep, const FHitResult& Hit);
	UFUNCTION() void OverlapEnded(UPrimitiveComponent* Component, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 BodyIndex);
	UFUNCTION() void VehicleEnded(AActor* Actor, EEndPlayReason::Type Reason);
	UPROPERTY(VisibleAnywhere, Category="Movement Area") TObjectPtr<UBoxComponent> Bounds;
	/** Edit before Play, or use ConfigureArea from authoritative C++ for runtime changes. */
	UPROPERTY(EditAnywhere, Replicated, Category="Movement Area") FHALMovementAreaConfig Settings;
	UPROPERTY(EditAnywhere, Replicated, Category="Movement Area") bool bEnabled = true;
	TSet<TWeakObjectPtr<ATestVehiclePawn>> Vehicles;
};
