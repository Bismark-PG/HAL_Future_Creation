#pragma once
#include "CoreMinimal.h"
class UWorld;
class APawn;

/** GT-only permission snapshot. Avatar/GAS validity and Stun remain separate checks. */
struct HAL_FUTURE_CREATION_API FHALFlowPermissions
{
	bool bRoomEnabled = false;
	bool bDrive = true, bBallControl = true, bAbilities = false, bRecovery = true, bObserve = true;
	bool bWorldInteractions = true, bDamage = true;
};
namespace HALGameFlow
{
	HAL_FUTURE_CREATION_API FHALFlowPermissions Permissions(const UWorld* World, const APawn* Pawn = nullptr);
}
