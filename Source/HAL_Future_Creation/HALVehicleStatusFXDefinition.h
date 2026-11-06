#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "HALVehicleStatusFXDefinition.generated.h"

class UMaterialInterface;
class UNiagaraSystem;

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
	/** Envelope growth relative to each mesh extent. Not a collision/body scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bounds", meta=(ClampMin="0", ClampMax="1")) float PaddingFraction = 0.07f;
};

/** Immutable shared visual recipe. No HP, force, GE duration, MID or per-vehicle clock. */
UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALVehicleStatusFXDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
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
