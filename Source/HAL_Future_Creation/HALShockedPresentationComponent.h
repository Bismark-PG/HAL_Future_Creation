#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Curves/CurveFloat.h"
#include "GameplayTagContainer.h"
#include "HALShockVisualSettings.h"
#include "HALVehicleStatusFXRuntime.h"
#include "HALShockedPresentationComponent.generated.h"

class UHALAbilitySystemComponent;
class UArcadeVehicleMovementComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UPointLightComponent;
class UHALVehicleStatusFXRenderer;
class UHALVehicleStatusFXComponent;
class UHALVehicleStatusFXDefinition;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHALShockedChanged, bool, bActive);

/**
 * Shock timeline phase.
 * Release fades the current out; Cooling then burns the scorch off on its own,
 * slower clock, because soot has to outlive the arcs that made it.
 */
enum class EHALShockPhase : uint8 { Idle, Active, Release, Cooling };

/** Event-only visual hook; no physics, ability restrictions, asset writes or authoritative state. */
UCLASS(ClassGroup=(Presentation))
class HAL_FUTURE_CREATION_API UHALShockedPresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALShockedPresentationComponent();
	void Bind(UHALAbilitySystemComponent* InASC);
	void Unbind();
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	/** Optional world-space shock origin, so the overlay wipe spreads from the real impact point. */
	void RefreshShocked();
	void NotifyShockOrigin(const FVector& WorldLocation);
	UFUNCTION(BlueprintPure, Category="Shocked") bool IsShocked() const { return bShocked; }
	UPROPERTY(BlueprintAssignable, Category="Shocked") FHALShockedChanged OnShockedChanged;
protected:
	/** Cosmetic arc looped while the shocked tag is present. Never reads or writes HP, physics or ball ownership. */
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Arc FX") TObjectPtr<UNiagaraSystem> ArcSystem;
	/** Offset from the physics body, in vehicle local axes. The arc tracks the authoritative pose, not the visual mesh. */
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Arc FX", meta=(Units="cm")) FVector ArcOffsetCm = FVector::ZeroVector;
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Arc FX", meta=(ClampMin="0.01", ClampMax="20")) float ArcScale = 1;

	/** Body-wrapping layer: current filaments and the scorch they leave, over the vehicle materials. */
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Overlay") TObjectPtr<UMaterialInterface> OverlayMaterial;
	/** Whole-body punch at the start; this is what makes the hit readable from the top-down camera. */
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Overlay", meta=(Units="s", ClampMin="0", ClampMax="0.5")) float FlashSeconds = 0.06f;
	/** How long the discharge takes to spread from the origin across the body. */
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Overlay", meta=(Units="s", ClampMin="0", ClampMax="1")) float WipeSeconds = 0.12f;
	/** Final wipe radius; needs to exceed the vehicle's own extent so the body ends fully covered. */
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Overlay", meta=(Units="cm", ClampMin="10", ClampMax="2000")) float WipeRadiusCm = 400.f;
	/** Current afterglow once the shocked tag is gone. Arcs already spawned keep their own lifetime. */
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Overlay", meta=(Units="s", ClampMin="0", ClampMax="3")) float FadeOutSeconds = 0.6f;
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Overlay", meta=(ClampMin="0", ClampMax="60")) float BaseBrightness = 8.f;
	/** Added to the filament exponent across the fade, so the lines thin out instead of only dimming. */
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Overlay", meta=(ClampMin="0", ClampMax="120")) float FadeFilamentGain = 40.f;
	/** Steps per second of the jump clock. Pushed to the material so filaments and light pulse together. */
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Overlay", meta=(ClampMin="1", ClampMax="60")) float FlickerSteps = 14.f;
	/** Brightness multiplier against seconds held. Values past the last key hold, so long stuns stay stable. */
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Overlay") FRuntimeFloatCurve BrightnessCurve;
	/** Filament exponent against seconds held; rising values thin the lines out as the charge dissipates. */
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Overlay") FRuntimeFloatCurve FilamentPowerCurve;

	/**
	 * Ribbon renderer for the hero arcs. The asset owns no look: it spawns one particle per
	 * point and reads Positions / Widths, which this component rebuilds on the flicker clock.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Hero Arcs") TObjectPtr<UNiagaraSystem> RibbonSystem;
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Hero Arcs") FHALShockArcSettings ArcSettings;
	/**
	 * Arc tint. The ribbon material keeps a thin white-hot spine whatever this is, so the
	 * colour is read from the halo around it: push saturation here, not brightness.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Hero Arcs") FLinearColor ArcColor = FLinearColor(0.22f, 0.52f, 1.f, 1.f);
	/**
	 * On release the arc withdraws from its tail instead of dimming as a whole, which reads
	 * as the current draining away rather than someone turning a dimmer down.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Hero Arcs") bool bRetractArcsOnRelease = true;
	/** Softness of the retracting front, as a fraction of each ribbon's length. */
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Hero Arcs", meta=(ClampMin="0.01", ClampMax="1")) float ArcRetractFeather = 0.25f;

	/** How fast the soot builds up while the current is on the body. */
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Scorch", meta=(Units="s", ClampMin="0", ClampMax="3")) float ScorchRiseSeconds = 0.25f;
	/**
	 * How long the scorch takes to burn off after the current is gone. Cosmetic only:
	 * the mark is never gameplay state, so it always clears on its own.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Scorch", meta=(Units="s", ClampMin="0", ClampMax="30")) float ScorchFadeSeconds = 4.f;

	/** Off by default: the pool of light it throws on the arena floor costs more readability than it buys. */
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Light") bool bEnableLight = false;
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Light") FLinearColor LightColor = FLinearColor(0.55f, 0.88f, 1.f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Light", meta=(Units="Candela", ClampMin="0", ClampMax="200000")) float LightPeakIntensity = 2200.f;
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Light", meta=(Units="cm", ClampMin="50", ClampMax="5000")) float LightRadiusCm = 280.f;
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Light", meta=(Units="cm")) FVector LightOffsetCm = FVector(0.f, 0.f, 40.f);
	/** Lower bound of the flicker, so the light never drops fully out between steps. */
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Light", meta=(ClampMin="0", ClampMax="1")) float LightFlickerFloor = 0.6f;
	/** Extra punch while the whole-body flash is up. */
	UPROPERTY(EditDefaultsOnly, Category="Shocked|Light", meta=(ClampMin="0", ClampMax="10")) float LightFlashBoost = 0.6f;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALShockFXTestAccess;
#endif
	void SelectLook();
	void SyncSharedSource();
	void ReleaseSharedSource();
	void DestroyVisualComponents();
	void SyncSharedOverlay(bool bResidue);
	void StartArc();
	void StopArc();
	void BeginTimeline();
	void ReleaseTimeline();
	void BeginCooling();
	void ClearTimeline();
	void EnsureOverlay();
	UHALVehicleStatusFXRenderer* OverlayOwner();
	UPROPERTY(Transient) TObjectPtr<UHALVehicleStatusFXRenderer> FallbackOverlayOwner;
	void EnsureLight();
	void EnsureRibbon();
	/** Rebuilds the arcs when the flicker clock steps, then pushes the current fade alpha. */
	void PushArcs(float FilamentAlpha);
	/** Soot built up so far, from seconds held. */
	float ScorchRamp() const;
	/** Single source of the look: filaments and scorch fade on separate clocks. */
	void ApplyTimeline(float FilamentAlpha, float ScorchAlpha);
	float ComputeFlicker() const;
	void TagChanged(FGameplayTag Tag, int32 Count);
	void SetShocked(bool bActive);
	void ExpireShockHandoff();
	FTimerHandle ShockHandoffTimer;
	bool bAwaitingAuthorityShock = false;
	TWeakObjectPtr<UArcadeVehicleMovementComponent> LocalBounceMovement;
	UPROPERTY(Transient) TObjectPtr<UHALAbilitySystemComponent> ASC;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> ArcComponent;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> ShockLight;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> RibbonComponent;
	/** One instance per visible mesh, so a vehicle with several body parts stays consistent. */
	UPROPERTY(Transient) TArray<TObjectPtr<UMeshComponent>> OverlayMeshes;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> OverlayInstances;
	/** Pinned across the current/release/cooling episode, so catalog readiness never restarts it. */
	UPROPERTY(Transient) TObjectPtr<const UHALVehicleStatusFXDefinition> SharedRecipe;
	UPROPERTY(Transient) FHALShockVisualSettings Look;
	TWeakObjectPtr<UHALVehicleStatusFXComponent> SharedStatus;
	FHALStatusFXSourceToken SharedSource;
	float LookFadeOutSeconds = 0.f;
	float LookScorchFadeSeconds = 0.f;
	FDelegateHandle TagHandle;
	EHALShockPhase Phase = EHALShockPhase::Idle;
	float PhaseSeconds = 0.f;
	float HeldSeconds = 0.f;
	/** Scorch level captured when the current stopped; a brief shock leaves a lighter mark. */
	float ScorchPeak = 0.f;
	/** Reused between rebuilds so the per-step arc refresh does not reallocate. */
	TArray<FVector> ArcPositions;
	TArray<float> ArcWidths;
	/** Widths as built. ArcWidths is this with the retraction mask applied, rebuilt per frame. */
	TArray<float> ArcBaseWidths;
	/** Retraction the widths were last masked for; negative forces the first push. */
	float LastArcRetract = -1.f;
	FVector BodyExtentCm = FVector(100.f);
	/** Flicker step the arcs were last built for; -1 forces a build on the first frame. */
	int32 LastArcStep = -1;
	FVector ShockOriginWS = FVector::ZeroVector;
	/** Cosmetic only: keeps four shocked vehicles from flickering in sync. */
	uint32 FlickerSeed = 0;
	bool bHasShockOrigin = false;
	bool bShocked = false;
};
