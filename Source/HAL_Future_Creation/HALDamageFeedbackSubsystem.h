#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "HALDamageFeedbackConfig.h"
#include "HALDamageFeedbackSubsystem.generated.h"

class UMeshComponent;
class UMaterialInstanceDynamic;
class APostProcessVolume;
struct FStreamableHandle;

struct FHALDamageFeedbackMeshLease
{
	TWeakObjectPtr<UMeshComponent> Mesh;
	int32 PreviousStencil=0;
	int32 AssignedStencil=0;
	ERendererStencilMask PreviousMask=ERendererStencilMask::ERSM_Default;
	bool bPreviousCustomDepth=false;
};
struct FHALDamageFeedbackSlot
{
	TWeakObjectPtr<AActor> Target;
	FHALDamageFeedbackPlayback Playback;
	TArray<FHALDamageFeedbackMeshLease> Meshes;
};

/** One local pass per rendering world; independent target envelopes share a global immutable style. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALDamageFeedbackSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return !Slots.IsEmpty()||!Pending.IsEmpty(); }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UHALDamageFeedbackSubsystem,STATGROUP_Tickables); }
	static double ServerTime(const UWorld* World);
	void Receive(AActor* Target,const FHALDamageFeedbackPulse& Pulse);
	void ClearTarget(AActor* Target);
	int32 GetActiveTargetCount() const;
	int32 GetLeasedMeshCount() const;
	bool IsReady() const { return bPrepared&&Config&&Config->Style.bEnabled; }
	void LogStatus() const;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FHALDamageFeedbackRenderTest;
	friend class FHALDamageFeedbackLeaseTest;
	friend struct FHALDamageFeedbackNetworkAccess;
#endif
	void CompleteConfig();
	void UpdateRendering();
	void RefreshTargets(int32 Index);
	void RestoreMeshes(int32 Index);
	UPROPERTY(Transient) TObjectPtr<UHALDamageFeedbackConfig> Config;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Material;
	UPROPERTY(Transient) TObjectPtr<APostProcessVolume> Volume;
	TArray<FHALDamageFeedbackSlot> Slots;
	TArray<TPair<TWeakObjectPtr<AActor>,FHALDamageFeedbackPulse>> Pending;
	TSharedPtr<FStreamableHandle> Handle;
	FSoftObjectPath ConfigPath;
	bool bStopped=false,bPrepared=false;
};
