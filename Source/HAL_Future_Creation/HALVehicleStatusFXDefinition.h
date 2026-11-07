#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "HALShockVisualSettings.h"
#include "HALVehicleStatusFXDefinition.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class UHALStatusFXSurfaceLayout;

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALStatusFXScalar
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parameter") FName Parameter;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parameter") float Value = 0;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALStatusFXColor
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parameter") FName Parameter;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parameter") FLinearColor Value = FLinearColor::White;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALStatusFXCurve
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parameter") FName Parameter;
	/** X: seconds since the visual instance started; Y: parameter value. Held beyond the last key. */
	UPROPERTY(EditAnywhere, Category="Parameter") FRuntimeFloatCurve Value;
};

UENUM(BlueprintType)
enum class EHALStatusFXBinding : uint8
{
	/** Approximate envelope using the FX-1 bounds, not triangle sampling. */
	Bounds,
	/** Separate reusable Static/Skeletal templates. Runtime sampling adapters are introduced in FX-4. */
	MeshSurface
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALStatusFXNiagaraLayer
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layer") FName LayerId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layer") EHALStatusFXBinding Binding = EHALStatusFXBinding::Bounds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layer", meta=(EditCondition="Binding == EHALStatusFXBinding::Bounds", EditConditionHides))
	TObjectPtr<UNiagaraSystem> BoundsSystem;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layer", meta=(EditCondition="Binding == EHALStatusFXBinding::MeshSurface", EditConditionHides))
	TObjectPtr<UNiagaraSystem> StaticSurfaceSystem;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layer", meta=(EditCondition="Binding == EHALStatusFXBinding::MeshSurface", EditConditionHides))
	TObjectPtr<UNiagaraSystem> SkeletalSurfaceSystem;
	/** Optional, explicitly configured Bounds substitute if the surface binding is unsupported. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layer", meta=(EditCondition="Binding == EHALStatusFXBinding::MeshSurface", EditConditionHides))
	TObjectPtr<UNiagaraSystem> SurfaceFallbackBoundsSystem;
	/** Envelope growth relative to each mesh extent. Not a collision/body scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bounds", meta=(ClampMin="0", ClampMax="1")) float PaddingFraction = 0.07f;
	/** CPU triangle sampling path; source mesh must retain CPU data for this LOD. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sampling", meta=(ClampMin="0", ClampMax="8")) int32 SamplingLOD = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Budget", meta=(ClampMin="1", ClampMax="4096")) int32 MaximumParticles = 128;
	/** Particles per second before size scaling and visual envelope. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Budget", meta=(ClampMin="0", ClampMax="2000")) float SpawnRate = 20;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Budget") bool bScaleRateBySize = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Budget", meta=(ClampMin="100", ClampMax="10000000")) float ReferenceAreaCm2 = 60000;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing") bool bEmitDuringResidue = false;
};

/** Immutable shared visual recipe. No HP, force, GE duration, MID or per-vehicle clock. */
UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALVehicleStatusFXDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	/** Opt in after the shared material/Niagara contract is configured. Existing assets remain observation-only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rendering") bool bEnableRendering = false;
	/** Use the existing electric bounce/authority driver with this recipe's shared visual settings. */
	UPROPERTY(EditAnywhere, Category="Shocked", meta=(EditCondition="bEnableRendering"))
	bool bUseSharedShockLook = false;
	UPROPERTY(EditAnywhere, Category="Shocked", meta=(EditCondition="bUseSharedShockLook", EditConditionHides))
	FHALShockVisualSettings ShockLook;
	/** Optional compatible material family. Leave Overlay Material empty, or use the same material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface") TObjectPtr<UHALStatusFXSurfaceLayout> SurfaceLayout;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface") FName SurfaceChannel = TEXT("Primary");
	/** Standalone material standard parameters; layout recipes instead use Prefix+Intensity/Age/Residue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface") FName IntensityParameter = TEXT("FX_Intensity");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface") FName AgeParameter = TEXT("FX_Age");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface") FName ResidueParameter = TEXT("FX_Residue");
	UMaterialInterface* GetSurfaceMaterial() const;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface") TObjectPtr<UMaterialInterface> OverlayMaterial;
	/** Higher wins when surface styles cannot coexist; ties will use the catalog tag ordering. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface", meta=(ClampMin="-1000", ClampMax="1000")) int32 SurfacePriority = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface", meta=(TitleProperty="Parameter")) TArray<FHALStatusFXScalar> Scalars;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface", meta=(TitleProperty="Parameter")) TArray<FHALStatusFXColor> Colors;
	UPROPERTY(EditAnywhere, Category="Surface", meta=(TitleProperty="Parameter")) TArray<FHALStatusFXCurve> Curves;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Particles", meta=(TitleProperty="LayerId")) TArray<FHALStatusFXNiagaraLayer> NiagaraLayers;
	/** Cosmetic transitions only. Active gameplay state is owned by GAS. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(Units="s", ClampMin="0", ClampMax="10")) float FadeInSeconds = 0.1f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(Units="s", ClampMin="0", ClampMax="10")) float FadeOutSeconds = 0.3f;
	/** Optional visual residue after release, e.g. scorch. Does not prolong the actual debuff. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(Units="s", ClampMin="0", ClampMax="30")) float ResidueSeconds = 0;
	virtual bool Validate(FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
