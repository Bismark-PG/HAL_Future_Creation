#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HALBallPresentationTypes.h"
#include "HALBallPresentationComponent.generated.h"

class ABasicBallActor;
class UStaticMeshComponent;
class FHALBallPoseCapture;
namespace Chaos { class FPBDRigidsSolver; }

/** Authority PostSolve relative samples; client PostPhysics mesh-only display. */
UCLASS(ClassGroup=(Ball))
class HAL_FUTURE_CREATION_API UHALBallPresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALBallPresentationComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	void Configure(const FHALBallPresentationConfig& InConfig);
	/** Called at the existing authority state commit, before a holder's physics body can be removed. */
	void NotifyStateChanged();
	EHALBallVisualMode GetVisualMode() const { return Mode; }
	uint32 GetPublishedSampleCount() const { return PublishedCount; }
	uint32 GetReceivedSampleCount() const { return ReceivedCount; }
	uint32 GetRelativeDisplayFrames() const { return RelativeFrames; }
	int32 GetLastSampleFrame() const { return AcceptedPose.PhysicsFrame; }
	float GetSampleAge() const { return SampleAge; }
	FTransform GetDisplayedRootPose() const { return DisplayPose; }
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALBallPresentationTestAccess;
#endif
	void StartCapture();
	void StopCapture();
	void PollCapture();
	void UpdateClient(float DeltaTime);
	void RestoreVisual();
	void SetMode(EHALBallVisualMode Next);
	UFUNCTION() void OnRep_RelativePose();
	UPROPERTY(ReplicatedUsing=OnRep_RelativePose) FHALBallRelativePose ReplicatedPose;
	UPROPERTY(Transient) FHALBallRelativePose AcceptedPose;
	FHALBallPresentationConfig Config;
	TWeakObjectPtr<ABasicBallActor> Ball;
	TWeakObjectPtr<UStaticMeshComponent> Visual;
	TWeakObjectPtr<AActor> CaptureHolder, DisplayHolder;
	FHALBallPoseCapture* Capture=nullptr;
	Chaos::FPBDRigidsSolver* CaptureSolver=nullptr;
	uint32 CaptureSequence=0, DisplaySequence=0;
	uint32 PublishedCount=0, ReceivedCount=0, RelativeFrames=0;
	float LastPublishSeconds=-1, LastReceiveSeconds=-1, SampleAge=0, HandoffAge=0, LogAge=0;
	EHALBallVisualMode Mode=EHALBallVisualMode::Physics;
	FTransform AuthoredVisual=FTransform::Identity, DisplayPose=FTransform::Identity, RelativePose=FTransform::Identity;
	FTransform HandoffStart=FTransform::Identity;
	FVector EntryOffset=FVector::ZeroVector;
	FQuat EntryRotation=FQuat::Identity;
	float EntryAge=0;
	bool bVisualOverride=false, bAuthoredAbsoluteLocation=false, bAuthoredAbsoluteRotation=false;
};
