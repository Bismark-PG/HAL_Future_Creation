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
class UTextRenderComponent;
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

/** A piece of trail no longer attached to the ball. */
struct FHALBallTrailGhost
{
	/** Old leg of a bounce: world point of the bounce and the unit direction back along the old leg. */
	FVector Point = FVector::ZeroVector;
	FVector Back = FVector::ZeroVector;
	/** Path distance from the latest bounce point back to this leg's start (legs between them). */
	double PathStartCm = 0;
	/** Dissolve clock; a duration of zero means no dissolve is running. */
	float Age = 0;
	float Duration = 0;
	/** True while the piece is a leg of the live trail's path, placed every frame; false while it dissolves or is free. */
	bool bPath = false;
	bool IsFree() const { return !bPath && Duration <= 0; }
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
	/** Camera facing quad round the held ball whose long axis is the holder's right as seen on screen; the hidden half stays up-screen. */
	HAL_FUTURE_CREATION_API bool HoldBallQuad(const FVector& Centre, float HalfSizeCm, const FVector& HolderRight, const FVector& ViewForward,
		const FVector& ViewUp, FTransform& OutQuad);
	/** Holder marker quad: camera facing, floating over the rear half of the car (world up, so it stays overhead while the car
	 * rolls), its +V pointing at the middle of the roof as seen on screen. Sized from the car's own width. False on unusable input. */
	HAL_FUTURE_CREATION_API bool HoldMarkerQuad(const FHALBallComicFXConfig& Config, const FVector& RoofCentre, const FVector& HolderForward,
		float BodyLengthCm, float BodyWidthCm, const FVector& ViewForward, const FVector& ViewUp, float Scale, FTransform& OutQuad);
	/** Triangle trail quad: level, behind the ball as drawn, its +Y (the tip) pointing back along the ground velocity. Length follows
	 * the speed up to a cap; OutAspect is length over width for the material. With a lead the quad also reaches ahead of the
	 * ball. OutBall, when given, is the ball on the quad in half widths (X centre from the front edge, Y radius).
	 * A LengthLimitCm of zero or more caps the trail behind the ball (how far it has travelled since launch or a bounce).
	 * A LengthOverrideCm of zero or more replaces the length the speed would give (the component blends it over time).
	 * False when the ball is too slow or the input unusable. */
	HAL_FUTURE_CREATION_API bool TrailWedgeQuad(const FHALBallComicFXConfig& Config, const FVector& Centre, float DrawnRadiusCm, const FVector& Velocity,
		FTransform& OutQuad, float& OutAspect, FVector2f* OutBall = nullptr, double LengthLimitCm = -1., double LengthOverrideCm = -1.);
	/** Whole length of the trail behind a ball at this speed (the clamp TrailWedgeQuad applies). Negative when the ball is too slow or the input unusable. */
	HAL_FUTURE_CREATION_API double TrailLength(const FHALBallComicFXConfig& Config, float DrawnRadiusCm, const FVector& Velocity);
	/** Quad of an old leg of a bounced trail: starts at the bounce Point, runs along BackDirection (unit), and shows the trail
	 * from StartCm behind the ball (along the path) to its end at TotalLengthCm. OutPath is (start, whole length) in half widths
	 * for the material. False once the ball has travelled the leg away (nothing left) or the input is unusable. */
	HAL_FUTURE_CREATION_API bool TrailPathSegmentQuad(const FHALBallComicFXConfig& Config, const FVector& Point, const FVector& BackDirection, float DrawnRadiusCm,
		double StartCm, double TotalLengthCm, FTransform& OutQuad, float& OutAspect, FVector2f& OutPath);
	/** True when a ball that was travelling along PreviousDirection (unit, on the ground) now travels more than the configured
	 * angle away from it: it bounced, and the trail it had belongs to the old path. */
	HAL_FUTURE_CREATION_API bool IsTrailBounce(const FHALBallComicFXConfig& Config, const FVector& PreviousDirection, const FVector& Velocity);
	/** Dissolve value of something left behind: 1 when just left, 0 once Duration has passed. */
	HAL_FUTURE_CREATION_API float TrailFade(float AgeSeconds, float DurationSeconds);
	/** Flame shell pose: an ellipsoid (unit mesh: 100 cm sphere) whose +X is the ground travel, reaching the configured radii
	 * ahead of, behind and across the ball as drawn. False when the ball is too slow (same gate as the trail quad) or the input unusable. */
	HAL_FUTURE_CREATION_API bool TrailShellTransform(const FHALBallComicFXConfig& Config, const FVector& Centre, float DrawnRadiusCm, const FVector& Velocity,
		FTransform& OutShell);
	/** Half size in cm of the camera facing ring sprite for a ball drawn with this radius. */
	HAL_FUTURE_CREATION_API float HoldBallHalfSize(const FHALBallComicFXConfig& Config, float DrawnRadiusCm, float Scale);
}

/**
 * Ball-side doodle readability layers, driven only by the ball's replicated state:
 *   Launched    an ink ribbon in the launcher's colour marks the ball as dangerous and says whose it is;
 *   Controlled  a hand drawn arrow on the ground ahead of the holder shows where the shot will go, and orbit rings
 *               on the ball plus a marker floating over the holder, both in the holder's colour, say whose ball it is.
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
	/** Called from charge RepNotify and authority commit, independently of ball ownership/state. */
	void SyncElectricCharge();
	bool IsTrailActive() const { return bTrailActive; }
	bool IsAimActive() const { return bAimActive; }
	EHALBallHoldRingPhase GetHoldRingPhase() const { return HoldPhase; }
private:
	const FHALBallComicFXConfig* GetComicConfig() const;
	FLinearColor ResolveColor(const AActor* Pawn, int32 PlayerId) const;
	void StartTrail(const FHALBallComicFXConfig& ComicConfig, const FBallRepState& State);
	void StopTrail(bool bImmediate);
	void UpdateTrailWedge(const FHALBallComicFXConfig& ComicConfig, float DeltaTime = 0);
	/** Leaves a copy of the live trail quad where it is, dissolving over Seconds. */
	void SpawnTrailGhost(const FHALBallComicFXConfig& ComicConfig, float Seconds);
	/** A free piece, a new one while there is room, else the oldest. INDEX_NONE when none can be made. */
	int32 AcquireTrailGhost(const FHALBallComicFXConfig& ComicConfig);
	/** Old legs stop following the path: they dissolve over Seconds where they are, or vanish at once for zero. */
	void ReleaseTrailPath(float Seconds);
	void UpdateTrailPath(const FHALBallComicFXConfig& ComicConfig, float DrawnRadius, double PathRunCm);
	/** Freezes the flame shell where it is and dissolves it over Seconds. */
	void BeginShellFade(const FHALBallComicFXConfig& ComicConfig, float Seconds);
	void UpdateTrailFades(const FHALBallComicFXConfig& ComicConfig, float DeltaTime);
	/** Shared setup of the trail's world placed meshes (quad, shell): never follows the spinning ball, never collides. */
	UStaticMeshComponent* CreateTrailMesh(USceneComponent* Root, UStaticMesh* Mesh);
	void StartAim(const FHALBallComicFXConfig& ComicConfig, const FBallRepState& State);
	void StopAim();
	void UpdateAim(const FHALBallComicFXConfig& ComicConfig);
	void StartHold(const FHALBallComicFXConfig& ComicConfig, const FBallRepState& State);
	/** Immediate hides at once (a steal restarts on the new holder, teardown); otherwise the marks shrink away. */
	void StopHold(const FHALBallComicFXConfig* ComicConfig, bool bImmediate);
	void UpdateHold(const FHALBallComicFXConfig& ComicConfig);
	void HideHold();
	/** Ticks only while an arrow, a ring or the triangle trail is on screen. */
	void RefreshTick();
	void UpdateElectricChargeHint(const FHALBallComicFXConfig& Config);
	/** Centre and radius of the ball as drawn this frame (the client shows a held ball from a relative sample). */
	bool ResolveDrawnBall(FVector& OutCentre, float& OutRadius) const;
	TWeakObjectPtr<ABasicBallActor> Ball;
	TWeakObjectPtr<UHALMagnetBeamPresentationComponent> MagnetBeam;
	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> TrailSystem;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> AimMaterial;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> AimMesh;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> TrailComponent;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> TrailWedgeMaterial;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> TrailWedgeMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> TrailWedgeComponent;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> TrailWedgeInstance;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> TrailShellMaterial;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> TrailShellMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> TrailShellComponent;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> TrailShellInstance;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> TrailGhostComponents;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> TrailGhostInstances;
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
	/** Kept through Leaving, so the marker shrinks on the car that just lost the ball. */
	TWeakObjectPtr<AActor> HoldHolder;
	EHALBallHoldRingPhase HoldPhase = EHALBallHoldRingPhase::Off;
	float HoldSeconds = 0;
	/** Half size the ball sprite was last built with; the billboard is only rebuilt when it changes. */
	float AimSeconds = 0;
	UPROPERTY(Transient) TObjectPtr<UTextRenderComponent> ElectricChargeHint;
	bool bElectricChargeHintActive = false;
	bool bReady = false;
	bool bTrailActive = false;
	bool bTrailWedgeActive = false;
	/** One entry per ghost component. */
	TArray<FHALBallTrailGhost> TrailGhosts;
	FVector2f TrailQuadPath = FVector2f::ZeroVector;
	/** Whole trail length, blended toward what the speed gives. */
	double TrailLengthCm = 0;
	bool bTrailLengthValid = false;
	FLinearColor TrailColour = FLinearColor::White;
	/** Ground direction of the last frame, to tell a bounce. */
	FVector TrailDirection = FVector::ZeroVector;
	FVector2f TrailQuadBall = FVector2f::ZeroVector;
	/** Ground distance since launch or the last bounce: the live trail is never longer than the path it belongs to. */
	double TrailRunCm = 0;
	float TrailQuadAspect = 1;
	float ShellFadeAge = 0;
	float ShellFadeDuration = 0;
	bool bTrailHasDirection = false;
	bool bShellFading = false;
	bool bTrailGhosts = false;
	bool bAimActive = false;
};
