#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VehicleDefinition.h"
#include "HALCameraConfig.generated.h"

class UCurveFloat;

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALCameraTargetRule
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Weight", meta=(ClampMin="0",ClampMax="10")) float Weight = 1;
	UPROPERTY(EditAnywhere, Category="Range", meta=(ClampMin="0",Units="cm")) float FullWeightDistance = 1200;
	UPROPERTY(EditAnywhere, Category="Range", meta=(ClampMin="1",Units="cm")) float ZeroWeightDistance = 4000;
	/** Optional normalized distance [0,1] -> weight fraction [0,1]. Default is smooth falloff. */
	UPROPERTY(EditAnywhere, Category="Weight") TObjectPtr<UCurveFloat> DistanceCurve;
	UPROPERTY(EditAnywhere, Category="Framing", meta=(ClampMin="0",ClampMax="1")) float ZoomInfluence = 1;
	bool Validate() const;
	float Evaluate(float Distance) const;
};

/** Canonical camera profile, including horizontal FOV and base distance. All runtime state is local. */
UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALCameraConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UHALCameraConfig();
	UPROPERTY(EditAnywhere, Category="Status") bool bReadyForUse = false;
	UPROPERTY(EditAnywhere, Category="Mode") bool bDynamicEnabled = true;
	/** FieldOfView and TargetArmLength are the single profile sources for FOV and base distance. */
	UPROPERTY(EditAnywhere, Category="Base Camera") FVehicleCameraConfig BaseCamera;
	UPROPERTY(EditAnywhere, Category="Targets", meta=(ClampMin="0.1",ClampMax="10")) float OwnWeight = 3;
	UPROPERTY(EditAnywhere, Category="Targets") FHALCameraTargetRule MainBall;
	UPROPERTY(EditAnywhere, Category="Targets") FHALCameraTargetRule OtherPlayers;
	UPROPERTY(EditAnywhere, Category="Targets", meta=(ClampMin="0.01",ClampMax="2",Units="s")) float WeightResponseSeconds = .2f;
	UPROPERTY(EditAnywhere, Category="Targets", meta=(ClampMin="0",ClampMax="10000",Units="cm")) float TargetHeightLimit = 800;
	UPROPERTY(EditAnywhere, Category="Distance", meta=(ClampMin="100",ClampMax="20000",Units="cm")) float MinimumDistance = 600;
	UPROPERTY(EditAnywhere, Category="Distance", meta=(ClampMin="100",ClampMax="20000",Units="cm")) float MaximumDistance = 4000;
	/** Fraction of the effective screen occupied by the soft-target framing region. */
	UPROPERTY(EditAnywhere, Category="Framing", meta=(ClampMin="0.1",ClampMax="0.95")) FVector2D TargetFrameSize = FVector2D(.8,.8);
	UPROPERTY(EditAnywhere, Category="Own Vehicle", meta=(ClampMin="0.1",ClampMax="0.9")) FVector2D SoftFrameSize = FVector2D(.4,.4);
	UPROPERTY(EditAnywhere, Category="Own Vehicle", meta=(ClampMin="0.1",ClampMax="0.95")) FVector2D HardFrameSize = FVector2D(.6,.6);
	UPROPERTY(EditAnywhere, Category="Own Vehicle", meta=(ClampMin="0",ClampMax="0.1")) float OwnScreenPadding = .02f;
	UPROPERTY(EditAnywhere, Category="Response", meta=(ClampMin="0",ClampMax="3",Units="s")) float FocusResponseSeconds = .25f;
	UPROPERTY(EditAnywhere, Category="Response", meta=(ClampMin="0",ClampMax="3",Units="s")) float ZoomOutResponseSeconds = .15f;
	UPROPERTY(EditAnywhere, Category="Response", meta=(ClampMin="0",ClampMax="3",Units="s")) float ZoomInResponseSeconds = .7f;
	UPROPERTY(EditAnywhere, Category="Response", meta=(ClampMin="0",ClampMax="300",Units="cm")) float DistanceDeadZone = 25;
	UPROPERTY(EditAnywhere, Category="Height", meta=(ClampMin="0",ClampMax="3",Units="s")) float HeightResponseSeconds = .3f;
	UPROPERTY(EditAnywhere, Category="Height", meta=(ClampMin="0",ClampMax="2000",Units="cm")) float MaximumHeightLag = 150;
	/** Screen fractions per raw mouse delta. Mouse deltas are not multiplied by DeltaTime. */
	UPROPERTY(EditAnywhere, Category="Mouse", meta=(ClampMin="0.00001",ClampMax="0.02")) float MouseSensitivity = .0008f;
	UPROPERTY(EditAnywhere, Category="Mouse", meta=(ClampMin="0",ClampMax="10")) float MouseNoiseThreshold = .05f;
	UPROPERTY(EditAnywhere, Category="Mouse") bool bInvertMouseY = false;
	UPROPERTY(EditAnywhere, Category="Gamepad", meta=(ClampMin="0",ClampMax="0.9")) float StickDeadZone = .2f;
	UPROPERTY(EditAnywhere, Category="Gamepad", meta=(ClampMin="0.1",ClampMax="4")) float StickExponent = 1.5f;
	/** Fractions of effective screen per second at full stick. Positive input Y observes upward. */
	UPROPERTY(EditAnywhere, Category="Gamepad", meta=(ClampMin="0.01",ClampMax="2")) FVector2D StickSpeed = FVector2D(.35,.35);
	UPROPERTY(EditAnywhere, Category="Gamepad") bool bInvertStickY = false;
	UPROPERTY(EditAnywhere, Category="Manual", meta=(ClampMin="0",ClampMax="0.45")) FVector2D MaximumManualOffset = FVector2D(.35,.35);
	UPROPERTY(EditAnywhere, Category="Manual", meta=(ClampMin="0",ClampMax="5",Units="s")) float ReturnDelaySeconds = .6f;
	UPROPERTY(EditAnywhere, Category="Manual", meta=(ClampMin="0",ClampMax="3",Units="s")) float ReturnResponseSeconds = .3f;
	UPROPERTY(EditAnywhere, Category="Sampling", meta=(ClampMin="0.05",ClampMax="1",Units="s")) float MaximumSampleDelta = .25f;
	UPROPERTY(EditAnywhere, Category="Sampling", meta=(ClampMin="100",ClampMax="10000",Units="cm")) float TeleportResetDistance = 1000;
	bool Validate(FString& Error) const;
#if WITH_EDITORONLY_DATA
	/** Team-operated migration button only; never copies runtime camera state back into a shared DA. */
	UPROPERTY(EditAnywhere, Category="Migration") TObjectPtr<UVehicleLocalConfig> LegacySource;
#endif
	UFUNCTION(CallInEditor, Category="Migration") void CopyLegacyCamera();
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
