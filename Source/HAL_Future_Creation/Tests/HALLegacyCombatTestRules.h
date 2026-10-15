#pragma once

#include "../VehicleKnockbackSettings.h"

/** Legacy fixtures must not inherit a team's migrated DA. In-memory only, restored before leaving. */
struct FScopedHALLegacyCombatTestRules
{
	UVehicleKnockbackSettings* Settings = GetMutableDefault<UVehicleKnockbackSettings>();
	EVehicleConfigurationSource PreviousSource = Settings->ConfigurationSource;
	FScopedHALLegacyCombatTestRules() { Settings->ConfigurationSource = EVehicleConfigurationSource::Legacy; }
	~FScopedHALLegacyCombatTestRules()
	{
		Settings->ConfigurationSource = PreviousSource;
		Settings->InitializeRules();
	}
};
