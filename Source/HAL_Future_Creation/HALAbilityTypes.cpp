#include "HALAbilityTypes.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogHALAbility);
#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarHALAbilityLog(TEXT("hal.AbilityLog"), 0,
	TEXT("Log GAS ability requests, accepted activations, effects and Avatar cleanup. 0=off, 1=on."));
#endif

bool HALAbilities::ShouldLog()
{
#if !UE_BUILD_SHIPPING
	return CVarHALAbilityLog.GetValueOnGameThread() > 0;
#else
	return false;
#endif
}
