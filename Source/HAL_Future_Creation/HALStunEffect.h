#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "HALStunEffect.generated.h"

/** Shared Avatar-scoped stun; sources supply game seconds through ASC::ApplyStun. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALStunEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UHALStunEffect();
};
