#include "HALMovementAreaTypes.h"

bool FHALMovementAreaConfig::IsValid() const
{
	const auto ValidMultiplier = [](float Value) { return FMath::IsFinite(Value) && Value >= 0 && Value <= 1; };
	return (Kind == EHALMovementAreaKind::Slow || Kind == EHALMovementAreaKind::Slippery)
		&& ValidMultiplier(AccelerationMultiplier) && ValidMultiplier(SpeedLimitMultiplier) && ValidMultiplier(LateralGripMultiplier)
		&& FMath::IsFinite(RecoverySeconds) && RecoverySeconds >= 0 && RecoverySeconds <= 10;
}
FVector FHALMovementAreaConfig::GetScales() const
{
	return Kind == EHALMovementAreaKind::Slow ? FVector(AccelerationMultiplier, SpeedLimitMultiplier, 1)
		: FVector(1, 1, LateralGripMultiplier);
}
void FHALMovementAreaSelection::Include(const FHALMovementAreaConfig& Config)
{
	if (!Config.IsValid()) { return; }
	const FVector Incoming = Config.GetScales();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (Incoming[Index] < Scales[Index])
		{
			Scales[Index] = Incoming[Index]; RecoverySeconds[Index] = Config.RecoverySeconds;
		}
		else if (Incoming[Index] == Scales[Index] && Incoming[Index] < 1)
		{
			RecoverySeconds[Index] = FMath::Max(RecoverySeconds[Index], double(Config.RecoverySeconds));
		}
	}
}
