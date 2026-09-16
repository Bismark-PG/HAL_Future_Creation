// Copyright Epic Games, Inc. All Rights Reserved.

#include "VehicleKnockbackSettings.h"

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

EVehicleKnockbackTier UVehicleKnockbackSettings::SelectTier(const float ImpactSpeed) const
{
	const float MediumThreshold = FMath::Max(0.0f, MediumImpactSpeedThreshold);
	const float HeavyThreshold = FMath::Max(MediumThreshold, HeavyImpactSpeedThreshold);
	if (ImpactSpeed >= HeavyThreshold)
	{
		return EVehicleKnockbackTier::Heavy;
	}
	if (ImpactSpeed >= MediumThreshold)
	{
		return EVehicleKnockbackTier::Medium;
	}
	return EVehicleKnockbackTier::Light;
}

const FVehicleKnockbackTierDefinition& UVehicleKnockbackSettings::GetTierDefinition(
	const EVehicleKnockbackTier Tier) const
{
	switch (Tier)
	{
	case EVehicleKnockbackTier::Heavy:
		return HeavyTier;
	case EVehicleKnockbackTier::Medium:
		return MediumTier;
	case EVehicleKnockbackTier::Light:
	default:
		return LightTier;
	}
}
