#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HALVehiclePresentationTypes.h"
#include "HALVehicleFXTargets.h"
#include "HALVehiclePresentationComponent.generated.h"

class UHALVehiclePresentationConfig;
class UPrimitiveComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UNiagaraComponent;
class UMaterialBillboardComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
struct FHALImpactBurstConfig;

struct FHALFadingTireSkid
{
	TWeakObjectPtr<UNiagaraComponent> Component;
	double Deadline = 0;
};

/** Cosmetic wheel/chassis/trails. Observers receive accepted steering, drive permission and skid demand. */
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
	/** Read-only cosmetic targets, game thread only. Includes the actual active visual path. */
	void CaptureStatusFXTargets(FHALVehicleFXTargetSnapshot& Out) const;
	/** Assembly-time multipart registration; same owner, unique semantic Id, never the physics body. */
	bool RegisterStatusFXPart(FName Id, UMeshComponent* Part);
	void UnregisterStatusFXPart(FName Id);
	/** Explicit invalidation on retirement/reconfiguration. Weak bindings never keep old meshes alive. */
	void ClearStatusFXBindings();
	/** Tests binding identity/visibility/assets, not transform freshness. Always re-query for geometry. */
	bool AreStatusFXBindingsCurrent(const FHALVehicleFXTargetSnapshot& Snapshot) const;
	FSimpleMulticastDelegate& OnStatusFXBindingsChanged() { return StatusFXBindingsChanged; }
	float GetObserverSteering() const { return float(ObserverSteering) / 127.f; }
	uint8 GetObserverTireSkidInputs() const { return ObserverTireSkidInputs; }
	/** Owning-client input edge only; no force or authority request is performed here. */
	void PredictLaunchRebound(uint32 Sequence);
	/** Authority accepted shot, cosmetic multicast; owner also gets the existing reliable result. */
	void PublishAcceptedLaunch(uint32 Sequence, float ServerTimeSeconds);
	void ReceiveLaunchResult(uint32 Sequence, bool bAccepted, float ServerTimeSeconds);
	/** Called once at the completed authority TieredV1 result boundary; no gameplay mutation.
	 * The contact point is cosmetic: it is validated against the target and may be replaced by a fallback. */
	void PublishAcceptedImpact(const FHALCombatHitResult& Result, const FVector& WorldDirection, const FVector& WorldImpactPoint);
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALPresentationTestAccess;
#endif
	void StopPresentation();
	FSimpleMulticastDelegate StatusFXBindingsChanged;
	TMap<FName, TWeakObjectPtr<UMeshComponent>> StatusFXParts;
	uint32 StatusFXBindingRevision = 0;
	bool bStatusFXBound = false;
	void ResetPose();
	float ReadSteering(float DeltaTime, bool bDrivingLeanAllowed);
	bool ReadDrivingLeanAllowed() const;
	void UpdateSuspension(float DeltaTime);
	FTransform GetWheelSampleTransform() const;
	uint8 ReadTireSkidInputs() const;
	void UpdateTireSkids(const FVector& Velocity, bool bDrivingAllowed);
	void StopTireSkid(int32 Slot, bool bImmediate);
	void ClearTireSkids();
	void ConsumeAcceptedLaunch(uint32 Sequence, float ServerTimeSeconds);
	double GetLaunchServerTime() const;
	UFUNCTION(NetMulticast, Unreliable) void MulticastAcceptedLaunch(uint32 Sequence, float ServerTimeSeconds);
	UFUNCTION(NetMulticast, Unreliable) void MulticastImpactRebound(FHALImpactVisualEvent Event);
	void ConsumeImpactRebound(const FHALImpactVisualEvent& Event);
	bool IsImpactPoseAllowed() const;
	FHALImpactVisualOutput UpdateImpactRebound(float DeltaTime);
	/** Allocated once per accepted impact and shared by every cosmetic event it publishes. */
	uint32 AllocateImpactSequence();
	UFUNCTION(NetMulticast, Unreliable) void MulticastImpactBurst(FHALImpactBurstEvent Event);
	void PublishImpactBurst(const FHALCombatHitResult& Result, const FVector& WorldDirection,
		const FVector& WorldImpactPoint, uint32 Sequence);
	void ConsumeImpactBurst(const FHALImpactBurstEvent& Event);
	/** World anchored and independent of the vehicle pose, so a teleport reset never freezes one. */
	void UpdateImpactBursts(float DeltaTime);
	void ClearImpactBursts();
	void ReleaseBurstSlots(const TArray<int32>& Slots);
	void ApplyBurstSlot(const FHALImpactBurstConfig& BurstConfig, const FHALImpactBurstPulse& Pulse);
	/** Settings are read once outside the hit path; changing them needs a session restart. */
	void ResolveBurstMaterial();
	int32 AcquireBurstSlot(const FHALImpactBurstConfig& BurstConfig);
	/** Null whenever the feature is disabled, misconfigured or its material is not loaded. */
	const FHALImpactBurstConfig* GetBurstConfig() const;
	UPROPERTY(Transient) TObjectPtr<UHALVehiclePresentationConfig> Config;
	UPROPERTY(Transient) TObjectPtr<UPrimitiveComponent> Body;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Fallback;
	UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Mesh;
	/** Server accepted input, quantized to [-127,127]. Owners use their own local input. */
	UPROPERTY(Replicated) int8 ObserverSteering = 0;
	/** Cosmetic only. Remote clients do not run the movement component's support probe. */
	UPROPERTY(Replicated) bool bObserverDrivingLeanAllowed = false;
	UPROPERTY(Replicated) uint8 ObserverTireSkidInputs = 0;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> TireSkidComponents[4];
	TArray<FHALFadingTireSkid> FadingTireSkids;
	FHALWheelSupportSample WheelSupports[4];
	FVector LastTireSkidPoints[4] {};
	int32 NextRibbonID = 0;
	bool bTireSkidReady = false;
	FHALVehiclePresentationFrame Frame;
	FHALChassisPresentationState ChassisState;
	FHALLaunchVisualState LaunchState;
	FHALImpactVisualState ImpactState;
	FHALImpactBurstState BurstState;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialBillboardComponent>> BurstSprites;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> BurstMaterials;
	/** Resolved once outside the hit path; impacts never load the sprite material synchronously. */
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BurstMaterial;
	bool bBurstReady = false;
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
