// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VehicleHitContext.h"

/** Narrow, authority-only rule entry point; later damage systems can replace its HP adapter. */
struct HAL_FUTURE_CREATION_API FCombatResolver
{
	/** Returns true only when one vehicle damage result was actually applied. */
	static bool ResolveVehicleHit(const FVehicleHitContext& Hit, float Damage, float AdditionalImpulse);
};
