#pragma once
#include "CoreMinimal.h"
#include "HALImpactTypes.h"
#include "HALTornadoTypes.generated.h"
class UNiagaraSystem;

UENUM(BlueprintType)
enum class EHALTornadoPhase : uint8 { Gathering, Active, Dissipating, Finished };

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALTornadoRules
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Timing", meta=(ClampMin="0.1", ClampMax="5", Units="s")) float GatherSeconds = .6f;
	UPROPERTY(EditAnywhere, Category="Timing", meta=(ClampMin="0.1", ClampMax="20", Units="s")) float ActiveSeconds = 5;
	UPROPERTY(EditAnywhere, Category="Timing", meta=(ClampMin="0.1", ClampMax="5", Units="s")) float DissipateSeconds = .8f;
	UPROPERTY(EditAnywhere, Category="Travel", meta=(ClampMin="0", ClampMax="2000", Units="cm/s")) float Speed = 800;
	UPROPERTY(EditAnywhere, Category="Travel", meta=(ClampMin="0", ClampMax="300", Units="cm")) float FrontOffset = 25;
	UPROPERTY(EditAnywhere, Category="Shape", meta=(ClampMin="30", ClampMax="400", Units="cm")) float Radius = 140;
	UPROPERTY(EditAnywhere, Category="Shape", meta=(ClampMin="60", ClampMax="600", Units="cm")) float Height = 280;
	UPROPERTY(EditAnywhere, Category="Ground", meta=(ClampMin="20", ClampMax="300", Units="cm")) float GroundProbe = 120;
	UPROPERTY(EditAnywhere, Category="Ground", meta=(ClampMin="5", ClampMax="100", Units="cm")) float MaximumGroundStep = 45;
	UPROPERTY(EditAnywhere, Category="Ground", meta=(ClampMin="0.2", ClampMax="1")) float MinimumGroundNormalZ = .55f;
	UPROPERTY(EditAnywhere, Category="Contact", meta=(ClampMin="0.1", ClampMax="5", Units="s")) float TargetCooldownSeconds = .8f;
	UPROPERTY(EditAnywhere, Category="Contact") EHALImpactPushPreset PushPreset = EHALImpactPushPreset::Long;
	UPROPERTY(EditAnywhere, Category="Ball", meta=(ClampMin="100", ClampMax="12000", Units="cm/s")) float BallSpeed = 3500;
	UPROPERTY(EditAnywhere, Category="Ball", meta=(ClampMin="0", ClampMax="2000", Units="cm/s")) float BallUpwardSpeed = 250;
	UPROPERTY(EditAnywhere, Category="Ball", meta=(ClampMin="0.1", ClampMax="2", Units="s")) float BallCooldownSeconds = .35f;
	UPROPERTY(EditAnywhere, Category="Ball", meta=(ClampMin="0.1", ClampMax="2", Units="s")) float DisarmPickupLockSeconds = .25f;
	UPROPERTY(EditAnywhere, Category="Debug") bool bDrawBounds = false;
	bool IsValid() const;
	float TotalSeconds() const { return GatherSeconds + ActiveSeconds + DissipateSeconds; }
	EHALTornadoPhase Phase(double Age) const;
	float PhaseProgress(double Age) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALTornadoFX
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Rendering") TObjectPtr<UNiagaraSystem> System;
	UPROPERTY(EditAnywhere, Category="Style", meta=(HideAlphaChannel)) FLinearColor ShadowColor = FLinearColor(.02f,.25f,.32f);
	UPROPERTY(EditAnywhere, Category="Style", meta=(HideAlphaChannel)) FLinearColor MidColor = FLinearColor(.08f,.60f,.66f);
	UPROPERTY(EditAnywhere, Category="Style", meta=(HideAlphaChannel)) FLinearColor LightColor = FLinearColor(.55f,.93f,.95f);
	UPROPERTY(EditAnywhere, Category="Style", meta=(ClampMin="0.02", ClampMax="0.4")) float ToneSoftness = .18f;
	UPROPERTY(EditAnywhere, Category="Style", meta=(ClampMin="0", ClampMax="1")) float ShadowThreshold = .4f;
	UPROPERTY(EditAnywhere, Category="Style", meta=(ClampMin="0", ClampMax="1")) float LightThreshold = .75f;
	UPROPERTY(EditAnywhere, Category="Style", meta=(ClampMin="0", ClampMax="1")) float Opacity = .65f;
	UPROPERTY(EditAnywhere, Category="Motion", meta=(ClampMin="0", ClampMax="3600", Units="deg/s")) float SpinRate = 900;
	UPROPERTY(EditAnywhere, Category="Motion", meta=(ClampMin="0", ClampMax="10")) float FlowSpeed = 1.2f;
	UPROPERTY(EditAnywhere, Category="Motion", meta=(ClampMin="0", ClampMax="10", Units="cm")) float Turbulence = 3;
	/** Per-stream angular-speed variation. Zero retains a common spin rate. */
	UPROPERTY(EditAnywhere, Category="Layered Flow", meta=(ClampMin="0", ClampMax="0.8")) float DifferentialRotation = .55f;
	/** Vertical circulation in normalized funnel heights per second. */
	UPROPERTY(EditAnywhere, Category="Layered Flow", meta=(ClampMin="0", ClampMax="1")) float RiseSpeed = .22f;
	UPROPERTY(EditAnywhere, Category="Layered Flow", meta=(ClampMin="0", ClampMax="1")) float CoreOpacity = .24f;
	UPROPERTY(EditAnywhere, Category="Outer Flow", meta=(ClampMin="0", ClampMax="1")) float OuterOpacity = .32f;
	UPROPERTY(EditAnywhere, Category="Outer Flow", meta=(ClampMin="1", ClampMax="1.5")) float OuterRadiusScale = 1.15f;
	UPROPERTY(EditAnywhere, Category="Outer Flow", meta=(ClampMin="0.1", ClampMax="2")) float OuterSpeedScale = .65f;
	UPROPERTY(EditAnywhere, Category="Color Flow", meta=(HideAlphaChannel)) FLinearColor GrayColor = FLinearColor(.70f,.78f,.80f);
	UPROPERTY(EditAnywhere, Category="Color Flow", meta=(ClampMin="0", ClampMax="1")) float GrayMix = .75f;
	UPROPERTY(EditAnywhere, Category="Color Flow", meta=(ClampMin="0.2", ClampMax="10", Units="s")) float ColorCycleSeconds = 1.6f;
	/** Uses Rules.GatherSeconds; values below one reveal the body earlier in that window. */
	UPROPERTY(EditAnywhere, Category="Formation", meta=(ClampMin="0.4", ClampMax="3")) float FormationExponent = .85f;
	/** Soft height band around the rising formation front, in normalized funnel heights. */
	UPROPERTY(EditAnywhere, Category="Formation", meta=(ClampMin="0.03", ClampMax="0.4")) float FormationFeather = .16f;
	/** Palette-colored rim emission; zero disables the highlight. Does not require Bloom. */
	UPROPERTY(EditAnywhere, Category="Glow", meta=(ClampMin="0", ClampMax="4")) float GlowStrength = 1.1f;
	/** Higher values produce a narrower view-dependent rim. */
	UPROPERTY(EditAnywhere, Category="Glow", meta=(ClampMin="1", ClampMax="8")) float GlowPower = 2.5f;
	UPROPERTY(EditAnywhere, Category="Gather", meta=(ClampMin="1", ClampMax="3")) float GatherRadiusScale = 1.6f;
	UPROPERTY(EditAnywhere, Category="Gather", meta=(ClampMin="0", ClampMax="30", Units="cm")) float GroundLift = 4;
	bool IsValid() const;
};

USTRUCT()
struct HAL_FUTURE_CREATION_API FHALTornadoState
{
	GENERATED_BODY()
	UPROPERTY() FGuid Activation;
	UPROPERTY() FHALTornadoRules Rules;
	UPROPERTY() FHALTornadoFX FX;
	UPROPERTY() FVector Ground = FVector::ZeroVector;
	UPROPERTY() FVector Direction = FVector::ForwardVector;
	UPROPERTY() double StartServerTime = -1;
	UPROPERTY() double SampleServerTime = -1;
	UPROPERTY() bool bStopped = false;

	/** Cosmetic ground position on the straight active leg, relative to this authority sample. */
	FVector PresentationGroundAt(double ServerSeconds) const;
};
