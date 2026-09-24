// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HALMatchGameMode.generated.h"

class AHALPlayerController;

/** Server-only connection, spawn and minimal phase rules for the Listen Server match. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALMatchGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHALMatchGameMode();

	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Called by the PlayerController; only the local Listen Server host may start. */
	bool TryStartMatch(AHALPlayerController* Requestor);

protected:
	/** Maximum players, including the Listen Server host. */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Players", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaximumPlayers = 4;

	/** The initial network slice starts with two players; can grow to four without changing the code path. */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Players", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MinimumPlayersToStart = 2;

private:
	/** Reject invalid or duplicate authored slots before admitting a player. */
	bool CountValidSpawnSlots(int32& OutCount) const;
};
