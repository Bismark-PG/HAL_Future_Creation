#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "HALMovementBoostEffect.generated.h"

/** First movement GE: five game seconds, +0.5 acceleration/+0.25 driving limits.
 * Same definition aggregates across sources on the target and refreshes one layer.
 * Physics consumes the resulting attributes through the movement bridge only. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALMovementBoostEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UHALMovementBoostEffect();
};
