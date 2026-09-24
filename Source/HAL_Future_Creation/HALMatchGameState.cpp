// Copyright Epic Games, Inc. All Rights Reserved.

#include "HALMatchGameState.h"

#include "Net/UnrealNetwork.h"

void AHALMatchGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHALMatchGameState, MatchPhase);
}

void AHALMatchGameState::SetMatchPhase(const EHALMatchPhase NewPhase)
{
	if (!HasAuthority() || MatchPhase == NewPhase)
	{
		return;
	}

	MatchPhase = NewPhase;
	OnRep_MatchPhase();
	ForceNetUpdate();
}

void AHALMatchGameState::OnRep_MatchPhase()
{
	OnMatchPhaseChanged.Broadcast(MatchPhase);
}
