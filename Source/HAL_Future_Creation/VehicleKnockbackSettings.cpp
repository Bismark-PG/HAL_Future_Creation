// Copyright Epic Games, Inc. All Rights Reserved.

#include "VehicleKnockbackSettings.h"
#include "UObject/UnrealType.h"

UVehicleKnockbackSettings::UVehicleKnockbackSettings()
{
	LightTier.HorizontalDeltaSpeed = 250.0f;
	LightTier.TargetVerticalSpeed = 0.0f;
	LightTier.AirborneFlipTurns = 0.0f;

	MediumTier.HorizontalDeltaSpeed = 600.0f;
	MediumTier.TargetVerticalSpeed = 500.0f;
	MediumTier.AirborneFlipTurns = 1.0f;

	HeavyTier.HorizontalDeltaSpeed = 900.0f;
	HeavyTier.TargetVerticalSpeed = 750.0f;
	HeavyTier.AirborneFlipTurns = 1.25f;
}

bool UVehicleKnockbackSettings::InitializeRules(UWorld* World)
{
	if (ConfigurationSource == EVehicleConfigurationSource::Legacy) { return true; }
	if (bRulesInitialized && CachedWorld.Get() == World) { return bRulesValid; }
	bRulesInitialized = true;
	CachedWorld = World;
	bRulesValid = false;
	CachedDefinition = CombatRules.LoadSynchronous();
	FString Error;
	if (ConfigurationSource != EVehicleConfigurationSource::Definition || !CachedDefinition || !CachedDefinition->Validate(Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid CombatRules %s: %s. Gameplay blocked; no default fallback."), *CombatRules.ToString(), *Error);
		return false;
	}
	CachedRules = CachedDefinition->BallImpacts;
	bRulesValid = true;
	return true;
}

bool UVehicleKnockbackSettings::GetRuntimeRules(FVehicleKnockbackConfig& OutRules) const
{
	if (ConfigurationSource == EVehicleConfigurationSource::Definition)
	{
		if (!bRulesInitialized || !bRulesValid) { return false; }
		OutRules = CachedRules;
		return true;
	}
	if (ConfigurationSource != EVehicleConfigurationSource::Legacy) { return false; }
	OutRules.MediumImpactSpeedThreshold = MediumImpactSpeedThreshold;
	OutRules.HeavyImpactSpeedThreshold = HeavyImpactSpeedThreshold;
	OutRules.LightTier = LightTier;
	OutRules.MediumTier = MediumTier;
	OutRules.HeavyTier = HeavyTier;
	OutRules.MaxAirborneAngularSpeed = MaxAirborneAngularSpeed;
	OutRules.GroundProbeExtraDistance = GroundProbeExtraDistance;
	return true;
}

#if WITH_EDITOR
void UVehicleKnockbackSettings::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	bRulesInitialized = false;
	bRulesValid = false;
	CachedDefinition = nullptr;
	Super::PostEditChangeProperty(Event);
}

bool UVehicleKnockbackSettings::CanEditChange(const FProperty* Property) const
{
	if (ConfigurationSource == EVehicleConfigurationSource::Definition && Property
		&& Property->GetOwnerClass() == StaticClass() && Property->GetFName() != GET_MEMBER_NAME_CHECKED(UVehicleKnockbackSettings, ConfigurationSource)
		&& Property->GetFName() != GET_MEMBER_NAME_CHECKED(UVehicleKnockbackSettings, CombatRules)) { return false; }
	return Super::CanEditChange(Property);
}
#endif
