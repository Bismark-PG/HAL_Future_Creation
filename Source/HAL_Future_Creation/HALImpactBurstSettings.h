#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HALImpactBurstSettings.generated.h"

class UMaterialInterface;

/** Comic impact burst: one camera-facing procedural sprite per accepted impact, drawn at the contact point.
 * Presentation only. Nothing here gates damage, impact, ball rules or the authority result. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALImpactBurstConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Impact Burst") bool bEnabled = true;
	/** Unlit translucent sprite material. None keeps the whole path inert. */
	UPROPERTY(EditAnywhere, Category="Impact Burst") TSoftObjectPtr<UMaterialInterface> Material;
	/** Authored push intent mapped to burst strength 0..1; below the minimum still shows the smallest burst. */
	UPROPERTY(EditAnywhere, Category="Strength", meta=(ClampMin="1", ClampMax="5000", Units="cm/s")) float MinimumPushSpeed = 150;
	UPROPERTY(EditAnywhere, Category="Strength", meta=(ClampMin="1", ClampMax="5000", Units="cm/s")) float MaximumPushSpeed = 900;
	UPROPERTY(EditAnywhere, Category="Size", meta=(ClampMin="10", ClampMax="2000", Units="cm")) float MinimumSizeCm = 90;
	UPROPERTY(EditAnywhere, Category="Size", meta=(ClampMin="10", ClampMax="2000", Units="cm")) float MaximumSizeCm = 240;
	/** Spike count at zero and full strength. The material floors this to whole lobes. */
	UPROPERTY(EditAnywhere, Category="Size", meta=(ClampMin="3", ClampMax="24")) float MinimumSpikes = 7;
	UPROPERTY(EditAnywhere, Category="Size", meta=(ClampMin="3", ClampMax="24")) float MaximumSpikes = 13;
	UPROPERTY(EditAnywhere, Category="Envelope", meta=(ClampMin="0.05", ClampMax="2", Units="s")) float LifetimeSeconds = .22f;
	/** Added to the lifetime at full strength so heavy hits linger a little longer. */
	UPROPERTY(EditAnywhere, Category="Envelope", meta=(ClampMin="0", ClampMax="1", Units="s")) float StrengthLifetimeSeconds = .06f;
	UPROPERTY(EditAnywhere, Category="Color") FLinearColor LightColor = FLinearColor(1.f, .95f, .78f, 1.f);
	UPROPERTY(EditAnywhere, Category="Color") FLinearColor MediumColor = FLinearColor(1.f, .72f, .18f, 1.f);
	UPROPERTY(EditAnywhere, Category="Color") FLinearColor HeavyColor = FLinearColor(1.f, .32f, .06f, 1.f);
	UPROPERTY(EditAnywhere, Category="Color") FLinearColor FillColor = FLinearColor::White;
	/** Pushed off the contact surface, then lifted toward the top-down camera so the body never splits it. */
	UPROPERTY(EditAnywhere, Category="Placement", meta=(ClampMin="0", ClampMax="200", Units="cm")) float SurfaceOffsetCm = 10;
	UPROPERTY(EditAnywhere, Category="Placement", meta=(ClampMin="0", ClampMax="400", Units="cm")) float VerticalOffsetCm = 20;
	/** A contact point further than this from the target is replaced by a body-edge fallback. */
	UPROPERTY(EditAnywhere, Category="Placement", meta=(ClampMin="50", ClampMax="2000", Units="cm")) float MaximumPointDistanceCm = 400;
	/** Used only when the authority point is unusable: the target's own edge on the source side. */
	UPROPERTY(EditAnywhere, Category="Placement", meta=(ClampMin="0", ClampMax="500", Units="cm")) float FallbackOffsetCm = 90;
	/** Per vehicle. The oldest burst is retired when a new one arrives at the cap. */
	UPROPERTY(EditAnywhere, Category="Budget", meta=(ClampMin="1", ClampMax="16")) int32 MaximumConcurrent = 4;
	/** Unknown accepted events older than this are discarded; never replayed on late join. */
	UPROPERTY(EditAnywhere, Category="Network", meta=(ClampMin="0.05", ClampMax="2", Units="s")) float MaximumEventAgeSeconds = .5f;
	UPROPERTY(EditAnywhere, Category="Material Parameters") FName AgeParameter = TEXT("FX_Age01");
	UPROPERTY(EditAnywhere, Category="Material Parameters") FName StrengthParameter = TEXT("FX_Strength");
	UPROPERTY(EditAnywhere, Category="Material Parameters") FName SeedParameter = TEXT("FX_Seed");
	UPROPERTY(EditAnywhere, Category="Material Parameters") FName SpikesParameter = TEXT("FX_Spikes");
	UPROPERTY(EditAnywhere, Category="Material Parameters") FName FillColorParameter = TEXT("FX_FillColor");
	UPROPERTY(EditAnywhere, Category="Material Parameters") FName EdgeColorParameter = TEXT("FX_EdgeColor");
	bool Validate(FString& Error) const;
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Vehicle Impact Burst FX"))
class HAL_FUTURE_CREATION_API UHALImpactBurstSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(Config, EditAnywhere, Category="Impact Burst") FHALImpactBurstConfig Burst;
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
#if WITH_EDITOR
	/** Cook-only synchronous validation. Runtime impacts never load the sprite material synchronously. */
	bool CollectCookPackages(TArray<FName>& Packages, FString& Error) const;
#endif
};
