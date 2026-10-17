#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HALDebugSettings.generated.h"

class ABasicBallActor;
class UHALMovementBoostEffect;

/** Development helpers reference authored content; they never replace its tuned values. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "HAL Debug"))
class HAL_FUTURE_CREATION_API UHALDebugSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UHALDebugSettings();

	/** Authority-selected GE. Native five-second default; an optional authored child can tune it. */
	UPROPERTY(Config, EditAnywhere, Category = "Movement")
	TSoftClassPtr<UHALMovementBoostEffect> MovementBoostEffectClass;

	UPROPERTY(Config, EditAnywhere, Category = "Ball")
	TSoftClassPtr<ABasicBallActor> DefaultBallClass;

	/** Horizontal distance from the current vehicle root, along its actual heading. */
	UPROPERTY(Config, EditAnywhere, Category = "Ball", meta = (ClampMin = "1", Units = "cm"))
	float BallForwardDistance = 350.0f;

	/** World-up offset from the vehicle root; the Free ball falls normally after spawning. */
	UPROPERTY(Config, EditAnywhere, Category = "Ball", meta = (ClampMin = "0", Units = "cm"))
	float BallHeightOffset = 100.0f;
};
