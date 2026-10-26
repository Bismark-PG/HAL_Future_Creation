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
	/** Owning-client input edge only; no force or authority request is performed here. */
	void PredictLaunchRebound(uint32 Sequence);
	/** Authority accepted shot, cosmetic multicast; owner also gets the existing reliable result. */
	void PublishAcceptedLaunch(uint32 Sequence, float ServerTimeSeconds);
	void ReceiveLaunchResult(uint32 Sequence, bool bAccepted, float ServerTimeSeconds);
	/** Called once at the completed authority TieredV1 result boundary; no gameplay mutation. */
	void PublishAcceptedImpact(const FHALCombatHitResult& Result, const FVector& WorldDirection);
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALPresentationTestAccess;
#endif
	void StopPresentation();
	void ResetPose();
	float ReadSteering(float DeltaTime, bool bDrivingLeanAllowed);
	bool ReadDrivingLeanAllowed() const;
	void UpdateSuspension(float DeltaTime);
	void ConsumeAcceptedLaunch(uint32 Sequence, float ServerTimeSeconds);
	double GetLaunchServerTime() const;
	UFUNCTION(NetMulticast, Unreliable) void MulticastAcceptedLaunch(uint32 Sequence, float ServerTimeSeconds);
	UFUNCTION(NetMulticast, Unreliable) void MulticastImpactRebound(FHALImpactVisualEvent Event);
	void ConsumeImpactRebound(const FHALImpactVisualEvent& Event);
	bool IsImpactPoseAllowed() const;
	FHALImpactVisualOutput UpdateImpactRebound(float DeltaTime);
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
	FHALLaunchVisualState LaunchState;
	FHALImpactVisualState ImpactState;
	uint32 NextImpactVisualSequence = 0;
	double LaunchResetLocalTime = 0;
	FVector ReferenceWheelCenters[4] {};
	FTransform ReferenceSuspensions[4];
	float SuspensionOffsets[4] {};
	float SuspensionDebugElapsed = 0;
	float SpinDegrees[4] {};
	FVector PreviousPosition = FVector::ZeroVector;
	float PublishElapsed = 0;
	bool bHasSample = false;
	bool bConfigured = false;
	bool bFallbackWasVisible = false;
	bool bFallbackWasHiddenInGame = false;
	TWeakObjectPtr<AController> PreviousController;
};
