// Copyright Epic Games, Inc. All Rights Reserved.

#include "HALMatchLobbyWidget.h"

#include "HALMatchGameMode.h"
#include "HALMatchGameState.h"
#include "HALPlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UHALMatchLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshInputMode();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(InputModeRefreshTimer, this,
			&UHALMatchLobbyWidget::RefreshInputMode, 0.25f, true);
	}
}

void UHALMatchLobbyWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InputModeRefreshTimer);
	}
	Super::NativeDestruct();
}

void UHALMatchLobbyWidget::RefreshInputMode()
{
	APlayerController* Controller = GetOwningPlayer();
	const AHALMatchGameState* MatchState = GetWorld() ? GetWorld()->GetGameState<AHALMatchGameState>() : nullptr;
	if (!Controller || !MatchState)
	{
		return;
	}
	if (MatchState->GetMatchPhase() == EHALMatchPhase::Playing)
	{
		Controller->bShowMouseCursor = false;
		Controller->SetInputMode(FInputModeGameOnly());
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(InputModeRefreshTimer);
		}
	}
	else
	{
		Controller->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		Controller->SetInputMode(InputMode);
	}
}

int32 UHALMatchLobbyWidget::GetConnectedPlayerCount() const
{
	const AHALMatchGameState* MatchState = GetWorld() ? GetWorld()->GetGameState<AHALMatchGameState>() : nullptr;
	return MatchState ? MatchState->PlayerArray.Num() : 0;
}

bool UHALMatchLobbyWidget::IsWaitingForPlayers() const
{
	const AHALMatchGameState* MatchState = GetWorld() ? GetWorld()->GetGameState<AHALMatchGameState>() : nullptr;
	return MatchState && MatchState->GetMatchPhase() == EHALMatchPhase::WaitingForPlayers;
}

bool UHALMatchLobbyWidget::IsLocalHost() const
{
	const APlayerController* Controller = GetOwningPlayer();
	return Controller && Controller->IsLocalController() && Controller->HasAuthority()
		&& GetWorld() && GetWorld()->GetNetMode() == NM_ListenServer;
}

bool UHALMatchLobbyWidget::CanStartMatch() const
{
	const AHALMatchGameMode* MatchMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHALMatchGameMode>() : nullptr;
	return IsLocalHost() && IsWaitingForPlayers() && MatchMode
		&& GetConnectedPlayerCount() >= MatchMode->GetMinimumPlayersToStart();
}

void UHALMatchLobbyWidget::StartMatch()
{
	if (AHALPlayerController* Controller = Cast<AHALPlayerController>(GetOwningPlayer()))
	{
		Controller->RequestStartMatch();
	}
}
