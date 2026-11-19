#pragma once

#include "CoreMinimal.h"
#include "BasicBallActor.h"
#include "Components/ActorComponent.h"
#include "HALBallComicFXComponent.generated.h"

class UHALMagnetBeamPresentationComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class UStaticMesh;
class UStaticMeshComponent;
struct FHALBallComicFXConfig;

/** What this component last drew for; compared against the next replicated snapshot. */
struct FHALBallComicFXSeen
{
	EBasicBallState State = EBasicBallState::Free;
	uint32 Sequence = 0;
	TWeakObjectPtr<const AActor> Holder;
	int32 LauncherPlayerId = INDEX_NONE;
	bool bValid = false;
};

struct FHALBallComicFXTransition
{
	bool bStartTrail = false;
	bool bStopTrail = false;
	/** The held layers share one condition (Controlled with a resolved holder): the aim arrow and the hold rings. */
	bool bStartAim = false;
	bool bStopAim = false;
};

/** Hold rings timeline: popping in and held, or shrinking away after the ball left the holder. */
enum class EHALBallHoldRingPhase : uint8 { Off, Shown, Leaving };

namespace HALBallComicFX
{
	/** Diffs by sequence and identity, not by state edge: one net update may merge Launched(A) -> Free -> Launched(B). */
	HAL_FUTURE_CREATION_API FHALBallComicFXTransition ResolveTransition(const FHALBallComicFXSeen& Previous, const FBallRepState& Next);
	/** Arrow quad pose on the ground ahead of the held ball. Uses the physics body's forward, flattened, because that
	 * is the launch direction; the leaning visual body is never read. False when the body points straight up or down. */
	HAL_FUTURE_CREATION_API bool AimAnchor(const FHALBallComicFXConfig& Config, const FTransform& HolderBody, const FVector& HolderExtent,
		const FVector& BallCentre, float BallRadiusCm, float PopScale, FTransform& OutQuad);
	/** Radius of the ball as the player sees it: the visual mesh may be larger than the physics sphere, never smaller. */
	HAL_FUTURE_CREATION_API float DrawnBallRadius(float PhysicsRadiusCm, const FVector& VisualBoundsExtent);
	/** Half size of a visual from its local box and scale only: a ball that has rolled must not measure bigger. */
	HAL_FUTURE_CREATION_API FVector UnrotatedExtent(const UPrimitiveComponent& Visual);
	/** 0 -> overshoot -> 1 over the pop time; exactly 1 afterwards. */
	HAL_FUTURE_CREATION_API float AimPopScale(const FHALBallComicFXConfig& Config, float AimSeconds);
	/** Size of the hold rings: the pop while Shown, a linear shrink to zero while Leaving (they shrink, never fade), zero when Off. */
	HAL_FUTURE_CREATION_API float HoldRingScale(const FHALBallComicFXConfig& Config, EHALBallHoldRingPhase Phase, float PhaseSeconds);
	/** Roof crest quad: level with the world, centred over the holder's world bounds, sized from the car's own width.
	 * World up rather than the car's up, so the crest stays overhead while the car rolls. False on unusable input. */
	/** Camera facing quad round the held ball whose long axis is the holder's right as seen on screen; the hidden half stays up-screen. */
	HAL_FUTURE_CREATION_API bool HoldBallQuad(const FVector& Centre, float HalfSizeCm, const FVector& HolderRight, const FVector& ViewForward,
		const FVector& ViewUp, FTransform& OutQuad);
	HAL_FUTURE_CREATION_API bool HoldRoofAnchor(const FHALBallComicFXConfig& Config, const FVector& BoundsOrigin, const FVector& BoundsExtent,
		float BodyWidthCm, float Scale, FTransform& OutQuad);
	/** Half size in cm of the camera facing ring sprite for a ball drawn with this radius. */
	HAL_FUTURE_CREATION_API float HoldBallHalfSize(const FHALBallComicFXConfig& Config, float DrawnRadiusCm, float Scale);
}

/**
 * Ball-side doodle readability layers, driven only by the ball's replicated state:
 *   Launched    an ink ribbon in the launcher's colour marks the ball as dangerous and says whose it is;
 *   Controlled  a hand drawn arrow on the ground ahead of the holder shows where the shot will go, and orbit rings
 *               on the ball plus a crest over the holder's roof, both in the holder's colour, say whose ball it is.
 * Called from the authority state commit and from the client OnRep, like the magnet beam, because a
 * listen-server host never runs the OnRep. Never reads or writes HP, physics, acquisition or launch permission.
 */
UCLASS(ClassGroup=(Ball))
class HAL_FUTURE_CREATION_API UHALBallComicFXComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALBallComicFXComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	void SyncToBallState(const FBallRepState& State);
	bool IsTrailActive() const { return bTrailActive; }
	bool IsAimActive() const { return bAimActive; }
	EHALBallHoldRingPhase GetHoldRingPhase() const { return HoldPhase; }
private:
	const FHALBallComicFXConfig* GetComicConfig() const;
	FLinearColor ResolveColor(const AActor* Pawn, int32 PlayerId) const;
	void StartTrail(const FHALBallComicFXConfig& ComicConfig, const FBallRepState& State);
	void StopTrail(bool bImmediate);
	void StartAim(const FHALBallComicFXConfig& ComicConfig, const FBallRepState& State);
	void StopAim();
	void UpdateAim(const FHALBallComicFXConfig& ComicConfig);
	void StartHold(const FHALBallComicFXConfig& ComicConfig, const FBallRepState& State);
	/** Immediate hides at once (a steal restarts on the new holder, teardown); otherwise the marks shrink away. */
	void StopHold(const FHALBallComicFXConfig* ComicConfig, bool bImmediate);
	void UpdateHold(const FHALBallComicFXConfig& ComicConfig);
	void HideHold();
	/** Ticks only while an arrow or a ring is on screen. */
	void RefreshTick();
	/** Centre and radius of the ball as drawn this frame (the client shows a held ball from a relative sample). */
	bool ResolveDrawnBall(FVector& OutCentre, float& OutRadius) const;
	TWeakObjectPtr<ABasicBallActor> Ball;
	TWeakObjectPtr<UHALMagnetBeamPresentationComponent> MagnetBeam;
	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> TrailSystem;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> AimMaterial;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> AimMesh;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> TrailComponent;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> AimComponent;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> AimInstance;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> HoldBallMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> HoldRoofMaterial;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> HoldRoofMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> HoldBallComponent;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> HoldRoofComponent;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> HoldBallInstance;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> HoldRoofInstance;
	FHALBallComicFXSeen Seen;
	TWeakObjectPtr<AActor> AimHolder;
	/** Kept through Leaving, so the crest shrinks on the car that just lost the ball. */
	TWeakObjectPtr<AActor> HoldHolder;
	EHALBallHoldRingPhase HoldPhase = EHALBallHoldRingPhase::Off;
	float HoldSeconds = 0;
	/** Half size the ball sprite was last built with; the billboard is only rebuilt when it changes. */
	float AimSeconds = 0;
	bool bReady = false;
	bool bTrailActive = false;
	bool bAimActive = false;
};
