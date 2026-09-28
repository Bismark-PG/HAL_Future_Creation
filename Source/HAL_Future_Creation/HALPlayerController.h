// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HALPlayerController.generated.h"

/** Local input, camera and match UI entry point for a single player. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AHALPlayerController();
	virtual void BeginPlay() override;

	/** Request the transition from WaitingForPlayers to Playing. The server checks host ownership. */
	UFUNCTION(BlueprintCallable, Category = "Match")
	void RequestStartMatch();

	/** Console entry point for early PIE/LAN tests; uses the same checked request path. */
	UFUNCTION(Exec)
	void HALStartMatch();

	/** Print the local controller, Pawn and replicated phase for PIE/LAN ownership checks. */
	UFUNCTION(Exec)
	void HALMatchStatus();

	/** Local network-physics diagnostic snapshot for four-machine testing. */
	UFUNCTION(Exec)
	void HALNetMetrics();

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestStartMatch();
};
