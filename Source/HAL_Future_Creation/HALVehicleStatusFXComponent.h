#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HALVehicleFXTargets.h"
#include "HALVehicleStatusFXRuntime.h"
#include "HALVehicleStatusFXComponent.generated.h"

class UHALVehiclePresentationComponent;
class UHALVehicleStatusFXSubsystem;
class UHALVehicleStatusFXDefinition;
class UVehicleDefinition;
class UMaterialInstanceDynamic;
class UNiagaraComponent;
class UAbilitySystemComponent;
class UHALVehicleStatusFXRenderer;

struct FHALStatusFXSourceDiagnostic
{
	FGameplayTag Tag;
	FName Kind, Name;
	double AgeSeconds = 0;
	bool bSubmitted = false;
};

/** Local preparation counters, not gameplay stacks or proof of rendered/GPU-ready effects. */
struct FHALStatusFXDiagnostics
{
	FGuid Binding;
	uint64 MeshGeneration = 0;
	bool bBound = false, bObservingASC = false, bTickEnabled = false;
	int32 Leases = 0, ActiveSources = 0, ASCSources = 0, DebugSources = 0, PendingRecipes = 0;
	int32 Targets = 0, Materials = 0, Particles = 0, RegisteredParticles = 0, ActiveParticles = 0;
	int32 OverlayTargets = 0, SurfaceChannels = 0, RejectedSurfaces = 0, RejectedParticles = 0;
	FString RenderError;
	TArray<FHALStatusFXSourceDiagnostic> Sources;
	TArray<FHALStatusFXInstanceSnapshot> Instances;
};

/** Prepared, inactive particle channel. Sampling/activation belongs to the FX-4 renderer. */
struct FHALStatusFXParticleRequest
{
	FName LayerId;
	TWeakObjectPtr<UNiagaraComponent> Component;
};

/** A borrowed request, valid only for this owner/instance/mesh generation. No overlay is assigned here. */
struct FHALStatusFXRenderRequest
{
	FHALStatusFXInstanceSnapshot State;
	uint64 MeshGeneration = 0;
	FHALVehicleFXTarget Target;
	TWeakObjectPtr<UHALVehicleStatusFXDefinition> Recipe;
	/** Shared by requests with the same mesh/base material on THIS car. FX-4 selects/composes one output. */
	TWeakObjectPtr<UMaterialInstanceDynamic> Surface;
	TArray<FHALStatusFXParticleRequest> Particles;
};

USTRUCT()
struct FHALStatusFXPreparedSurface
{
	GENERATED_BODY()
	FName TargetId;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Material;
};

USTRUCT()
struct FHALStatusFXPreparedParticle
{
	GENERATED_BODY()
	FHALStatusFXInstanceHandle Instance;
	FName TargetId, LayerId;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> Component;
	bool bConfigured = false, bAttemptedConfiguration = false, bBoundsFallback = false;
	FString RenderState;
	FString FallbackReason;
};

struct FHALStatusFXDemand
{
	FHALStatusFXSourceToken Source;
	FGameplayTag Tag;
	double ObservedAt = 0;
	bool bSubmitted = false;
};

/** Local per-vehicle resources and demand, never gameplay authority. FX-3B prepares but does not draw. */
UCLASS(ClassGroup=(Vehicle))
class HAL_FUTURE_CREATION_API UHALVehicleStatusFXComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALVehicleStatusFXComponent();
	/** Same tuple is idempotent. A changed Avatar identity revokes all previous sources/requests. */
	void Bind(UHALVehiclePresentationComponent* Provider, UHALVehicleStatusFXSubsystem* Cache,
		const UVehicleDefinition* Vehicle, const UObject* AvatarIdentity);
	void Unbind();
	/** Sole owner of the final mesh overlay, including the dedicated shock surface. */
	UHALVehicleStatusFXRenderer* GetSurfaceRenderer();
	const UHALVehicleStatusFXDefinition* FindSharedShockRecipe() const;
	void CaptureShockTargets(FHALVehicleFXTargetSnapshot& Out) const;
	bool OwnsSource(FHALStatusFXSourceToken Source) const { return Runtime.HasSource(Source); }
	/** Attach only to the ASC identity of this binding and this Avatar. Reads current explicit tags first. */
	bool ObserveAbilitySystem(UAbilitySystemComponent* InASC);
	FHALStatusFXDiagnostics CaptureDiagnostics();
	void LogDiagnostics();
#if !UE_BUILD_SHIPPING
	/** Local cosmetic leases only. Never adds a GAS tag, GE, RPC, or gameplay force. */
	bool SetDebugSource(FName Name, FGameplayTag Tag, bool bActive);
	void ClearDebugSources();
#endif
	FHALStatusFXSourceToken AcquireSource();
	/** Cache-only: retains demand if the recipe is still loading; no synchronous loads at state time. */
	bool Submit(FHALStatusFXSourceToken Source, FGameplayTag Tag);
	bool RemoveSource(FHALStatusFXSourceToken Source);
	void CaptureRenderRequests(TArray<FHALStatusFXRenderRequest>& Out);
	bool IsRequestCurrent(const FHALStatusFXRenderRequest& Request);
	TArray<FHALStatusFXInstanceSnapshot> GetRuntimeSnapshots() const { return Runtime.GetSnapshots(); }
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void OnUnregister() override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALStatusFXComponentTestAccess;
#endif
	double Now() const;
	void Refresh();
	void ReleaseResources();
	void PrepareRequests(double Time);
	void RenderParticles();
	void ReconcileObservedTags();
	void DetachObserver();
	void LogStateChange(const TCHAR* Reason);
	bool HasValidObserver() const;
	TWeakObjectPtr<UAbilitySystemComponent> ObservedASC;
	FDelegateHandle TagsChanged;
	TMap<FGameplayTag, FHALStatusFXSourceToken> ObservedSources;
	TMap<FName, FHALStatusFXSourceToken> DebugSources;
	TWeakObjectPtr<UHALVehiclePresentationComponent> Provider;
	TWeakObjectPtr<UHALVehicleStatusFXSubsystem> Cache;
	TWeakObjectPtr<const UVehicleDefinition> Vehicle;
	TWeakObjectPtr<const UObject> AvatarIdentity;
	FDelegateHandle TargetChanged, CatalogChanged;
	FHALStatusFXRuntime Runtime;
	TArray<FHALStatusFXDemand> Demands;
	FHALVehicleFXTargetSnapshot Targets;
	TArray<FHALStatusFXRenderRequest> Requests;
	UPROPERTY(Transient) TMap<FGameplayTag, TObjectPtr<UHALVehicleStatusFXDefinition>> Recipes;
	UPROPERTY(Transient) TArray<FHALStatusFXPreparedSurface> Surfaces;
	UPROPERTY(Transient) TArray<FHALStatusFXPreparedParticle> Particles;
	UPROPERTY(Transient) TObjectPtr<UHALVehicleStatusFXRenderer> Renderer;

	double Epoch = 0;
	uint64 MeshGeneration = 0;
	bool bBound = false, bRefreshing = false, bHasAvatarIdentity = false, bObservingASC = false, bReconcilingTags = false;
};
