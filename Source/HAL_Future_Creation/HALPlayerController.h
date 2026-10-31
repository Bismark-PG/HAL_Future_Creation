// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffectTypes.h"
#include "HALPlayerController.generated.h"

class UHALDebugHUDWidget;
class UHALAbilitySystemComponent;
class UHALCameraDebugWidget;

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
	UFUNCTION(Exec) void HALCameraReset();
	UFUNCTION(Exec) void HALCameraStatus();

	/** Print every vehicle's GAS Owner/Avatar and sole authoritative HP source. */
	UFUNCTION(Exec)
	void HALGASStatus();
	/** Development host console only. Changes test resistance without creating content assets. */
	UFUNCTION(Exec)
	void HALImpactDebug(int32 PlayerId, int32 Reduction, bool bImmune);
	/** Development host console only: HALStun PlayerId DurationSeconds. Uses shared Stun rules. */
	UFUNCTION(Exec)
	void HALStun(int32 PlayerId, float DurationSeconds);
	/** Development only: request one authored Free ball in front of this controller's current vehicle. */
	UFUNCTION(Exec)
	void HALSpawnBall();
	/** Request the current primary skill through the same local input/GAS path as an Input Action. */
	UFUNCTION(Exec)
	void HALUseSkill();
	/** Development only: server spawns one physical bomb and immediately starts its configured fuse. */
	UFUNCTION(Exec) void HALSpawnBomb();
	/** Development only: request the server-configured movement boost on this controller's own Avatar. */
	UFUNCTION(Exec)
	void HALMovementBoost();
	/** Development only: remove this Avatar's boost family, preserving other movement GEs/cooldowns. */
	UFUNCTION(Exec)
	void HALClearMovementBoost();

private:
	UFUNCTION(Server, Reliable) void ServerSpawnDebugBomb();
	UFUNCTION(Client, Reliable) void ClientDebugBombResult(const FString& Message, bool bSuccess);
	UPROPERTY(Transient)
	TObjectPtr<UHALDebugHUDWidget> DebugHUD;
	UPROPERTY(Transient) TObjectPtr<UHALCameraDebugWidget> CameraDebugHUD;
	TMap<TWeakObjectPtr<UHALAbilitySystemComponent>, FActiveGameplayEffectHandle> DebugImpactImmuneTargets;

	UFUNCTION(Server, Reliable)
	void ServerRequestStartMatch();
	UFUNCTION(Server, Reliable)
	void ServerSpawnDebugBall();
	UFUNCTION(Client, Reliable)
	void ClientDebugBallResult(const FString& Message, bool bSuccess);
	UFUNCTION(Server, Reliable)
	void ServerRequestMovementBoost(bool bRemove);
	UFUNCTION(Client, Reliable)
	void ClientMovementBoostResult(const FString& Message, bool bSuccess);
};
