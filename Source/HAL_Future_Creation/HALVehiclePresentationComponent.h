#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HALVehiclePresentationTypes.h"
#include "HALVehiclePresentationComponent.generated.h"

class UHALVehiclePresentationConfig;
class UPrimitiveComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;

/** Visual-only wheel/chassis animation. Observers receive steering and a driving-lean permission bit. */
UCLASS(ClassGroup = (Vehicle))
class HAL_FUTURE_CREATION_API UHALVehiclePresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALVehiclePresentationComponent();
	void Configure(UHALVehiclePresentationConfig* InConfig, UPrimitiveComponent* InBody,
		UStaticMeshComponent* InFallback, USkeletalMeshComponent* InMesh, bool bPreview);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FHALVehiclePresentationFrame GetPresentationFrame() const { return Frame; }
	bool IsUsingSkeletalPresentation() const { return bConfigured; }
	const USkeletalMeshComponent* GetPresentationMesh() const { return Mesh; }
	float GetObserverSteering() const { return float(ObserverSteering) / 127.f; }
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALPresentationTestAccess;
#endif
	void StopPresentation();
	void ResetPose();
	float ReadSteering(float DeltaTime, bool bDrivingLeanAllowed);
	bool ReadDrivingLeanAllowed() const;
	UPROPERTY(Transient) TObjectPtr<UHALVehiclePresentationConfig> Config;
	UPROPERTY(Transient) TObjectPtr<UPrimitiveComponent> Body;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Fallback;
	UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Mesh;
	/** Server accepted input, quantized to [-127,127]. Owners use their own local input. */
	UPROPERTY(Replicated) int8 ObserverSteering = 0;
	/** Cosmetic only. Remote clients do not run the movement component's support probe. */
	UPROPERTY(Replicated) bool bObserverDrivingLeanAllowed = false;
	FHALVehiclePresentationFrame Frame;
	FHALChassisPresentationState ChassisState;
	FVector ReferenceWheelCenters[4] {};
	float SpinDegrees[4] {};
	FVector PreviousPosition = FVector::ZeroVector;
	float PublishElapsed = 0;
	bool bHasSample = false;
	bool bConfigured = false;
	bool bFallbackWasVisible = false;
	bool bFallbackWasHiddenInGame = false;
	TWeakObjectPtr<AController> PreviousController;
};
