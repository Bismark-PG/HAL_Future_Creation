#pragma once
#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "HALSmokeScreenTypes.generated.h"

class UNiagaraSystem;
class UMaterialInterface;

/** Car08 smoke uses world-fixed, independently ageing puffs. Units are cm and seconds. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALSmokeScreenRules
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Timing", meta=(Units="s", ClampMin="0.1", ClampMax="15")) float EmissionSeconds=4;
	UPROPERTY(EditAnywhere, Category="Timing", meta=(Units="s", ClampMin="0.5", ClampMax="20")) float PuffSeconds=6;
	/** Included in Puff Seconds; area strength and visible smoke fade together. */
	UPROPERTY(EditAnywhere, Category="Timing", meta=(Units="s", ClampMin="0.1", ClampMax="5")) float DissolveSeconds=1;
	UPROPERTY(EditAnywhere, Category="Shape", meta=(Units="cm", ClampMin="20", ClampMax="500")) float InitialRadius=100;
	UPROPERTY(EditAnywhere, Category="Shape", meta=(Units="cm", ClampMin="30", ClampMax="1000")) float MaximumRadius=350;
	UPROPERTY(EditAnywhere, Category="Shape", meta=(Units="s", ClampMin="0.1", ClampMax="5")) float ExpansionSeconds=1.2f;
	UPROPERTY(EditAnywhere, Category="Shape", meta=(Units="cm", ClampMin="80", ClampMax="700")) float Height=300;
	/** One physical-root-space outlet drives both authority sampling and the cosmetic upward jet. */
	UPROPERTY(EditAnywhere, Category="Outlet", meta=(Units="cm")) FVector OutletOffset=FVector(130,0,180);
	UPROPERTY(EditAnywhere, Category="Sampling", meta=(Units="s", ClampMin="0.03", ClampMax="0.15")) float UpdateSeconds=.05f;
	UPROPERTY(EditAnywhere, Category="Sampling", meta=(Units="cm", ClampMin="30", ClampMax="300")) float SampleSpacing=120;
	UPROPERTY(EditAnywhere, Category="Sampling", meta=(Units="s", ClampMin="0.1", ClampMax="1")) float StationaryInterval=.3f;
	UPROPERTY(EditAnywhere, Category="Budget", meta=(ClampMin="8", ClampMax="128")) int32 MaximumPuffs=96;
	/** Radial footprint clipped by solid arena geometry; vehicles/balls never clip smoke. */
	UPROPERTY(EditAnywhere, Category="Walls", meta=(ClampMin="12", ClampMax="48")) int32 WallRays=24;
	UPROPERTY(EditAnywhere, Category="Walls", meta=(Units="s", ClampMin="0.1", ClampMax="1")) float WallRefreshSeconds=.25f;
	bool IsValid() const;
	float Radius(float Age) const;
	float Density(float Age) const;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALSmokeScreenFX
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Resources") TObjectPtr<UNiagaraSystem> JetSystem;
	/** UI-domain cloud material, composed below normal HUD and above all scene render passes. */
	UPROPERTY(EditAnywhere, Category="Resources") TObjectPtr<UMaterialInterface> CloudMaterial;
	UPROPERTY(EditAnywhere, Category="Resources") TObjectPtr<UMaterialInterface> ScreenMaterial;
	UPROPERTY(EditAnywhere, Category="World Smoke", meta=(HideAlphaChannel)) FLinearColor DarkColor=FLinearColor(.012f,.016f,.02f);
	UPROPERTY(EditAnywhere, Category="World Smoke", meta=(HideAlphaChannel)) FLinearColor LightColor=FLinearColor(.16f,.18f,.20f);
	UPROPERTY(EditAnywhere, Category="World Smoke", meta=(ClampMin="0.1", ClampMax="1")) float CloudOpacity=1;
	UPROPERTY(EditAnywhere, Category="World Smoke", meta=(ClampMin="0.1", ClampMax="4")) float FlowSpeed=.7f;
	UPROPERTY(EditAnywhere, Category="Screen", meta=(ClampMin="0", ClampMax="0.95")) float MaximumObscuration=.8f;
	UPROPERTY(EditAnywhere, Category="Screen", meta=(Units="s", ClampMin="0.01", ClampMax="2")) float FadeInSeconds=.2f;
	UPROPERTY(EditAnywhere, Category="Screen", meta=(Units="s", ClampMin="0.01", ClampMax="3")) float FadeOutSeconds=.5f;
	UPROPERTY(EditAnywhere, Category="Jet", meta=(ClampMin="1", ClampMax="100")) float JetRate=30;
	UPROPERTY(EditAnywhere, Category="Jet", meta=(Units="cm/s", ClampMin="20", ClampMax="1000")) float UpwardSpeed=240;
	UPROPERTY(EditAnywhere, Category="Jet", meta=(Units="cm/s", ClampMin="0", ClampMax="500")) float BackwardSpeed=60;
	bool IsValid() const;
};

USTRUCT()
struct HAL_FUTURE_CREATION_API FHALSmokePuff
{
	GENERATED_BODY()
	UPROPERTY() FVector_NetQuantize10 Ground=FVector::ZeroVector;
	UPROPERTY() float BirthAge=0;
	/** World-space radial distances, in cm, at Maximum Radius. Growth is reconstructed from server time. */
	UPROPERTY() TArray<uint16> WallDistances;
	float Boundary(float Angle) const;
	float Strength(const FHALSmokeScreenRules& Rules,float Age,const FVector& Position) const;
};

namespace HALSmoke
{
	HAL_FUTURE_CREATION_API double ServerTime(const UWorld* World);
	HAL_FUTURE_CREATION_API FVector TargetPosition(const AActor& Target);
	/** No Tag/health mutation. Shared authority/presentation geometry and concealment policy. */
	HAL_FUTURE_CREATION_API bool IsConcealed(const AActor* Target);
	HAL_FUTURE_CREATION_API float AdvanceObscuration(float Current,float Desired,float Delta,float InSeconds,float OutSeconds,float FullStrength=.8f);
}
