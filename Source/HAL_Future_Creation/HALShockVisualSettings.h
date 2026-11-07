#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "HALShockArcGeometry.h"
#include "HALShockVisualSettings.generated.h"

class UNiagaraSystem;

/** Shared settings for the existing electric flash, ribbon and scorch driver. */
USTRUCT()
struct HAL_FUTURE_CREATION_API FHALShockVisualSettings
{
	GENERATED_BODY()

	FHALShockVisualSettings();
	bool Validate(FString& Error) const;

	/** Cosmetic arc looped while the shocked tag is present. Never reads or writes HP, physics or ball ownership. */
	UPROPERTY(EditAnywhere, Category="Arc FX") TObjectPtr<UNiagaraSystem> ArcSystem;
	/** Offset from the physics body, in vehicle local axes. The arc tracks the authoritative pose, not the visual mesh. */
	UPROPERTY(EditAnywhere, Category="Arc FX", meta=(Units="cm")) FVector ArcOffsetCm = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category="Arc FX", meta=(ClampMin="0.01", ClampMax="20")) float ArcScale = 1;

	/** Whole-body punch at the start; this is what makes the hit readable from the top-down camera. */
	UPROPERTY(EditAnywhere, Category="Overlay", meta=(Units="s", ClampMin="0", ClampMax="0.5")) float FlashSeconds = 0.06f;
	/** How long the discharge takes to spread from the origin across the body. */
	UPROPERTY(EditAnywhere, Category="Overlay", meta=(Units="s", ClampMin="0", ClampMax="1")) float WipeSeconds = 0.12f;
	/** Final wipe radius; needs to exceed the vehicle's own extent so the body ends fully covered. */
	UPROPERTY(EditAnywhere, Category="Overlay", meta=(Units="cm", ClampMin="10", ClampMax="2000")) float WipeRadiusCm = 400.f;
	UPROPERTY(EditAnywhere, Category="Overlay", meta=(ClampMin="0", ClampMax="60")) float BaseBrightness = 8.f;
	/** Added to the filament exponent across the fade, so the lines thin out instead of only dimming. */
	UPROPERTY(EditAnywhere, Category="Overlay", meta=(ClampMin="0", ClampMax="120")) float FadeFilamentGain = 40.f;
	/** Steps per second of the jump clock. Pushed to the material so filaments and light pulse together. */
	UPROPERTY(EditAnywhere, Category="Overlay", meta=(ClampMin="1", ClampMax="60")) float FlickerSteps = 14.f;
	/** Brightness multiplier against seconds held. Values past the last key hold, so long stuns stay stable. */
	UPROPERTY(EditAnywhere, Category="Overlay") FRuntimeFloatCurve BrightnessCurve;
	/** Filament exponent against seconds held; rising values thin the lines out as the charge dissipates. */
	UPROPERTY(EditAnywhere, Category="Overlay") FRuntimeFloatCurve FilamentPowerCurve;

	/**
	 * Ribbon renderer for the hero arcs. The asset owns no look: it spawns one particle per
	 * point and reads Positions / Widths, which this component rebuilds on the flicker clock.
	 */
	UPROPERTY(EditAnywhere, Category="Hero Arcs") TObjectPtr<UNiagaraSystem> RibbonSystem;
	UPROPERTY(EditAnywhere, Category="Hero Arcs") FHALShockArcSettings ArcSettings;
	/**
	 * Arc tint. The ribbon material keeps a thin white-hot spine whatever this is, so the
	 * colour is read from the halo around it: push saturation here, not brightness.
	 */
	UPROPERTY(EditAnywhere, Category="Hero Arcs") FLinearColor ArcColor = FLinearColor(0.22f, 0.52f, 1.f, 1.f);
	/**
	 * On release the arc withdraws from its tail instead of dimming as a whole, which reads
	 * as the current draining away rather than someone turning a dimmer down.
	 */
	UPROPERTY(EditAnywhere, Category="Hero Arcs") bool bRetractArcsOnRelease = true;
	/** Softness of the retracting front, as a fraction of each ribbon's length. */
	UPROPERTY(EditAnywhere, Category="Hero Arcs", meta=(ClampMin="0.01", ClampMax="1")) float ArcRetractFeather = 0.25f;

	/** How fast the soot builds up while the current is on the body. */
	UPROPERTY(EditAnywhere, Category="Scorch", meta=(Units="s", ClampMin="0", ClampMax="3")) float ScorchRiseSeconds = 0.25f;

	/** Off by default: the pool of light it throws on the arena floor costs more readability than it buys. */
	UPROPERTY(EditAnywhere, Category="Light") bool bEnableLight = false;
	UPROPERTY(EditAnywhere, Category="Light") FLinearColor LightColor = FLinearColor(0.55f, 0.88f, 1.f, 1.f);
	UPROPERTY(EditAnywhere, Category="Light", meta=(Units="Candela", ClampMin="0", ClampMax="200000")) float LightPeakIntensity = 2200.f;
	UPROPERTY(EditAnywhere, Category="Light", meta=(Units="cm", ClampMin="50", ClampMax="5000")) float LightRadiusCm = 280.f;
	UPROPERTY(EditAnywhere, Category="Light", meta=(Units="cm")) FVector LightOffsetCm = FVector(0.f, 0.f, 40.f);
	/** Lower bound of the flicker, so the light never drops fully out between steps. */
	UPROPERTY(EditAnywhere, Category="Light", meta=(ClampMin="0", ClampMax="1")) float LightFlickerFloor = 0.6f;
	/** Extra punch while the whole-body flash is up. */
	UPROPERTY(EditAnywhere, Category="Light", meta=(ClampMin="0", ClampMax="10")) float LightFlashBoost = 0.6f;
};
