// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VehicleHitContext.h"
#include "VehicleKnockbackSettings.h"

struct FVehicleHitResolution
{
	bool bResolved = false;
	EVehicleKnockbackTier KnockbackTier = EVehicleKnockbackTier::Light;
	float ImpactSpeed = 0.0f;
};

/** Narrow, authority-only rule entry point; later damage systems can replace its HP adapter. */
struct HAL_FUTURE_CREATION_API FCombatResolver
{
	/** Resolves one damage result and applies its globally configured tier outcome. */
	static FVehicleHitResolution ResolveVehicleHit(
		const FVehicleHitContext& Hit,
		float Damage,
		float KnockbackStrengthMultiplier);
};
