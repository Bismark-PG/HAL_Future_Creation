#include "HALVehicleLifecycleTypes.h"
bool FHALVehicleLifecycleSeed::Validate() const
{
	FString Error;
	return FMath::IsFinite(MaxHP) && MaxHP >= 1 && Combat.Validate(Error) && Impact.Validate(Error);
}
