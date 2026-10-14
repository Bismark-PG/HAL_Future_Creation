// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VehicleHitContext.h"
#include "VehicleKnockbackSettings.h"
#include "HALCombatTypes.h"

struct FVehicleHitResolution
{
	bool bResolved = false;
	EVehicleKnockbackTier KnockbackTier = EVehicleKnockbackTier::Light;
	float ImpactSpeed = 0.0f;
};

/** Authority-only shared hit rules; sources own detection, pulses and their action lifetime. */
struct HAL_FUTURE_CREATION_API FCombatResolver
{
	static FHALCombatHitResult ResolveCombatHit(FHALCombatAction& Action, const FHALCombatHitRequest& Request);
	/** Capture only at action acceptance, never from a collision callback or client request. */
	static bool CaptureVehicleAttack(AActor& Source, AActor& Instigator, float ActionMultiplier,
		float ActionMultiplierDelta, FHALAttackSourceSnapshot& OutSnapshot);
	/** One-off instantaneous compatibility adapter. Repeated callbacks must share FHALCombatAction. */
	static FVehicleHitResolution ResolveVehicleHit(
		const FVehicleHitContext& Hit,
		float Damage,
		float KnockbackStrengthMultiplier);
};
