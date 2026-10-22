#include "HALDebugSettings.h"
#include "HALMovementBoostEffect.h"
#include "HALBombActor.h"

UHALDebugSettings::UHALDebugSettings()
{
	MovementBoostEffectClass = UHALMovementBoostEffect::StaticClass();
	DefaultBombClass = AHALBombActor::StaticClass();
}
