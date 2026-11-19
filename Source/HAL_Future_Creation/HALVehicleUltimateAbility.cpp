#include "HALVehicleUltimateAbility.h"
#include "HALAbilitySystemComponent.h"
#include "HALGameplayTags.h"

bool UHALVehicleUltimateAbility::CheckCost(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayTagContainer* Reasons) const
{
	auto* ASC = Info ? Cast<UHALAbilitySystemComponent>(Info->AbilitySystemComponent.Get()) : nullptr;
	if (ASC && Info->IsNetAuthority()) { ASC->UpdateUltimateBalance(); }
	if (ASC && ASC->CanPayUltimate(GetUltimateCost())) { return true; }
	if (Reasons) { Reasons->AddTag(HALGameplayTags::Ability_Fail_UltimateEnergy); }
	return false;
}

void UHALVehicleUltimateAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	auto* ASC = Info && Info->IsNetAuthority() ? Cast<UHALAbilitySystemComponent>(Info->AbilitySystemComponent.Get()) : nullptr;
	const float Cost = GetUltimateCost();
	const uint32 Reservation = ASC ? ASC->ReserveUltimate(Cost) : 0;
	EHALUltimateStart Result = EHALUltimateStart::Failed;
	if (Reservation)
	{
		if (CommitAbility(Handle, Info, Activation)) { Result = StartUltimate(Handle, Info, Activation, Event); }
		ASC->FinishUltimate(Reservation, Cost, Result != EHALUltimateStart::Failed);
	}
	if (Result != EHALUltimateStart::Active)
	{
		EndAbility(Handle, Info, Activation, true, Result == EHALUltimateStart::Failed);
	}
}
