#include "HALDebugSettings.h"
#include "HALMovementBoostEffect.h"
#include "HALBombActor.h"
#include "HALExpandingSlipperyArea.h"
#include "HALExpandingHealingArea.h"

UHALDebugSettings::UHALDebugSettings()
{
	MovementBoostEffectClass = UHALMovementBoostEffect::StaticClass();
	DefaultBombClass = AHALBombActor::StaticClass();
	DefaultSlipperyAreaClass = AHALExpandingSlipperyArea::StaticClass();
	DefaultHealingAreaClass = AHALExpandingHealingArea::StaticClass();
}
