#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HALMatchFeverDefinition.generated.h"

class USoundBase;

/** Shared pacing and broadcast cue for the closing stretch of a formal match. */
UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALMatchFeverDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	/** Start Fever when the running match has at most this many game seconds left. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trigger", meta=(Units="s", ClampMin="0", ClampMax="3600"))
	float RemainingSeconds = 90.f;
	/** Multiplies spring-punch cooldown and the generator's roll and return durations. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pacing", meta=(ClampMin="0.05", ClampMax="4"))
	float GimmickWaitMultiplier = .75f;
	/** Multiplies the shared time between special-ball drops, reserving the full warning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pacing", meta=(ClampMin="0.05", ClampMax="4"))
	float SpecialBallIntervalMultiplier = .75f;
	/** Multiplies natural energy earned per second while driving. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pacing", meta=(ClampMin="0.05", ClampMax="8"))
	float NaturalEnergyRateMultiplier = 1.25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation", meta=(Units="s", ClampMin="0.1", ClampMax="5"))
	float BannerSeconds = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
	TObjectPtr<USoundBase> CrowdCheer;

	bool Validate(FString& Error) const;
};
