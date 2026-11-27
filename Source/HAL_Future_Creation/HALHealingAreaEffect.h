#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "HALHealingAreaEffect.generated.h"

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALHealingAreaRecipe
{
	GENERATED_BODY()
	/** HP restored per completed period; no immediate pulse on entering. */
	UPROPERTY(EditAnywhere, Category="Healing", meta=(ClampMin="0.01", ClampMax="10000")) float HealPerTick = 5.f;
	UPROPERTY(EditAnywhere, Category="Healing", meta=(ClampMin="0.1", ClampMax="60", Units="s")) float PeriodSeconds = 1.f;
	bool IsValid() const;
	bool operator==(const FHALHealingAreaRecipe& Other) const { return HealPerTick == Other.HealPerTick && PeriodSeconds == Other.PeriodSeconds; }
};

/** One receiver-owned infinite periodic GE; area sources never own or modify HP themselves. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALHealingAreaEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UHALHealingAreaEffect();
};
