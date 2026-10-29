#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "HALShockedEffect.generated.h"

/** Presentation marker only. Authority removes it when the current Stun ends. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALShockedEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UHALShockedEffect();
};
