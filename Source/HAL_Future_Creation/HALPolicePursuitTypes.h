#pragma once
#include "CoreMinimal.h"
#include "HALVehicleFXMount.h"
#include "HALPolicePursuitTypes.generated.h"
class UMaterialInterface; class UStaticMesh; class USoundBase; class UHALPursuitHUDWidget; class ATestVehiclePawn;

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALPursuitRules
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere,Category="Timing",meta=(ClampMin="1",ClampMax="30",Units="s")) float Duration=8;
	UPROPERTY(EditAnywhere,Category="Range",meta=(ClampMin="100",ClampMax="3000",Units="cm")) float Radius=900;
	UPROPERTY(EditAnywhere,Category="Range",meta=(ClampMin="0",ClampMax="100",Units="cm")) float ExitBuffer=25;
	UPROPERTY(EditAnywhere,Category="Range",meta=(ClampMin="50",ClampMax="1000",Units="cm")) float MaximumHeightDifference=300;
	UPROPERTY(EditAnywhere,Category="Wanted",meta=(ClampMin="1",ClampMax="1000")) float Threshold=100;
	UPROPERTY(EditAnywhere,Category="Wanted",meta=(ClampMin="1",ClampMax="1000")) float GainPerSecond=25;
	UPROPERTY(EditAnywhere,Category="Wanted",meta=(ClampMin="1",ClampMax="1000")) float DecayPerSecond=20;
	UPROPERTY(EditAnywhere,Category="Arrest",meta=(ClampMin="0.1",ClampMax="10",Units="s")) float StunSeconds=1.5f;
	UPROPERTY(EditAnywhere,Category="Arrest",meta=(ClampMin="0.1",ClampMax="10",Units="s")) float ProtectionSeconds=2;
	UPROPERTY(EditAnywhere,Category="Sampling",meta=(ClampMin="0.02",ClampMax="0.2",Units="s")) float ScanInterval=.05f;
	UPROPERTY(EditAnywhere,Category="Debug") bool bDrawBounds=false;
	bool IsValid() const;
	float Integrate(float Value,float Seconds,bool InRange,bool Stunned,bool Protected) const;
};
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALPursuitFX
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Assets") TObjectPtr<UStaticMesh> RangeMesh;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Assets") TObjectPtr<UMaterialInterface> RangeMaterial;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Assets") TObjectPtr<UStaticMesh> BeaconMesh;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Assets") TObjectPtr<UMaterialInterface> SignalMaterial;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Assets") TObjectPtr<UStaticMesh> ArrestMesh;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Assets") TObjectPtr<USoundBase> SirenSound;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="UI") TSubclassOf<UHALPursuitHUDWidget> WidgetClass;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Roof Lights") FHALVehicleFXMount RoofMount;
	/** Default uses the registered visual mesh's roof; disable for a precise socket/offset. */
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Roof Lights") bool bAutomaticRoof=true;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Roof Lights",meta=(ClampMin="0",ClampMax="100",Units="cm")) float RoofLift=8;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Roof Lights",meta=(ClampMin="5",ClampMax="100",Units="cm")) float LightSpacing=35;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Roof Lights",meta=(ClampMin="2",ClampMax="30",Units="cm")) float BeaconRadius=9;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Roof Lights",meta=(ClampMin="0.1",ClampMax="2",Units="s")) float FlashHalfPeriod=.22f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Roof Lights",meta=(ClampMin="0",ClampMax="10000")) float LightIntensity=1800;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Roof Lights",meta=(ClampMin="50",ClampMax="500",Units="cm")) float LightRadius=180;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Roof Lights",meta=(ClampMin="0",ClampMax="10")) float SignalGlow=2;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Colors") FLinearColor Red=FLinearColor(1,.035f,.06f);
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Colors") FLinearColor Blue=FLinearColor(.035f,.18f,1);
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Range",meta=(ClampMin="0",ClampMax="1")) float RangeOpacity=.12f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Range",meta=(ClampMin="0",ClampMax="1")) float RangeRingOpacity=.36f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Range",meta=(ClampMin="0.05",ClampMax="4")) float ScanSpeed=.65f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Range",meta=(ClampMin="0.02",ClampMax="0.5")) float ScanWidth=.12f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Range",meta=(ClampMin="0",ClampMax="1")) float ScanOpacity=.18f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Range",meta=(ClampMin="0.005",ClampMax="0.1")) float RingWidth=.025f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Range",meta=(ClampMin="1",ClampMax="30",Units="cm")) float GroundLift=3;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Arrest",meta=(ClampMin="10",ClampMax="150",Units="cm")) float ArrestSize=65;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Arrest",meta=(ClampMin="10",ClampMax="200",Units="cm")) float ArrestHeight=55;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Arrest",meta=(ClampMin="0.1",ClampMax="5")) float ArrestPulseSpeed=1.5f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="UI",meta=(ClampMin="1",ClampMax="10")) int32 StarCount=6;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="UI",meta=(ClampMin="8",ClampMax="40")) float StarSize=18;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="UI",meta=(ClampMin="0",ClampMax="15")) float StarGap=3;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="UI",meta=(ClampMin="20",ClampMax="150")) float StarsAboveHealth=52;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="UI") FLinearColor StarColor=FLinearColor(.65f,.68f,.73f);
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="UI") FLinearColor StarOutline=FLinearColor(.05f,.06f,.08f);
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Audio",meta=(ClampMin="0",ClampMax="2")) float SirenVolume=.35f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Audio",meta=(ClampMin="50",ClampMax="1000",Units="cm")) float SirenInnerRadius=200;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Audio",meta=(ClampMin="100",ClampMax="5000",Units="cm")) float SirenFalloff=2500;
	bool IsValid() const;
};
USTRUCT()
struct FHALPursuitRecord
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<ATestVehiclePawn> Target;
	UPROPERTY() float Value=0;
	UPROPERTY() bool bInside=false;
	UPROPERTY() bool bArrested=false;
};
USTRUCT()
struct FHALPursuitState
{
	GENERATED_BODY()
	UPROPERTY() FGuid Activation;
	UPROPERTY() FHALPursuitRules Rules;
	UPROPERTY() FHALPursuitFX FX;
	UPROPERTY() double Start=-1;
	UPROPERTY() TArray<FHALPursuitRecord> Records;
};
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALPursuitView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TObjectPtr<ATestVehiclePawn> Target;
	UPROPERTY(BlueprintReadOnly) float Fraction=0;
	UPROPERTY(BlueprintReadOnly) bool bArrested=false;
	UPROPERTY(BlueprintReadOnly) bool bProtected=false;
	UPROPERTY(BlueprintReadOnly) FHALPursuitFX Style;
	float StarFill(int32 Index) const { return FMath::Clamp(Fraction*Style.StarCount-Index,0.f,1.f); }
};
