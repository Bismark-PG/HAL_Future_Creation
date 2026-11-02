#pragma once
#include "CoreMinimal.h"
#include "HALCombatTypes.h"
#include "HALVehicleLifecycleTypes.generated.h"

class ATestVehiclePawn;

/** Saved vehicle seeds, never read back from the outgoing ASC's current values. */
struct HAL_FUTURE_CREATION_API FHALVehicleLifecycleSeed
{
	float MaxHP = 0;
	FVehicleCombatConfig Combat;
	FHALVehicleImpactConfig Impact;
	bool Validate() const;
};

/** Replicated binding identity for explicit transitions. Attribute replication remains GAS-owned. */
USTRUCT()
struct HAL_FUTURE_CREATION_API FHALVehicleLifecycleState
{
	GENERATED_BODY()
	UPROPERTY() uint32 Revision = 0;
	UPROPERTY() uint32 ChangeSerial = 0;
	UPROPERTY() uint32 RoundId = 0;
	UPROPERTY() TObjectPtr<ATestVehiclePawn> Avatar;
};
