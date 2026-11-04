#include "HALAbilityTypes.h"
#include "HAL/IConsoleManager.h"
#include "Abilities/GameplayAbility.h"
#include "HALGameplayTags.h"

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

bool HALAbilities::ValidateGrants(const TArray<FHALAbilityGrant>& Grants,FString& Error)
{
	TSet<FGameplayTag> Inputs;
	for (const auto& G:Grants)
	{
		if (!G.Ability || G.Ability->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)
			|| !G.InputTag.IsValid() || G.Level<1 || Inputs.Contains(G.InputTag))
		{ Error=TEXT("Ability grants need concrete GA classes, valid unique input tags and Level >= 1."); return false; }
		Inputs.Add(G.InputTag);
	}
	return true;
}
bool FHALVehicleAbilityConfig::Validate(FString& Error) const
{
	if (Version==EHALVehicleAbilityVersion::LegacyPlayerState)
	{
		if (!Grants.IsEmpty()) { Error=TEXT("Legacy Player State mode must not contain ignored vehicle Grants; select Vehicle Definition V1."); return false; }
		return true;
	}
	if (Version!=EHALVehicleAbilityVersion::VehicleDefinitionV1)
	{ Error=TEXT("Unknown vehicle ability version."); return false; }
	if (!HALAbilities::ValidateGrants(Grants,Error)) { return false; }
	if (Grants.Num()!=1 || Grants[0].InputTag!=HALGameplayTags::Input_Ability_Primary)
	{ Error=TEXT("Vehicle Definition V1 currently requires exactly one Input.Ability.Primary grant. No legacy fallback."); return false; }
	return true;
}
