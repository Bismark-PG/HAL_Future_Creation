#include "HALGameFlowPermissions.h"
#include "HALMatchGameState.h"
#include "HALRoomComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

FHALFlowPermissions HALGameFlow::Permissions(const UWorld* World, const APawn* Pawn)
{
	FHALFlowPermissions P;
	const auto* Match = World ? World->GetGameState<AHALMatchGameState>() : nullptr;
	const auto* Room = Match ? Match->GetRoom() : nullptr;
	if (!Room || !Room->IsEnabled())
	{
		// Preserve legacy: waiting already permits driving/ball/ordinary combat, while abilities and observation have phase checks.
		P.bAbilities = Match && Match->GetMatchPhase() == EHALMatchPhase::Playing;
		P.bRecovery = P.bObserve = !Match || Match->GetMatchPhase() == EHALMatchPhase::Playing;
		return P;
	}
	P.bRoomEnabled = true;
	const auto Phase = Room->GetData().Phase;
	P.bDrive = P.bBallControl = P.bAbilities = P.bRecovery = Room->AllowsPlayerActions(Pawn);
	P.bWorldInteractions = Phase == EHALRoomPhase::Open || Phase == EHALRoomPhase::Playing;
	P.bDamage = Phase == EHALRoomPhase::Playing;
	P.bObserve = Pawn && Pawn->GetController() && Phase != EHALRoomPhase::Returning;
	const auto& M=Room->GetData().Match;
	if (M.bActive && (M.bComplete || Match->GetServerWorldTimeSeconds()>=M.EndServerTime))
	{ P.bDrive=P.bBallControl=P.bAbilities=P.bRecovery=P.bObserve=P.bWorldInteractions=P.bDamage=false; }
	return P;
}
