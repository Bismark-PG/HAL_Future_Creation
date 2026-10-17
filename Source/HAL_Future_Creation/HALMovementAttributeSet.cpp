#include "HALMovementAttributeSet.h"
#include "Net/UnrealNetwork.h"
void UHALMovementAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UHALMovementAttributeSet, AccelerationMultiplierDelta, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHALMovementAttributeSet, SpeedLimitMultiplierDelta, COND_None, REPNOTIFY_Always);
}
void UHALMovementAttributeSet::RejectNonFinite(const FGameplayAttribute& Attribute, float& Value) const
{
	if (!FMath::IsFinite(Value)) { Value = Attribute == GetAccelerationMultiplierDeltaAttribute() ? GetAccelerationMultiplierDelta() : GetSpeedLimitMultiplierDelta(); }
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
