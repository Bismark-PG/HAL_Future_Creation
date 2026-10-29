#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "HALMovementAreaEffects.generated.h"

/** Internal selected-area GEs. One handle per family on each Avatar, not one additive GE per area. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALSlowAreaEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UHALSlowAreaEffect();
};

UCLASS()
class HAL_FUTURE_CREATION_API UHALSlipperyAreaEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UHALSlipperyAreaEffect();
};
