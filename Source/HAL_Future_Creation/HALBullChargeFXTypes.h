#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "HALVehicleFXMount.h"
#include "HALBullChargeFXTypes.generated.h"

class UNiagaraSystem;
class UHALBullChargeConfig;

/** Skill-owned art recipe. These settings never participate in driving, damage or impact calculations. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALBullChargeFXConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Rendering") bool bEnabled = false;
	UPROPERTY(EditAnywhere, Category="Front Wind") TObjectPtr<UNiagaraSystem> FrontWind;
	UPROPERTY(EditAnywhere, Category="Front Wind") FHALVehicleFXMount FrontMount;
	UPROPERTY(EditAnywhere, Category="Front Wind|Legacy Particle Wind", meta=(ClampMin="0", ClampMax="100", ToolTip="Legacy short-lived wind only; continuous pressure wind uses persistent geometry.")) float WindSpawnRate = 14;
	UPROPERTY(EditAnywhere, Category="Front Wind|Legacy Particle Wind", meta=(ClampMin="0.08", ClampMax="1", Units="s")) float WindLifetime = .32f;
	UPROPERTY(EditAnywhere, Category="Front Wind", meta=(ClampMin="0.25", ClampMax="4")) float WindSizeScale = 1;
	/** Extra local-X stretch about the common nose apex. Width, height and turbulence stay unchanged. */
	UPROPERTY(EditAnywhere, Category="Front Wind", meta=(ClampMin="0.1", ClampMax="4", ToolTip="Length multiplier on top of Wind Size Scale. 1 preserves the existing length; width, height and pressure shell are unchanged.")) float WindLengthScale = 1;
	UPROPERTY(EditAnywhere, Category="Front Wind", meta=(ClampMin="0", ClampMax="1")) float WindOpacity = .48f;
	UPROPERTY(EditAnywhere, Category="Front Wind", meta=(ClampMin="1", ClampMax="12000", Units="cm/s")) float WindFullSpeed = 3000;
	/** Continuous material flow cycles per second, independent of particle spawn rate. */
	UPROPERTY(EditAnywhere, Category="Front Wind", meta=(ClampMin="0", ClampMax="8")) float WindFlowSpeed = 1.8f;
	UPROPERTY(EditAnywhere, Category="Front Wind", meta=(ClampMin="0", ClampMax="0.5", Units="s")) float WindResponseSeconds = .1f;
	UPROPERTY(EditAnywhere, Category="Front Wind", meta=(ClampMin="0", ClampMax="30", Units="cm")) float WindTurbulence = 8;
	/** Airflow contour only. The pressure shell uses its own whole-surface soft three-tone shading. */
	UPROPERTY(EditAnywhere, Category="Front Wind|Contour") FLinearColor WindContourColor = FLinearColor(.14f, .16f, .17f);
	UPROPERTY(EditAnywhere, Category="Front Wind|Contour", meta=(ClampMin="0", ClampMax="0.3")) float WindContourWidth = .12f;
	/** Ellipsoidal bow-wave half axes in cm: forward depth, half width, half height. */
	UPROPERTY(EditAnywhere, Category="Pressure Shell", meta=(ClampMin="10", ClampMax="500", Units="cm")) FVector PressureSize = FVector(100, 145, 110);
	UPROPERTY(EditAnywhere, Category="Pressure Shell", meta=(ClampMin="0", ClampMax="1")) float PressureOpacity = .42f;
	UPROPERTY(EditAnywhere, Category="Pressure Shell", meta=(ClampMin="0.05", ClampMax="0.6")) float PressureFeather = .28f;
	UPROPERTY(EditAnywhere, Category="Pressure Shell", meta=(ClampMin="0", ClampMax="10", Units="cm")) float PressureTurbulence = 2.5f;
	UPROPERTY(EditAnywhere, Category="Pressure Shell|Toon Shading", meta=(HideAlphaChannel)) FLinearColor PressureShadowColor = FLinearColor(.52f, .56f, .60f);
	UPROPERTY(EditAnywhere, Category="Pressure Shell|Toon Shading", meta=(HideAlphaChannel)) FLinearColor PressureMidColor = FLinearColor(.70f, .74f, .77f);
	UPROPERTY(EditAnywhere, Category="Pressure Shell|Toon Shading", meta=(HideAlphaChannel)) FLinearColor PressureLightColor = FLinearColor(.88f, .91f, .94f);
	/** Normal-based tone coordinate in [0,1]; must be below Pressure Light Threshold. */
	UPROPERTY(EditAnywhere, Category="Pressure Shell|Toon Shading", meta=(ClampMin="0", ClampMax="1")) float PressureShadowThreshold = .40f;
	UPROPERTY(EditAnywhere, Category="Pressure Shell|Toon Shading", meta=(ClampMin="0", ClampMax="1")) float PressureLightThreshold = .75f;
	/** Full width of each soft color transition; capped to 90% of threshold separation to retain a middle band. */
	UPROPERTY(EditAnywhere, Category="Pressure Shell|Toon Shading", meta=(ClampMin="0.02", ClampMax="0.4")) float PressureToneSoftness = .18f;
	UPROPERTY(EditAnywhere, Category="Rear Sparks") TObjectPtr<UNiagaraSystem> RearSparks;
	/** Up to four emitters, e.g. left and right exhaust sockets. */
	UPROPERTY(EditAnywhere, Category="Rear Sparks", meta=(TitleProperty="SocketOrBone")) TArray<FHALVehicleFXMount> RearMounts;
	UPROPERTY(EditAnywhere, Category="Rear Sparks", meta=(ClampMin="0", ClampMax="300", ToolTip="Particles per second per rear mount at full drive strength.")) float RearSpawnRate = 65;
	UPROPERTY(EditAnywhere, Category="Rear Sparks", meta=(ClampMin="0.08", ClampMax="1.2", Units="s")) float RearLifetime = .36f;
	UPROPERTY(EditAnywhere, Category="Rear Sparks", meta=(ClampMin="100", ClampMax="4000", Units="cm/s")) float RearSpeed = 1100;
	UPROPERTY(EditAnywhere, Category="Rear Sparks", meta=(ClampMin="1", ClampMax="40", Units="deg")) float RearSpreadDegrees = 18;
	UPROPERTY(EditAnywhere, Category="Rear Sparks", meta=(ClampMin="1", ClampMax="100", Units="cm")) FVector2D RearSize = FVector2D(8, 24);
	UPROPERTY(EditAnywhere, Category="Contact") TObjectPtr<UNiagaraSystem> ContactImpact;
	UPROPERTY(EditAnywhere, Category="Contact", meta=(ClampMin="1", ClampMax="96")) int32 ContactCount = 48;
	UPROPERTY(EditAnywhere, Category="Contact", meta=(ClampMin="1", ClampMax="200", Units="cm")) FVector2D ContactSize = FVector2D(14, 34);
	UPROPERTY(EditAnywhere, Category="Contact", meta=(ClampMin="0.1", ClampMax="1.2", Units="s")) float ContactLifetime = .45f;
	UPROPERTY(EditAnywhere, Category="Contact", meta=(ClampMin="100", ClampMax="4000", Units="cm/s")) float ContactSpeed = 1600;
	UPROPERTY(EditAnywhere, Category="Contact", meta=(ClampMin="5", ClampMax="70", Units="deg")) float ContactSpreadDegrees = 45;
	UPROPERTY(EditAnywhere, Category="Contact", meta=(ClampMin="0.04", ClampMax="0.3", Units="s")) float FlashLifetime = .12f;
	UPROPERTY(EditAnywhere, Category="Contact", meta=(ClampMin="10", ClampMax="400", Units="cm")) float FlashSize = 150;
	UPROPERTY(EditAnywhere, Category="Contact", meta=(ClampMin="0.1", ClampMax="0.8", Units="s")) float ShockLifetime = .3f;
	UPROPERTY(EditAnywhere, Category="Contact", meta=(ClampMin="20", ClampMax="600", Units="cm")) float ShockSize = 240;
	UPROPERTY(EditAnywhere, Category="Style") FLinearColor SparkColor = FLinearColor(1, .42f, .06f);
	UPROPERTY(EditAnywhere, Category="Style") FLinearColor HighlightColor = FLinearColor(1, .92f, .63f);
	UPROPERTY(EditAnywhere, Category="Style", meta=(ClampMin="0", ClampMax="3")) float GlowStrength = 1.1f;
	bool IsValid() const;
};

/** Persistent presentation identity and phase, including late observers. No per-frame RPC. */
USTRUCT()
struct FHALBullChargeVisualState
{
	GENERATED_BODY()
	UPROPERTY() FHALBullChargeFXConfig FX;
	UPROPERTY() float DurationSeconds = 0;
	UPROPERTY() float RampSeconds = 0;
	UPROPERTY() FGuid Activation;
	UPROPERTY() double StartServerTime = -1;
	UPROPERTY() bool bDriving = false;
};

USTRUCT()
struct FHALBullChargeImpactFXEvent
{
	GENERATED_BODY()
	UPROPERTY() FGuid Activation;
	UPROPERTY() uint32 Sequence = 0;
	UPROPERTY() double ServerTime = 0;
	UPROPERTY() FVector_NetQuantize Point = FVector::ZeroVector;
	/** Points away from the contacted surface, toward the charging vehicle. */
	UPROPERTY() FVector_NetQuantizeNormal Normal = FVector::UpVector;
	UPROPERTY() float Strength = 0;
};

namespace HALBullChargeFX
{
	HAL_FUTURE_CREATION_API float Ramp(double Elapsed, float RampSeconds);
	HAL_FUTURE_CREATION_API float WindStrength(float ForwardSpeed, float FullSpeed, float Ramp, bool bDriving);
	HAL_FUTURE_CREATION_API float SmoothWind(float Current, float Target, float DeltaSeconds, float ResponseSeconds, bool bDriving);
	/** A bounded world-space velocity box; all directions stay in the backward/outgoing hemisphere. */
	HAL_FUTURE_CREATION_API void VelocityBox(const FVector& Direction, float Speed, float SpreadDegrees,
		FVector& Min, FVector& Max);
}
