#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HALBallComicFXSettings.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class UStaticMesh;

/** Doodle style ball readability layers: the ink trail of a launched ball, and the aim arrow and ownership rings of a held one.
 * Presentation only. Nothing here gates acquisition, launch direction, damage or the authority result. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALBallComicFXConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Ball Comic FX") bool bEnabled = true;

	/** Ink trail behind a Launched ball, painted in the launcher's colour. Free and Controlled balls never trail. */
	UPROPERTY(EditAnywhere, Category="Ink Trail") bool bTrail = true;
	/** Triangle trail: unlit translucent material drawn on Trail Wedge Mesh, a level quad behind the ball stretched along its
	 * travel. None keeps the triangle off. */
	UPROPERTY(EditAnywhere, Category="Ink Trail") TSoftObjectPtr<UMaterialInterface> TrailWedgeMaterial;
	/** 100 cm quad in its local XY plane, U along X and V along Y (the engine Plane). */
	UPROPERTY(EditAnywhere, Category="Ink Trail") TSoftObjectPtr<UStaticMesh> TrailWedgeMesh;
	/** Triangle length is the distance the ball covers in this time, so a faster shot draws a longer triangle. */
	UPROPERTY(EditAnywhere, Category="Ink Trail", meta=(ClampMin="0.01", ClampMax="1", Units="s")) float TrailWedgeLengthSeconds = .3f;
	/** Shortest triangle, in diameters of the ball as drawn: a slow shot still shows which way it is going. */
	UPROPERTY(EditAnywhere, Category="Ink Trail", meta=(ClampMin="0", ClampMax="10")) float TrailWedgeMinLengthDiameters = .8f;
	/** Longest triangle, in diameters of the ball as drawn. */
	UPROPERTY(EditAnywhere, Category="Ink Trail", meta=(ClampMin="0.2", ClampMax="10")) float TrailWedgeMaxLengthDiameters = 2.f;
	/** Below this ground speed the triangle is hidden: a crawling ball has no direction worth drawing. */
	UPROPERTY(EditAnywhere, Category="Ink Trail", meta=(ClampMin="0", ClampMax="5000", Units="cm/s")) float TrailWedgeMinSpeed = 150;
	/** Where the base starts behind the ball's centre, as a share of the drawn radius. Below 1 the ball covers the base
	 * (first PIE pass at 0.6 left only the tip showing). */
	UPROPERTY(EditAnywhere, Category="Ink Trail", meta=(ClampMin="0", ClampMax="2")) float TrailWedgeStartRadiusFraction = .9f;
	/** Vector parameter that receives the launcher's colour. */
	UPROPERTY(EditAnywhere, Category="Ink Trail") FName TrailWedgeColorParameter = TEXT("FX_Color");
	/** Scalar parameter that receives quad length over width, so the ink keeps one thickness at any length. */
	UPROPERTY(EditAnywhere, Category="Ink Trail") FName TrailWedgeAspectParameter = TEXT("FX_Aspect");
	/** Alternative particle trail (the pulse rings). None keeps it off; it can run together with the triangle. */
	UPROPERTY(EditAnywhere, Category="Ink Trail") TSoftObjectPtr<UNiagaraSystem> TrailSystem;
	/** LinearColor User parameter that receives the launcher's colour. */
	UPROPERTY(EditAnywhere, Category="Ink Trail") FName TrailColorParameter = TEXT("User.TrailColor");
	/** Float User parameter that receives the ribbon width in cm. */
	UPROPERTY(EditAnywhere, Category="Ink Trail") FName TrailWidthParameter = TEXT("User.TrailWidth");
	/** Trail width at the ball (triangle base, particle width) as a share of the diameter of the ball as drawn (its visual mesh, never smaller than the physics sphere). */
	UPROPERTY(EditAnywhere, Category="Ink Trail", meta=(ClampMin="0.1", ClampMax="3")) float TrailWidthDiameterFraction = .75f;
	/** True removes the whole trail on the frame the ball stops being dangerous; false lets the tail play out. */
	UPROPERTY(EditAnywhere, Category="Ink Trail") bool bCutTrailImmediately = true;

	/** Hand drawn arrow on the ground ahead of the car that holds a ball, along the real launch direction. */
	UPROPERTY(EditAnywhere, Category="Aim Arrow") bool bAim = true;
	/** Unlit translucent material drawn on Aim Mesh. None keeps the arrow off. */
	UPROPERTY(EditAnywhere, Category="Aim Arrow") TSoftObjectPtr<UMaterialInterface> AimMaterial;
	/** Flat 100 x 100 cm quad in the XY plane; the arrow points along its +X. */
	UPROPERTY(EditAnywhere, Category="Aim Arrow") TSoftObjectPtr<UStaticMesh> AimMesh;
	/** Arrow length as a share of the holder body's width, so it reads the same on every car.
	 * Short on purpose: it shows the direction, it must never suggest a range. */
	UPROPERTY(EditAnywhere, Category="Aim Arrow", meta=(ClampMin="0.1", ClampMax="4")) float AimLengthWidthFraction = .575f;
	UPROPERTY(EditAnywhere, Category="Aim Arrow", meta=(ClampMin="0.1", ClampMax="4")) float AimWidthWidthFraction = .5f;
	/** Clear ground between the held ball as drawn and the arrow tail, as a share of the holder's width,
	 * so the arrow never sits under the ball. */
	UPROPERTY(EditAnywhere, Category="Aim Arrow", meta=(ClampMin="0", ClampMax="2")) float AimGapWidthFraction = .3f;
	/** Lift above the holder's underside, enough to stay clear of the floor. */
	UPROPERTY(EditAnywhere, Category="Aim Arrow", meta=(ClampMin="0", ClampMax="100", Units="cm")) float AimGroundLiftCm = 4;
	/** Pop-in when the ball is captured. */
	UPROPERTY(EditAnywhere, Category="Aim Arrow", meta=(ClampMin="0", ClampMax="1", Units="s")) float AimPopSeconds = .16f;
	UPROPERTY(EditAnywhere, Category="Aim Arrow", meta=(ClampMin="1", ClampMax="2")) float AimPopOvershoot = 1.25f;
	UPROPERTY(EditAnywhere, Category="Aim Arrow") FName AimColorParameter = TEXT("FX_Color");

	/** Ownership marks of a held ball in the holder's colour: orbit rings on the ball and a marker floating over the holder. */
	UPROPERTY(EditAnywhere, Category="Hold Rings") bool bHoldRings = true;
	/** Unlit translucent material for the camera facing sprite centred on the held ball. None keeps the ball rings off. */
	UPROPERTY(EditAnywhere, Category="Hold Rings") TSoftObjectPtr<UMaterialInterface> HoldBallMaterial;
	/** Unlit translucent material drawn on Hold Roof Mesh above the holder. None keeps the holder marker off. */
	UPROPERTY(EditAnywhere, Category="Hold Rings") TSoftObjectPtr<UMaterialInterface> HoldRoofMaterial;
	/** Flat 100 x 100 cm quad in the XY plane. */
	UPROPERTY(EditAnywhere, Category="Hold Rings") TSoftObjectPtr<UStaticMesh> HoldRoofMesh;
	/** True hides the magnet beam arc while the rings are available, so ownership is drawn once. The ball shell pulse stays. */
	UPROPERTY(EditAnywhere, Category="Hold Rings") bool bHoldReplacesBeam = true;
	/** Ball sprite width as a share of the diameter of the ball as drawn. Must leave room for the rings outside the ball. */
	UPROPERTY(EditAnywhere, Category="Hold Rings", meta=(ClampMin="1.1", ClampMax="4")) float HoldBallSizeDiameterFraction = 1.7f;
	/** Holder marker quad size as a share of the holder body's width, so it reads the same on every car. */
	UPROPERTY(EditAnywhere, Category="Hold Rings", meta=(ClampMin="0.1", ClampMax="4")) float HoldRoofSizeWidthFraction = 1.1f;
	/** Height of the marker above the top of the holder's bounds, as a share of the body's width. Kept low: under a perspective
	 * camera anything lifted slides away from the screen centre, which would carry the marker off the rear half. */
	UPROPERTY(EditAnywhere, Category="Hold Rings", meta=(ClampMin="0", ClampMax="3")) float HoldMarkerLiftWidthFraction = .1f;
	/** How far behind the middle of the car the marker floats, as a share of the body's length. 0.5 is the rear bumper;
	 * 0 puts it straight overhead, where its arrow points down the screen. */
	UPROPERTY(EditAnywhere, Category="Hold Rings", meta=(ClampMin="0", ClampMax="1.5")) float HoldMarkerBackLengthFraction = .3f;
	/** Pop-in when the ball is captured. */
	UPROPERTY(EditAnywhere, Category="Hold Rings", meta=(ClampMin="0", ClampMax="1", Units="s")) float HoldPopSeconds = .16f;
	UPROPERTY(EditAnywhere, Category="Hold Rings", meta=(ClampMin="1", ClampMax="2")) float HoldPopOvershoot = 1.25f;
	/** The marks shrink to nothing over this time when the ball is launched or lost; they never fade. Zero cuts at once. */
	UPROPERTY(EditAnywhere, Category="Hold Rings", meta=(ClampMin="0", ClampMax="1", Units="s")) float HoldShrinkSeconds = .12f;
	UPROPERTY(EditAnywhere, Category="Hold Rings") FName HoldColorParameter = TEXT("FX_Color");
	/** Scalar that receives the drawn ball radius in sprite half extents; the ball material hides the far half of each ring inside it. */
	UPROPERTY(EditAnywhere, Category="Hold Rings") FName HoldBallRadiusParameter = TEXT("FX_BallRadius");

	/** Local-only screen edge arrow for a launched ball that is off screen and heading for this player's car. */
	UPROPERTY(EditAnywhere, Category="Incoming Warning") bool bIncomingWarning = true;
	/** Only warn when the closest approach is at most this far ahead. */
	UPROPERTY(EditAnywhere, Category="Incoming Warning", meta=(ClampMin="0.1", ClampMax="5", Units="s")) float IncomingMaximumSeconds = 1.5f;
	/** Horizontal closest approach that still counts as heading for the car. */
	UPROPERTY(EditAnywhere, Category="Incoming Warning", meta=(ClampMin="50", ClampMax="2000", Units="cm")) float IncomingMissDistanceCm = 300;
	/** Slower balls are never announced. */
	UPROPERTY(EditAnywhere, Category="Incoming Warning", meta=(ClampMin="0", ClampMax="10000", Units="cm/s")) float IncomingMinimumSpeed = 300;
	/** Arrow length as a share of the viewport height. */
	UPROPERTY(EditAnywhere, Category="Incoming Warning", meta=(ClampMin="0.01", ClampMax="0.3")) float IncomingArrowScreenFraction = .07f;
	/** Distance from the screen edge as a share of the viewport. */
	UPROPERTY(EditAnywhere, Category="Incoming Warning", meta=(ClampMin="0", ClampMax="0.4")) float IncomingEdgeInsetFraction = .06f;
	/** The entry condition must hold this long before the arrow appears. */
	UPROPERTY(EditAnywhere, Category="Incoming Warning", meta=(ClampMin="0", ClampMax="1", Units="s")) float IncomingConfirmSeconds = .12f;
	/** Once shown, an arrow stays at least this long. */
	UPROPERTY(EditAnywhere, Category="Incoming Warning", meta=(ClampMin="0", ClampMax="2", Units="s")) float IncomingMinimumShowSeconds = .3f;
	UPROPERTY(EditAnywhere, Category="Incoming Warning", meta=(ClampMin="1", ClampMax="4")) int32 IncomingMaximumArrows = 2;
	/** Pop-in time of a newly shown arrow. */
	UPROPERTY(EditAnywhere, Category="Incoming Warning", meta=(ClampMin="0", ClampMax="1", Units="s")) float IncomingPopSeconds = .1f;
	/** Pose changes per second of the hand drawn wobble; zero holds the arrow still. */
	UPROPERTY(EditAnywhere, Category="Incoming Warning", meta=(ClampMin="0", ClampMax="24")) float IncomingBoilFps = 8;
	/** Swing of the wobble either side of the true direction. */
	UPROPERTY(EditAnywhere, Category="Incoming Warning", meta=(ClampMin="0", ClampMax="20", Units="deg")) float IncomingWobbleDegrees = 4;
	/** Viewport layer of the overlay; above the match HUD, below menus. */
	UPROPERTY(EditAnywhere, Category="Incoming Warning", meta=(ClampMin="0", ClampMax="100")) int32 IncomingZOrder = 12;
	bool Validate(FString& Error) const;
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Ball Comic FX"))
class HAL_FUTURE_CREATION_API UHALBallComicFXSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(Config, EditAnywhere, Category="Ball Comic FX") FHALBallComicFXConfig Comic;
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
#if WITH_EDITOR
	/** Cook-only synchronous validation; roots the soft referenced trail system and arrow material. */
	bool CollectCookPackages(TArray<FName>& Packages, FString& Error) const;
#endif
};
