#include "HALMovementAttributeSet.h"
#include "Net/UnrealNetwork.h"
void UHALMovementAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UHALMovementAttributeSet, AccelerationMultiplierDelta, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHALMovementAttributeSet, SpeedLimitMultiplierDelta, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHALMovementAttributeSet, LateralGripMultiplierDelta, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHALMovementAttributeSet, AreaAccelerationDelta, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHALMovementAttributeSet, AreaSpeedDelta, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHALMovementAttributeSet, AreaGripDelta, COND_None, REPNOTIFY_Always);
}
void UHALMovementAttributeSet::RejectNonFinite(const FGameplayAttribute& Attribute, float& Value) const
{
	if (!FMath::IsFinite(Value)) { Value = Attribute.GetNumericValue(this); }
}
void UHALMovementAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& Value)
{
	Super::PreAttributeChange(Attribute, Value); RejectNonFinite(Attribute, Value);
}
void UHALMovementAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& Value) const
{
	Super::PreAttributeBaseChange(Attribute, Value); RejectNonFinite(Attribute, Value);
}
void UHALMovementAttributeSet::OnRep_AccelerationDelta(const FGameplayAttributeData& Old) { GAMEPLAYATTRIBUTE_REPNOTIFY(UHALMovementAttributeSet, AccelerationMultiplierDelta, Old); }
void UHALMovementAttributeSet::OnRep_SpeedDelta(const FGameplayAttributeData& Old) { GAMEPLAYATTRIBUTE_REPNOTIFY(UHALMovementAttributeSet, SpeedLimitMultiplierDelta, Old); }
void UHALMovementAttributeSet::OnRep_GripDelta(const FGameplayAttributeData& Old) { GAMEPLAYATTRIBUTE_REPNOTIFY(UHALMovementAttributeSet, LateralGripMultiplierDelta, Old); }
void UHALMovementAttributeSet::OnRep_AreaAcceleration(const FGameplayAttributeData& Old) { GAMEPLAYATTRIBUTE_REPNOTIFY(UHALMovementAttributeSet, AreaAccelerationDelta, Old); }
void UHALMovementAttributeSet::OnRep_AreaSpeed(const FGameplayAttributeData& Old) { GAMEPLAYATTRIBUTE_REPNOTIFY(UHALMovementAttributeSet, AreaSpeedDelta, Old); }
void UHALMovementAttributeSet::OnRep_AreaGrip(const FGameplayAttributeData& Old) { GAMEPLAYATTRIBUTE_REPNOTIFY(UHALMovementAttributeSet, AreaGripDelta, Old); }
