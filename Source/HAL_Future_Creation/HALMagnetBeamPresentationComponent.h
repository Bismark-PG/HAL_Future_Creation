#pragma once
#include "CoreMinimal.h"
#include "BasicBallActor.h"
#include "Components/ActorComponent.h"
#include "HALMagnetBeamGeometry.h"
#include "HALMagnetBeamPresentationComponent.generated.h"

class UHALBallPresentationComponent;
class UMaterialInstanceDynamic;
class UNiagaraComponent;
class UNiagaraSystem;
class UPrimitiveComponent;

/**
 * Beam timeline phase.
 * Release and Launch are separate because they mean opposite things: Release is the grip
 * being broken from outside, so the beam snaps back toward the roof, while Launch is the
 * holder spending the ball, so the beam collapses into it.
 */
enum class EHALMagnetBeamPhase : uint8 { Idle, Attach, Hold, Release, Launch };

/**
 * Ball-side ownership presentation: the magnet beam arcing from the holder's roof, and the
 * holder-coloured pulse on the ball shell. Event-only visual hook; it never reads or writes
 * HP, physics, acquisition or launch permission.
 *
 * It lives on the ball rather than on the vehicle for three reasons: the state that drives
 * it (Controlled plus Holder) is already replicated on the ball with OnRep as the event hook,
 * every vehicle gets the effect without per-Blueprint wiring, and each ball owns its own
 * effect once there is more than one ball in play.
 */
UCLASS(ClassGroup=(Ball))
class HAL_FUTURE_CREATION_API UHALMagnetBeamPresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALMagnetBeamPresentationComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

	/**
	 * Called from both the authority state commit and the client OnRep, because the
	 * authority path of the ball's own presentation returns early and a listen-server
	 * host would otherwise see nothing. The component diffs against its own cached state,
	 * so neither caller has to know what changed.
	 */
	void SyncToBallState(EBasicBallState State, AActor* Holder);

	/** Hides the beam arc only: phases, identity colour and the shell pulse keep running. Set by the doodle hold
	 *  rings, which draw ownership themselves; two ownership marks for one fact would be noise. */
	void SetBeamRibbonSuppressed(bool bSuppressed);
	bool IsBeamRibbonSuppressed() const { return bRibbonSuppressed; }

	EHALMagnetBeamPhase GetPhase() const { return Phase; }
	/** Colour this component resolved for the current holder; read by tests and diagnostics. */
	FLinearColor GetResolvedColor() const { return ResolvedColor; }
	/** The one identity colour for ball ownership layers (beam, ink trail, aim arrow): room seat colour, then the
	 *  PlayerId palette, then the car's own mark colour for actors without a PlayerState, then the fallback.
	 *  PlayerId is only consulted when the pawn is gone, e.g. a launcher that left while its shot is in flight. */
	FLinearColor ResolveIdentityColor(const AActor* Actor, int32 PlayerId = INDEX_NONE) const;

protected:
	/** Ribbon renderer for the beam. The asset owns no look: it spawns one particle per point
	 *  and reads the arrays this component rebuilds. Local Space must be off on the asset,
	 *  because the positions pushed here are world space. */
	UPROPERTY(EditDefaultsOnly, Category="Magnet Beam") TObjectPtr<UNiagaraSystem> BeamSystem;
	UPROPERTY(EditDefaultsOnly, Category="Magnet Beam") FHALMagnetBeamSettings BeamSettings;

	/** Where the beam leaves the holder, derived from the holder body's own bounds so a
	 *  bigger vehicle emits from its own roof instead of a hardcoded height. */
	UPROPERTY(EditDefaultsOnly, Category="Magnet Beam|Anchors", meta=(ClampMin="-1", ClampMax="1"))
	float RoofBackFraction = 0.18f;
	UPROPERTY(EditDefaultsOnly, Category="Magnet Beam|Anchors", meta=(Units="cm", ClampMin="0", ClampMax="200"))
	float RoofLiftCm = 6.f;
	/** How far up the ball the beam lands, as a fraction of its radius. */
	UPROPERTY(EditDefaultsOnly, Category="Magnet Beam|Anchors", meta=(ClampMin="-1", ClampMax="1"))
	float BallEntryHeightFraction = 0.6f;
	/** Beyond this the pair cannot plausibly be holder and held, so nothing is drawn. */
	UPROPERTY(EditDefaultsOnly, Category="Magnet Beam|Anchors", meta=(Units="cm", ClampMin="100", ClampMax="5000"))
	float MaximumBeamLengthCm = 1200.f;

	/** Helix revolutions per second. Continuous drift, not a stepped flicker: the shock arcs
	 *  jump because electricity does, while a magnetic field should breathe. */
	UPROPERTY(EditDefaultsOnly, Category="Magnet Beam|Timeline", meta=(ClampMin="0", ClampMax="8"))
	float DriftTurnsPerSecond = 0.35f;
	UPROPERTY(EditDefaultsOnly, Category="Magnet Beam|Timeline", meta=(Units="s", ClampMin="0.01", ClampMax="1"))
	float AttachSeconds = 0.14f;
	UPROPERTY(EditDefaultsOnly, Category="Magnet Beam|Timeline", meta=(Units="s", ClampMin="0.01", ClampMax="2"))
	float ReleaseSeconds = 0.3f;
	UPROPERTY(EditDefaultsOnly, Category="Magnet Beam|Timeline", meta=(Units="s", ClampMin="0.01", ClampMax="1"))
	float LaunchSeconds = 0.12f;
	/** Extra apex and spread at the start of Attach, settling to the authored values. */
	UPROPERTY(EditDefaultsOnly, Category="Magnet Beam|Timeline", meta=(ClampMin="0", ClampMax="4"))
	float AttachOvershoot = 1.6f;
	/** Softness of the retracting front, as a fraction of each strand's length. */
	UPROPERTY(EditDefaultsOnly, Category="Magnet Beam|Timeline", meta=(ClampMin="0.01", ClampMax="1"))
	float RetractFeather = 0.25f;

	/** Beam length that counts as fully strained. Past the resting gap the strands thicken and
	 *  brighten, which is what makes "about to lose the ball" readable before it happens. */
	UPROPERTY(EditDefaultsOnly, Category="Magnet Beam|Tension", meta=(Units="cm", ClampMin="50", ClampMax="2000"))
	float TensionReferenceCm = 320.f;
	UPROPERTY(EditDefaultsOnly, Category="Magnet Beam|Tension", meta=(ClampMin="1", ClampMax="4"))
	float MaximumTensionGain = 1.8f;


	/** Ball shell pulse. Same colour and same phase as the beam, because synchronised
	 *  blinking is what ties the ball to one specific vehicle at camera distance. */
	UPROPERTY(EditDefaultsOnly, Category="Magnet Beam|Shell")
	bool bPulseBallShell = true;
	UPROPERTY(EditDefaultsOnly, Category="Magnet Beam|Shell", meta=(ClampMin="0", ClampMax="20"))
	float ShellPulseHz = 1.7f;
	UPROPERTY(EditDefaultsOnly, Category="Magnet Beam|Shell", meta=(ClampMin="0", ClampMax="1"))
	float ShellPulseFloor = 0.45f;
	/** Brightness changes per second, so the pulse steps like the other doodle layers. Zero is a smooth sine. */
	UPROPERTY(EditDefaultsOnly, Category="Magnet Beam|Shell", meta=(ClampMin="0", ClampMax="30"))
	float ShellPulseStepsPerSecond = 8.f;

private:
	void EnsureBeam();
	void StopBeam();
	void BeginPhase(EHALMagnetBeamPhase Next);
	/** Single source of the look: one rebuild, then the masks and scalars for this frame. */
	void PushBeam(float PhaseAlpha, bool bRetractFromBall);
	void PushShell(float PhaseAlpha);
	/** Also reports the length the pair would have at rest, which is what turns a raw
	 *  distance into readable strain regardless of vehicle or ball size. */
	bool ResolveAnchors(FVector& OutRoof, FVector& OutBall, float& OutRestLengthCm) const;
	FLinearColor ResolveHolderColor(const AActor* Holder) const;

	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> BeamComponent;
	TWeakObjectPtr<ABasicBallActor> Ball;
	TWeakObjectPtr<UHALBallPresentationComponent> BallPresentation;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> ShellInstances;
	TWeakObjectPtr<AActor> BeamHolder;
	EHALMagnetBeamPhase Phase = EHALMagnetBeamPhase::Idle;
	EBasicBallState LastSeenState = EBasicBallState::Free;
	FLinearColor ResolvedColor = FLinearColor::White;
	float PhaseSeconds = 0.f;
	float HelixPhase = 0.f;
	float HeldSeconds = 0.f;
	/** Reused between rebuilds so the per-frame refresh does not reallocate. */
	TArray<FVector> BeamPositions;
	TArray<float> BeamWidths;
	/** Widths as built. BeamWidths is this with the retraction mask applied. */
	TArray<float> BeamBaseWidths;
	bool bSynced = false;
	bool bRibbonSuppressed = false;
};
