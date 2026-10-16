// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HALPlayerController.generated.h"

class UHALDebugHUDWidget;
class UHALAbilitySystemComponent;

/** Local input, camera and match UI entry point for a single player. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AHALPlayerController();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	/** Print every vehicle's GAS Owner/Avatar and sole authoritative HP source. */
	UFUNCTION(Exec)
	void HALGASStatus();
	/** Development host console only. Changes test resistance without creating content assets. */
	UFUNCTION(Exec)
	void HALImpactDebug(int32 PlayerId, int32 Reduction, bool bImmune);
	/** Development only: request one authored Free ball in front of this controller's current vehicle. */
	UFUNCTION(Exec)
	void HALSpawnBall();

private:
	UPROPERTY(Transient)
	TObjectPtr<UHALDebugHUDWidget> DebugHUD;
	TSet<TWeakObjectPtr<UHALAbilitySystemComponent>> DebugImpactImmuneTargets;

	UFUNCTION(Server, Reliable)
	void ServerRequestStartMatch();
	UFUNCTION(Server, Reliable)
	void ServerSpawnDebugBall();
	UFUNCTION(Client, Reliable)
	void ClientDebugBallResult(const FString& Message, bool bSuccess);
};
