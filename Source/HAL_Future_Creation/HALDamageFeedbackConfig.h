#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HALDamageFeedbackConfig.generated.h"

class UMaterialInterface;

/** Shared look only. Damage/impact resistance and control rules never read this configuration. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALDamageFeedbackStyle
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Rendering") bool bEnabled=true;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Body") FLinearColor BodyColor=FLinearColor(.9f,.025f,.015f);
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Body",meta=(ClampMin="0",ClampMax="1")) float BodyOpacity=.85f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Body",meta=(ClampMin="0.03",ClampMax="1",Units="s")) float BodySeconds=.2f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Body",meta=(ClampMin="0",ClampMax="0.2",Units="s")) float BodyHoldSeconds=.025f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Outline") FLinearColor EdgeColor=FLinearColor(1,.96f,.88f);
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Outline",meta=(ClampMin="0",ClampMax="1")) float WhiteOpacity=.9f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Outline",meta=(ClampMin="0.01",ClampMax="0.3",Units="s")) float WhiteSeconds=.065f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Outline",meta=(ClampMin="0",ClampMax="8")) float OutlinePixels=2;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Outline",meta=(ClampMin="0.1",ClampMax="2")) float OutlineSoftness=.5f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="RGB",meta=(ClampMin="0",ClampMax="1")) float RGBOpacity=.6f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="RGB",meta=(ClampMin="0.01",ClampMax="0.3",Units="s")) float RGBSeconds=.09f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="RGB",meta=(ClampMin="0",ClampMax="12")) float SeparationPixels=3;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="RGB") FVector2D SeparationDirection=FVector2D(1,.25);
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Repeated Hits",meta=(ClampMin="0.05",ClampMax="1",Units="s")) float MinimumAccentInterval=.14f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Repeated Hits",meta=(ClampMin="0.1",ClampMax="2",Units="s")) float MaximumContinuousSeconds=.55f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Repeated Hits",meta=(ClampMin="0.05",ClampMax="1",Units="s")) float RestSeconds=.15f;
	/** Reject stale network/asset-preparation events; an accepted pulse starts locally at receipt so short accents survive latency. */
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Network",meta=(ClampMin="0.1",ClampMax="2",Units="s")) float MaximumEventAge=.5f;
	/** Reserved project stencil range. Existing unrelated stencil users are never overwritten. */
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Render Budget",meta=(ClampMin="1",ClampMax="248")) int32 FirstStencil=240;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Render Budget",meta=(ClampMin="1",ClampMax="8")) int32 MaximumTargets=8;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Visibility",meta=(ClampMin="0",ClampMax="10",Units="cm")) float DepthTolerance=1;
	bool IsValid() const;
};

UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALDamageFeedbackConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Rendering") TObjectPtr<UMaterialInterface> Material;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Damage Feedback") FHALDamageFeedbackStyle Style;
	bool Validate(FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/** Transient authority result, not a replicated persistent buff or a client-reported hit. */
USTRUCT()
struct FHALDamageFeedbackPulse
{
	GENERATED_BODY()
	UPROPERTY() uint32 Sequence=0;
	UPROPERTY() FGuid AttackId;
	UPROPERTY() uint32 AttackPulse=0;
	UPROPERTY() double ServerTime=0;
	UPROPERTY() float ActualDamage=0;
};

/** One bounded playback per target; no accumulation of materials or damage-event timers. */
struct HAL_FUTURE_CREATION_API FHALDamageFeedbackPlayback
{
	double BurstStart=-1,LastBody=-1,LastAccent=-1;
	bool Receive(const FHALDamageFeedbackStyle& Style,double Time);
	FVector Weights(const FHALDamageFeedbackStyle& Style,double Now) const;
	double RetireAt(const FHALDamageFeedbackStyle& Style) const;
};
