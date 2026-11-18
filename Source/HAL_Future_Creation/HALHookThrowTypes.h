#pragma once
#include "CoreMinimal.h"
#include "HALHookThrowTypes.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UNiagaraSystem;

UENUM(BlueprintType)
enum class EHALHookPhase : uint8 { Outbound, Choosing, Returning, Interrupted, Finished, Swinging };

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALHookRules
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere,Category="Hook",meta=(ClampMin="200",ClampMax="4000",Units="cm")) float Range=1500;
	UPROPERTY(EditAnywhere,Category="Hook",meta=(ClampMin="200",ClampMax="10000",Units="cm/s")) float OutboundSpeed=2500;
	UPROPERTY(EditAnywhere,Category="Hook",meta=(ClampMin="500",ClampMax="15000",Units="cm/s")) float ReturnSpeed=5000;
	UPROPERTY(EditAnywhere,Category="Hook",meta=(ClampMin="5",ClampMax="80",Units="cm")) float HitRadius=28;
	/** Collision-root local offset. Keep the start clear of the vehicle's own visual mesh. */
	UPROPERTY(EditAnywhere,Category="Hook") FVector OutletOffset=FVector(170,0,45);
	UPROPERTY(EditAnywhere,Category="Input",meta=(ClampMin="0.2",ClampMax="3",Units="s")) float ChoiceSeconds=1.2f;
	UPROPERTY(EditAnywhere,Category="Input",meta=(ClampMin="0.1",ClampMax="0.9")) float DirectionThreshold=.35f;
	UPROPERTY(EditAnywhere,Category="Swing",meta=(ClampMin="0.1",ClampMax="1.5",Units="s")) float SwingSeconds=.3f;
	UPROPERTY(EditAnywhere,Category="Swing",meta=(ClampMin="1",ClampMax="80",Units="deg")) float SwingAngle=30;
	UPROPERTY(EditAnywhere,Category="Swing",meta=(ClampMin="20",ClampMax="1500",Units="cm")) float SwingArcLength=250;
	/** Ease-in exponent. Greater values build speed later; the release never eases to zero. */
	UPROPERTY(EditAnywhere,Category="Swing",meta=(ClampMin="1.2",ClampMax="4")) float SwingAccelerationPower=2;
	/** Bounded planar motor acceleration in cm/s squared. Contacts can shorten the sweep. */
	UPROPERTY(EditAnywhere,Category="Swing",meta=(ClampMin="1000",ClampMax="50000")) float SwingMaximumAcceleration=20000;
	/** Maximum authored swing exit speed. Release keeps actual momentum, including collision losses. */
	UPROPERTY(EditAnywhere,Category="Throw",meta=(ClampMin="100",ClampMax="6000",Units="cm/s")) float HorizontalSpeed=1800;
	UPROPERTY(EditAnywhere,Category="Throw",meta=(ClampMin="0",ClampMax="1500",Units="cm/s")) float UpwardSpeed=250;
	UPROPERTY(EditAnywhere,Category="Physics",meta=(ClampMin="0",ClampMax="300")) float CaptureStiffness=80;
	UPROPERTY(EditAnywhere,Category="Physics",meta=(ClampMin="1",ClampMax="80")) float Damping=18;
	/** Maximum planar acceleration in cm/s squared. */
	UPROPERTY(EditAnywhere,Category="Physics",meta=(ClampMin="100",ClampMax="50000")) float MaximumAcceleration=8000;
	UPROPERTY(EditAnywhere,Category="Physics",meta=(ClampMin="50",ClampMax="1500",Units="cm")) float MaximumCaptureDrift=350;
	/** Applied external launch below this ascent speed still interrupts when it actually loses ground. */
	UPROPERTY(EditAnywhere,Category="Interrupt",meta=(ClampMin="20",ClampMax="1500",Units="cm/s")) float AirborneUpwardSpeed=100;
	UPROPERTY(EditAnywhere,Category="Debug") bool bDrawBounds=false;
	bool IsValid() const;
	float MaximumSeconds() const;
};

/** One burst at the vehicle surface when a hook successfully latches. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALHookCaptureSparks
{
	GENERATED_BODY()
	/** Optional one-shot system. The default asset uses Battle Top's toon spark user parameters. */
	UPROPERTY(EditAnywhere, Category="Sparks") TObjectPtr<UNiagaraSystem> System;
	UPROPERTY(EditAnywhere, Category="Sparks", meta=(ClampMin="0",ClampMax="64")) int32 Count=14;
	/** Multiplier for the width and length of the cartoon strokes. */
	UPROPERTY(EditAnywhere, Category="Sparks", meta=(ClampMin="0.25",ClampMax="4")) float SizeScale=1;
	/** Longest particle lifetime. Shorter strokes live for 65% of this value. */
	UPROPERTY(EditAnywhere, Category="Sparks", meta=(ClampMin="0.08",ClampMax="1",Units="s")) float Lifetime=.24f;
	/** Outward emission speed along the contact normal before sideways spread. */
	UPROPERTY(EditAnywhere, Category="Sparks", meta=(ClampMin="100",ClampMax="2500",Units="cm/s")) float Speed=620;
	UPROPERTY(EditAnywhere, Category="Sparks") FLinearColor Color=FLinearColor(1.f,.67f,.22f);
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALHookFX
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere,Category="Assets") bool bEnabled=true;
	UPROPERTY(EditAnywhere,Category="Assets") TObjectPtr<UStaticMesh> ChainMesh;
	UPROPERTY(EditAnywhere,Category="Assets") TObjectPtr<UStaticMesh> HookMesh;
	UPROPERTY(EditAnywhere,Category="Assets") TObjectPtr<UMaterialInterface> MetalMaterial;
	UPROPERTY(EditAnywhere,Category="Contact") FHALHookCaptureSparks CaptureSparks;
	UPROPERTY(EditAnywhere,Category="Geometry",meta=(ClampMin="8",ClampMax="80",Units="cm")) float LinkPitch=35;
	UPROPERTY(EditAnywhere,Category="Geometry",meta=(ClampMin="0.3",ClampMax="4")) float LinkScale=1.9f;
	UPROPERTY(EditAnywhere,Category="Geometry",meta=(ClampMin="0.3",ClampMax="4")) float HookScale=2.12f;
	/** Connection point in the hook mesh's local coordinates. */
	UPROPERTY(EditAnywhere,Category="Geometry") FVector HookChainOffset=FVector(-45,0,0);
	UPROPERTY(EditAnywhere,Category="Geometry",meta=(ClampMin="16",ClampMax="512")) int32 MaximumLinks=256;
	UPROPERTY(EditAnywhere,Category="Motion",meta=(ClampMin="0",ClampMax="80",Units="cm")) float Sag=12;
	UPROPERTY(EditAnywhere,Category="Motion",meta=(ClampMin="0",ClampMax="30",Units="cm")) float Wobble=3;
	UPROPERTY(EditAnywhere,Category="Motion",meta=(ClampMin="0",ClampMax="20")) float WobbleFrequency=3;
	UPROPERTY(EditAnywhere,Category="Motion",meta=(ClampMin="0.02",ClampMax="0.3",Units="s")) float EnterSeconds=.06f;
	UPROPERTY(EditAnywhere,Category="Metal") FLinearColor DarkColor=FLinearColor(.028f,.042f,.064f);
	UPROPERTY(EditAnywhere,Category="Metal") FLinearColor MiddleColor=FLinearColor(.23f,.32f,.44f);
	UPROPERTY(EditAnywhere,Category="Metal") FLinearColor LightColor=FLinearColor(.95f,1.02f,1.12f);
	UPROPERTY(EditAnywhere,Category="Metal") FVector LightDirection=FVector(.35f,-.55f,.75f);
	UPROPERTY(EditAnywhere,Category="Metal",meta=(ClampMin="-0.8",ClampMax="0.3")) float ShadowThreshold=-.04f;
	UPROPERTY(EditAnywhere,Category="Metal",meta=(ClampMin="0.1",ClampMax="0.95")) float HighlightThreshold=.48f;
	UPROPERTY(EditAnywhere,Category="Metal",meta=(ClampMin="0.02",ClampMax="0.4")) float ToneSoftness=.125f;
	UPROPERTY(EditAnywhere,Category="Metal",meta=(ClampMin="0",ClampMax="2")) float GlintStrength=.75f;
	UPROPERTY(EditAnywhere,Category="Metal",meta=(ClampMin="2",ClampMax="80")) float GlintPower=13;
	UPROPERTY(EditAnywhere,Category="Metal",meta=(ClampMin="0",ClampMax="1")) float ContourStrength=.48f;
	bool IsValid() const;
};

namespace HALHook
{
	HAL_FUTURE_CREATION_API FVector VisualPoint(const FVector& Start,const FVector& End,float Alpha,double Age,const FHALHookFX& FX,bool Taut);
	HAL_FUTURE_CREATION_API void BuildChain(const FVector& Start,const FVector& End,double Age,const FHALHookFX& FX,bool Taut,TArray<FTransform>& Out);
}
