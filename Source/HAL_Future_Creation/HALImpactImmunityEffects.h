#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "HALImpactImmunityEffects.generated.h"

/** Eight-second default; same-definition applications refresh on this target, never add time. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALImpactImmunityEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UHALImpactImmunityEffect();
};

/** Separate owner-scoped cooldown. Removing immunity does not refund the cooldown. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALImpactImmunityCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UHALImpactImmunityCooldownEffect();
};

/** HALImpactDebug's independent source; native GE keeps Mixed tag replication consistent with gameplay GEs. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALDebugImpactImmunityEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UHALDebugImpactImmunityEffect();
};
