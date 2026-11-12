#pragma once

#include "Components/DynamicMeshComponent.h"
#include "HALGroundSurfacePresentation.generated.h"

class UMaterialInstanceDynamic;

/** Shared authoring data for a raised ground surface. Distances are world centimeters. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALGroundSurfaceDeformation
{
	GENERATED_BODY()
	/** Enable after the material consumes the HAL_Ground parameters and Vertex Color R. */
	UPROPERTY(EditAnywhere, Category="Deformation") bool bEnabled = false;
	/** Optional real shadow from the deformed visual mesh. Does not enable collision or lighting reception. */
	UPROPERTY(EditAnywhere, Category="Deformation", meta=(EditCondition="bEnabled"))
	bool bCastShadow = false;
	/** Even subdivision count across each strip and the diameter of rounded ends. */
	UPROPERTY(EditAnywhere, Category="Deformation", meta=(ClampMin="2", ClampMax="16", EditCondition="bEnabled"))
	int32 WidthSubdivisions = 8;
	/** Desired longitudinal spacing. Very long paths use a coarser bounded mesh. */
	UPROPERTY(EditAnywhere, Category="Deformation", meta=(ClampMin="20", ClampMax="200", Units="cm", EditCondition="bEnabled"))
	float VertexSpacing = 40.f;
	UPROPERTY(EditAnywhere, Category="Deformation", meta=(ClampMin="0", ClampMax="60", Units="cm", EditCondition="bEnabled"))
	float BulgeHeight = 8.f;
	/** Keep at or below Bulge Height so the surface stays above its sampled ground. */
	UPROPERTY(EditAnywhere, Category="Deformation", meta=(ClampMin="0", ClampMax="20", Units="cm", EditCondition="bEnabled"))
	float WaveAmplitude = 2.f;
	UPROPERTY(EditAnywhere, Category="Deformation", meta=(ClampMin="0.5", ClampMax="20", Units="s", EditCondition="bEnabled"))
	float WavePeriod = 3.f;
	UPROPERTY(EditAnywhere, Category="Deformation", meta=(ClampMin="40", ClampMax="2000", Units="cm", EditCondition="bEnabled"))
	float WaveLength = 180.f;
	UPROPERTY(EditAnywhere, Category="Deformation", meta=(ClampMin="-180", ClampMax="180", Units="deg", EditCondition="bEnabled"))
	float WaveDirection = 30.f;
	bool IsValid() const;
	float MaximumDisplacement() const { return bEnabled ? BulgeHeight + WaveAmplitude : 0.f; }
	bool operator==(const FHALGroundSurfaceDeformation& Other) const = default;
};

/** Bounded render-only margin; never passed to a gameplay footprint query. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALGroundSurfacePaintEdge
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Paint Edge") bool bEnabled = false;
	UPROPERTY(EditAnywhere, Category="Paint Edge", meta=(ClampMin="0", ClampMax="30", Units="cm", EditCondition="bEnabled"))
	float MaximumOutset = 15.f;
	/** Additional half-width is capped by both Maximum Outset and this fraction. */
	UPROPERTY(EditAnywhere, Category="Paint Edge", meta=(ClampMin="0", ClampMax="0.2", EditCondition="bEnabled"))
	float MaximumHalfWidthFraction = .08f;
	bool IsValid() const;
	float Padding(float HalfWidth) const;
	bool operator==(const FHALGroundSurfacePaintEdge& Other) const = default;
};

/** Already evaluated shape data. Width and ground sampling belong to the calling area. */
struct HAL_FUTURE_CREATION_API FHALGroundSurfacePoint
{
	FVector Ground = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	float HalfWidth = 1.f;
	bool bLinked = false;
};

namespace HALGroundSurface
{
	/** UV0 tiles in ground centimeters. Vertex Color R is zero at the edge and one at the center. */
	HAL_FUTURE_CREATION_API UE::Geometry::FDynamicMesh3 BuildRibbon(TConstArrayView<FHALGroundSurfacePoint> Points,
		const FVector& Origin, float GroundLift, float TextureTileSize, const FHALGroundSurfaceDeformation& Deformation,
		const FHALGroundSurfacePaintEdge& PaintEdge = FHALGroundSurfacePaintEdge());
}

/** Per-area material state and WPO bounds, reusable by ground-effect actors. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALGroundSurfaceComponent : public UDynamicMeshComponent
{
	GENERATED_BODY()
public:
	UHALGroundSurfaceComponent();
	void ConfigureSurface(UMaterialInterface* Material, float TextureTileSize, const FHALGroundSurfaceDeformation& Deformation,
		const FHALGroundSurfacePaintEdge& PaintEdge = FHALGroundSurfacePaintEdge());
	/** Caller supplies elapsed server game time, including when the component first becomes visible. */
	void SetSurfaceAge(float Age);
	/** Area-local, world-fixed dissolve; works even when vertex deformation is disabled. */
	void SetDissolveProgress(float Progress);
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
private:
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> SourceMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RuntimeMaterial;
	FHALGroundSurfaceDeformation Settings;
	FHALGroundSurfacePaintEdge EdgeSettings;
	float TileSize = 320.f;
};
