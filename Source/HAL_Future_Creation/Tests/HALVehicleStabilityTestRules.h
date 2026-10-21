#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "../VehicleStabilityRules.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

inline UVehicleStabilityRules* MakeHALStabilityTestRules(FName Name = NAME_None)
{
	auto* Rules = NewObject<UVehicleStabilityRules>(GetTransientPackage(), Name);
	Rules->bReadyForUse = true;
	Rules->VehiclePhysicalMaterial = DuplicateObject<UPhysicalMaterial>(
		LoadObject<UPhysicalMaterial>(nullptr, TEXT("/Engine/EngineMaterials/DefaultPhysicalMaterial.DefaultPhysicalMaterial")), Rules, TEXT("ContactMaterial"));
	Rules->VehiclePhysicalMaterial->Restitution = 0;
	Rules->VehiclePhysicalMaterial->bOverrideRestitutionCombineMode = true;
	Rules->VehiclePhysicalMaterial->RestitutionCombineMode = EFrictionCombineMode::Min;
	return Rules;
}

/** Only runtime memory; never writes a settings file or asset. Restore after destroying the test world. */
struct FScopedHALStabilityTestRules
{
	UVehicleStabilitySettings* Settings = GetMutableDefault<UVehicleStabilitySettings>();
	bool bOldUse = Settings->bUseSharedRules;
	TSoftObjectPtr<UVehicleStabilityRules> OldAsset = Settings->SharedRules;
	FScopedHALStabilityTestRules(bool bEnable)
	{
		Settings->bUseSharedRules = bEnable;
		if (bEnable) { Settings->SharedRules = MakeHALStabilityTestRules(); }
	}
	~FScopedHALStabilityTestRules() { Settings->bUseSharedRules = bOldUse; Settings->SharedRules = OldAsset; }
};
#endif
