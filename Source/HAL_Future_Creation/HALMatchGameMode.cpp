// Copyright Epic Games, Inc. All Rights Reserved.

#include "HALMatchGameMode.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HALMatchGameState.h"
#include "HALPlayerController.h"
#include "HALPlayerStart.h"
#include "HALPlayerState.h"
#include "TestVehiclePawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogHALMatch, Log, All);

AHALMatchGameMode::AHALMatchGameMode()
{
	GameStateClass = AHALMatchGameState::StaticClass();
	PlayerControllerClass = AHALPlayerController::StaticClass();
	PlayerStateClass = AHALPlayerState::StaticClass();
	DefaultPawnClass = nullptr; // The map's GameMode Blueprint selects the tuned vehicle Blueprint.
	bStartPlayersAsSpectators = false;
}

bool AHALMatchGameMode::CountValidSpawnSlots(int32& OutCount) const
{
	OutCount = 0;
	bool bFound[4] = { false, false, false, false };
	for (TActorIterator<AHALPlayerStart> It(GetWorld()); It; ++It)
	{
		const int32 Slot = It->SpawnSlot;
		if (Slot < 0 || Slot >= UE_ARRAY_COUNT(bFound) || bFound[Slot])
		{
			UE_LOG(LogHALMatch, Error, TEXT("Invalid or duplicate HALPlayerStart slot %d at %s."), Slot, *GetNameSafe(*It));
			return false;
		}
		bFound[Slot] = true;
		++OutCount;
	}
	return true;
}

void AHALMatchGameMode::PreLogin(
	const FString& Options,
	const FString& Address,
	const FUniqueNetIdRepl& UniqueId,
	FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	const AHALMatchGameState* MatchState = GetGameState<AHALMatchGameState>();
	if (!MatchState)
	{
		ErrorMessage = TEXT("The network GameState is unavailable or misconfigured.");
		return;
	}
	if (MatchState->GetMatchPhase() != EHALMatchPhase::WaitingForPlayers)
	{
		ErrorMessage = TEXT("The match has already started.");
		return;
	}

	if (!DefaultPawnClass || !DefaultPawnClass->IsChildOf(ATestVehiclePawn::StaticClass()))
	{
		ErrorMessage = TEXT("The network GameMode needs the configured vehicle Pawn class.");
		return;
	}
	if (MinimumPlayersToStart < 1 || MaximumPlayers < MinimumPlayersToStart || MaximumPlayers > 4)
	{
		ErrorMessage = TEXT("Invalid network player count settings.");
		return;
	}

	int32 SpawnSlotCount = 0;
	if (!CountValidSpawnSlots(SpawnSlotCount) || SpawnSlotCount < MinimumPlayersToStart)
	{
		ErrorMessage = TEXT("Configure unique HALPlayerStart slots before joining.");
		return;
	}

	if (GetNumPlayers() >= FMath::Min(MaximumPlayers, SpawnSlotCount))
	{
		ErrorMessage = TEXT("The match is full.");
	}
}

AActor* AHALMatchGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	AHALPlayerStart* Starts[4] = { nullptr, nullptr, nullptr, nullptr };
	bool bReserved[4] = { false, false, false, false };

	for (TActorIterator<AHALPlayerStart> It(GetWorld()); It; ++It)
	{
		const int32 Slot = It->SpawnSlot;
		if (Slot < 0 || Slot >= UE_ARRAY_COUNT(Starts) || Starts[Slot])
		{
			UE_LOG(LogHALMatch, Error, TEXT("Cannot choose spawn: invalid or duplicate slot %d."), Slot);
			return nullptr;
		}
		Starts[Slot] = *It;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Existing = It->Get();
		const AHALPlayerStart* ReservedStart = Existing && Existing != Player
			? Cast<AHALPlayerStart>(Existing->StartSpot.Get()) : nullptr;
		if (ReservedStart && ReservedStart->SpawnSlot >= 0 && ReservedStart->SpawnSlot < UE_ARRAY_COUNT(bReserved))
		{
			bReserved[ReservedStart->SpawnSlot] = true;
		}
	}

	for (int32 Slot = 0; Slot < UE_ARRAY_COUNT(Starts); ++Slot)
	{
		if (Starts[Slot] && !bReserved[Slot])
		{
			return Starts[Slot];
		}
	}

	UE_LOG(LogHALMatch, Warning, TEXT("No unreserved HALPlayerStart is available for %s."), *GetNameSafe(Player));
	return nullptr;
}

void AHALMatchGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	UE_LOG(LogHALMatch, Log, TEXT("Player joined: %s, Pawn=%s, Start=%s."),
		*GetNameSafe(NewPlayer), *GetNameSafe(NewPlayer ? NewPlayer->GetPawn() : nullptr),
		*GetNameSafe(NewPlayer ? NewPlayer->StartSpot.Get() : nullptr));
	if (!NewPlayer || !NewPlayer->GetPawn())
	{
		UE_LOG(LogHALMatch, Error, TEXT("Player joined without a vehicle Pawn; check the selected Blueprint and spawn collision."));
	}
}

void AHALMatchGameMode::Logout(AController* Exiting)
{
	APawn* LeavingPawn = Exiting ? Exiting->GetPawn() : nullptr;
	if (Exiting)
	{
		Exiting->UnPossess();
		Exiting->StartSpot.Reset();
	}
	if (IsValid(LeavingPawn))
	{
		LeavingPawn->Destroy();
	}
	UE_LOG(LogHALMatch, Log, TEXT("Player left: %s."), *GetNameSafe(Exiting));
	Super::Logout(Exiting);
}

bool AHALMatchGameMode::TryStartMatch(AHALPlayerController* Requestor)
{
	AHALMatchGameState* MatchState = GetGameState<AHALMatchGameState>();
	if (!Requestor || Requestor->GetWorld() != GetWorld() || !Requestor->IsLocalController()
		|| !Requestor->GetPawn() || GetNetMode() != NM_ListenServer
		|| !MatchState || MatchState->GetMatchPhase() != EHALMatchPhase::WaitingForPlayers)
	{
		UE_LOG(LogHALMatch, Warning, TEXT("Start request rejected: requester is not the waiting Listen Server host."));
		return false;
	}
	if (GetNumPlayers() < MinimumPlayersToStart)
	{
		UE_LOG(LogHALMatch, Warning, TEXT("Start request rejected: need %d players, have %d."),
			MinimumPlayersToStart, GetNumPlayers());
		return false;
	}
	MatchState->SetMatchPhase(EHALMatchPhase::Playing);
	UE_LOG(LogHALMatch, Log, TEXT("Match phase changed to Playing with %d players."), GetNumPlayers());
	return true;
}
