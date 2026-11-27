#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "HALApplyDamageEffect.generated.h"

/** Instant HP result. Damage amount and source are supplied by the authority resolver. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALApplyDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UHALApplyDamageEffect();
};

/** Charged-ball hit result; retains the common HP modifier and attribution pipeline. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALApplyElectrifiedBallDamageEffect : public UHALApplyDamageEffect
{
	GENERATED_BODY()
public:
	UHALApplyElectrifiedBallDamageEffect();
};
