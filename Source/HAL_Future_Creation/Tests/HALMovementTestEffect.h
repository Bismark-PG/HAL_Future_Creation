#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "HALMovementTestEffect.generated.h"

/** Native fixture for opt-in bridge tests. Not the configurable phase10-F-3 boost. */
UCLASS(NotBlueprintable, Transient)
class UHALMovementBridgeTestEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UHALMovementBridgeTestEffect();
};
