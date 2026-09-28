// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "HALNetworkGameInstance.generated.h"

class UHALNetworkMenuWidget;
class UHALMatchLobbyWidget;
class UUserWidget;
class UNetDriver;

/** Local menu travel and connection feedback. Gameplay authority stays in the match GameMode. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALNetworkGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;
	virtual void LoadComplete(float LoadTime, const FString& MapName) override;

	UFUNCTION(BlueprintCallable, Category = "Network Menu")
	bool HostMatch();

	/** Accepts a numeric IPv4 address only; the configured port is appended internally. */
	UFUNCTION(BlueprintCallable, Category = "Network Menu")
	bool JoinMatch(const FString& HostIPv4);

	UFUNCTION(BlueprintPure, Category = "Network Menu")
	FText GetConnectionMessage() const { return ConnectionMessage; }

	/** Called by the local match PlayerController once its map has begun play. */
	void ShowMatchLobby(APlayerController* LocalController);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Network Menu")
	FString EntryMapPath = TEXT("/Game/Maps/MVPB_NetEntry");

	UPROPERTY(EditDefaultsOnly, Category = "Network Menu")
	FString MatchMapPath = TEXT("/Game/Maps/MVPB_NetTest");

	/** UE's default listen port for this milestone; Host and Join use the same fixed value. */
	static constexpr int32 MatchPort = 7777;

	UPROPERTY(EditDefaultsOnly, Category = "Network Menu|Widgets")
	TSubclassOf<UHALNetworkMenuWidget> MenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Network Menu|Widgets")
	TSubclassOf<UHALMatchLobbyWidget> MatchLobbyWidgetClass;

private:
	void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	void ShowEntryMenu();
	void ReplaceVisibleWidget(UUserWidget* NewWidget);
	void SetConnectionMessage(const FString& Message);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> VisibleWidget;

	UPROPERTY(Transient)
	FText ConnectionMessage;
};
