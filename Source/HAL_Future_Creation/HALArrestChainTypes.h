#pragma once

#include "CoreMinimal.h"
#include "HALArrestChainTypes.generated.h"
class UStaticMesh;
class UMaterialInterface;
class UVehicleDefinition;

/** A narrow fit correction for a cab/roof that is offset from the whole vehicle's bounds. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALArrestChainFit
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Fit") TObjectPtr<UVehicleDefinition> VehicleDefinition;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Fit",meta=(Units="cm")) FVector CenterOffset=FVector::ZeroVector;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Fit",meta=(ClampMin="0.25",ClampMax="2")) FVector BoundsScale=FVector(.94f,.94f,1);
};

/** Cosmetic-only Car06 binding. All positions are in the vehicle frame, never the imported rig axes. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALArrestChainFX
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Assets") bool bEnabled=true;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Assets") TObjectPtr<UStaticMesh> LinkMesh;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Assets") TObjectPtr<UMaterialInterface> Material;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Fit") FName PartId=TEXT("Body");
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Fit",meta=(ClampMin="0.25",ClampMax="2")) FVector BoundsScale=FVector(.94f,.94f,1);
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Fit",meta=(Units="cm")) FVector CenterOffset=FVector(0,0,-2);
	/** Optional absolute offset overrides, still authored on this one skill DA. */
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Fit",meta=(TitleProperty="VehicleDefinition")) TArray<FHALArrestChainFit> VehicleFits;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Fit",meta=(ClampMin="0",ClampMax="0.4")) float LowerHeightFraction=.15f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Fit",meta=(ClampMin="0.6",ClampMax="1.2")) float UpperHeightFraction=1;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Fit",meta=(ClampMin="0",ClampMax="40",Units="cm")) float Clearance=18;
	/** Use an even count so alternating links also interlock at the closed seam. */
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Geometry",meta=(ClampMin="16",ClampMax="96",Delta="2")) int32 LinksPerLoop=64;
	/** Two vertical wrapping planes at +/- this angle from vehicle forward. */
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Geometry",meta=(ClampMin="10",ClampMax="75",Units="deg")) float CrossAngle=36;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Geometry",meta=(ClampMin="2",ClampMax="6")) float ProfileExponent=4;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Geometry",meta=(ClampMin="2",ClampMax="30",Units="cm")) float LinkWidth=18;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Geometry",meta=(ClampMin="1.05",ClampMax="1.6")) float LinkOverlap=1.3f;
	/** Legacy source-mesh plane thickness. A compatible WireRadius material replaces this stretched cross-section. */
	UPROPERTY(EditAnywhere,BlueprintReadOnly,AdvancedDisplay,Category="Geometry",meta=(ClampMin="0.25",ClampMax="3")) float WireScale=1.25f;
	/** World-space metal radius for a compatible link material. Does not enlarge the ring centerline. */
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Geometry",meta=(ClampMin="0.25",ClampMax="6",Units="cm")) float WireRadius=2.8f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Geometry",meta=(ClampMin="2",ClampMax="20",Units="cm")) float CrossingSeparation=8;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Animation",meta=(ClampMin="0.05",ClampMax="1",Units="s")) float TightenSeconds=.24f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Animation",meta=(ClampMin="0",ClampMax="0.8")) float InitialLooseness=.28f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Animation",meta=(ClampMin="0",ClampMax="0.1")) float ReboundAmount=.025f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Animation",meta=(ClampMin="0.05",ClampMax="1",Units="s")) float ReboundSeconds=.3f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Animation",meta=(ClampMin="0.05",ClampMax="0.6",Units="s")) float ReleaseSeconds=.18f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Animation",meta=(ClampMin="0",ClampMax="0.8")) float ReleaseLooseness=.18f;
	/** Signed speed along each closed chain, not rotation of the whole wrapping plane. Zero stops circulation. */
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Animation",meta=(ClampMin="-200",ClampMax="200",Units="cm/s")) float FlowSpeed=45;
	/** Reverse the second loop relative to the first; each still follows its own wrapping path. */
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Animation") bool bOppositeLoopFlow=true;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Animation",meta=(ClampMin="0",ClampMax="2",Units="s")) float FlowRampSeconds=.25f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Metal") FLinearColor DarkColor=FLinearColor(.055f,.075f,.09f);
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Metal") FLinearColor MiddleColor=FLinearColor(.24f,.29f,.34f);
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Metal") FLinearColor LightColor=FLinearColor(.72f,.8f,.86f);
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Metal") FVector LightDirection=FVector(.35f,-.55f,.75f);
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Metal",meta=(ClampMin="-0.8",ClampMax="0.3")) float ShadowThreshold=-.15f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Metal",meta=(ClampMin="0.1",ClampMax="0.95")) float HighlightThreshold=.5f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Metal",meta=(ClampMin="0.02",ClampMax="0.4")) float ToneSoftness=.16f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Metal",meta=(ClampMin="0",ClampMax="2")) float GlintStrength=.35f;
	bool IsValid() const;
};

struct FHALArrestChainEnvelope
{
	float Looseness=0;
	float Visibility=0;
	bool bFinished=true;
};
namespace HALArrestChains
{
	HAL_FUTURE_CREATION_API FHALArrestChainEnvelope Envelope(const FHALArrestChainFX& Config,
		bool Arrested,double Now,double Start,double ReleaseStart);
	/** Integrated smooth speed, derived solely from replicated timestamps; stable across frame rates and late joins. */
	HAL_FUTURE_CREATION_API double FlowDistance(const FHALArrestChainFX& Config,double Now,double Start,double ReleaseStart);
	/** Closed, arc-length sampled interlocking rings. Link mesh major axis X, link plane XY. */
	HAL_FUTURE_CREATION_API bool BuildPose(const FBox& VehicleBounds,const FHALArrestChainFX& Config,
		const FVector& MeshDimensions,float Looseness,TArray<FTransform>& Out,double FlowDistance=0);
}
