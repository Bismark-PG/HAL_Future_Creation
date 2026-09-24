// Copyright Epic Games, Inc. All Rights Reserved.

#include "HALPlayerController.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "HALMatchGameMode.h"
#include "HALMatchGameState.h"

AHALPlayerController::AHALPlayerController()
{
	bAutoManageActiveCameraTarget = true;
}

void AHALPlayerController::RequestStartMatch()
{
	if (!IsLocalController())
	{
		return;
	}

	if (HasAuthority())
	{
		if (AHALMatchGameMode* MatchMode = GetWorld()->GetAuthGameMode<AHALMatchGameMode>())
		{
			MatchMode->TryStartMatch(this);
		}
	}
	else
	{
		ServerRequestStartMatch();
	}
}

void AHALPlayerController::HALStartMatch()
{
	RequestStartMatch();
}

void AHALPlayerController::HALMatchStatus()
{
	const AHALMatchGameState* MatchState = GetWorld()->GetGameState<AHALMatchGameState>();
	const TCHAR* PhaseName = !MatchState ? TEXT("Unavailable")
		: MatchState->GetMatchPhase() == EHALMatchPhase::Playing ? TEXT("Playing") : TEXT("WaitingForPlayers");
	const FString Status = FString::Printf(
		TEXT("HAL Match: Local=%d Authority=%d Pawn=%s PlayerId=%d Phase=%s"),
		IsLocalController() ? 1 : 0,
		HasAuthority() ? 1 : 0,
		*GetNameSafe(GetPawn()),
		PlayerState ? PlayerState->GetPlayerId() : INDEX_NONE,
		PhaseName);
	UE_LOG(LogTemp, Display, TEXT("%s"), *Status);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, Status);
	}
}

void AHALPlayerController::ServerRequestStartMatch_Implementation()
{
	if (AHALMatchGameMode* MatchMode = GetWorld()->GetAuthGameMode<AHALMatchGameMode>())
	{
		MatchMode->TryStartMatch(this);
	}
}
