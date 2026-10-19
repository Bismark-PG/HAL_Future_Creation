#pragma once

#if WITH_DEV_AUTOMATION_TESTS
#include "../HALMovementBoostAbility.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALGameplayTags.h"
#include "../HALPlayerState.h"
#include "UObject/UnrealType.h"

namespace HALMovementAbilityTests
{
	/** Emulates the team's DefaultAbilities edit on a runtime instance only; never saves a Blueprint/CDO. */
	inline bool ConfigurePrimary(AHALPlayerState* State)
	{
		if (!State || !State->HasAuthority()) { return false; }
		auto* Property = FindFProperty<FArrayProperty>(State->GetClass(), TEXT("DefaultAbilities"));
		if (!Property) { return false; }
		auto& Grants = *Property->ContainerPtrToValuePtr<TArray<FHALAbilityGrant>>(State);
		Grants.RemoveAll([](const auto& Grant) { return Grant.InputTag == HALGameplayTags::Input_Ability_Primary; });
		auto& Primary = Grants.AddDefaulted_GetRef();
		Primary.Ability = UHALMovementBoostAbility::StaticClass(); Primary.InputTag = HALGameplayTags::Input_Ability_Primary; Primary.Level = 1;
		auto* ASC = State->GetHALAbilitySystemComponent();
		TArray<FGameplayAbilitySpecHandle> Remove;
		for (const auto& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.GetDynamicSpecSourceTags().HasTagExact(HALGameplayTags::Input_Ability_Primary)) { Remove.Add(Spec.Handle); }
		}
		for (const auto Handle : Remove) { ASC->ClearAbility(Handle); }
		if (ASC->GetAvatarActor()) { State->GrantVehicleAbilities(); State->ForceNetUpdate(); }
		return true;
	}
	inline bool HasMovementPrimary(const UHALAbilitySystemComponent* ASC)
	{
		int32 Matches = 0; bool bMovement = false;
		for (const auto& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.GetDynamicSpecSourceTags().HasTagExact(HALGameplayTags::Input_Ability_Primary))
			{
				++Matches; bMovement = Spec.Ability && Spec.Ability->IsA<UHALMovementBoostAbility>();
			}
		}
		return Matches == 1 && bMovement;
	}
}
#endif
