#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "HALHealingAreaGizmoComponent.generated.h"

/** Editor visualization anchor only. No collision, physics, replication or gameplay state. */
UCLASS(NotBlueprintable)
class HAL_FUTURE_CREATION_API UHALHealingAreaGizmoComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	UHALHealingAreaGizmoComponent();
};
