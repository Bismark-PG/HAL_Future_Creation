// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HALMatchLobbyWidget.generated.h"

/** Local waiting/playing display; all decisions remain in the server GameMode. */
UCLASS(Abstract, Blueprintable)
class HAL_FUTURE_CREATION_API UHALMatchLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintPure, Category = "Match UI")
	int32 GetConnectedPlayerCount() const;

	UFUNCTION(BlueprintPure, Category = "Match UI")
	bool IsWaitingForPlayers() const;

	UFUNCTION(BlueprintPure, Category = "Match UI")
	bool IsLocalHost() const;

	UFUNCTION(BlueprintPure, Category = "Match UI")
	bool CanStartMatch() const;

	UFUNCTION(BlueprintCallable, Category = "Match UI")
	void StartMatch();

private:
	void RefreshInputMode();
	FTimerHandle InputModeRefreshTimer;
};
