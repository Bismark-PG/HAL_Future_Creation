// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "HALRoundTravelTypes.h"
#include "Containers/Ticker.h"
#include "HALNetworkGameInstance.generated.h"

class UHALNetworkMenuWidget;
class UHALMatchLobbyWidget;
class UUserWidget;
class UNetDriver;
class UHALFrontEndConfig;
class UHALArcadeEntryWidget;
class UHALPracticeRoomWidget;

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
	const UHALFrontEndConfig* GetFrontEndConfig() const;
	bool IsConnecting() const { return bConnecting; }
	const FString& GetLastHostAddress() const { return LastHostAddress; }
	void CancelConnection();
	void BeginTravelPresentation();

	/** Called by the local match PlayerController once its map has begun play. */
	void ShowMatchLobby(APlayerController* LocalController);
	void ShowPracticeRoom(APlayerController* LocalController);
	void ReturnToEntry(const FText& Reason);
	void HideTravelWidgets();
	/** Server GameMode's one-shot map handoff; not a replicated rule store. */
	void StoreRoundTravel(const FHALRoundTravel& Value) { PendingRoundTravel=Value; }
	bool TakeRoundTravel(const FString& Map, FHALRoundTravel& Out);

protected:
	/** None selects the existing native layout. A Widget Blueprint subclass owns the Designer layout. */
	UPROPERTY(EditDefaultsOnly, Category="Front End|Screens") TSubclassOf<UHALArcadeEntryWidget> EntryScreenClass;
	UPROPERTY(EditDefaultsOnly, Category="Front End|Screens") TSubclassOf<UHALPracticeRoomWidget> RoomScreenClass;
	/** Optional separate local menu while a confirmed driver opens the menu in practice or Playing. */
	UPROPERTY(EditDefaultsOnly, Category="Front End|Screens") TSubclassOf<UHALPracticeRoomWidget> PauseScreenClass;
	/** Serialized compatibility only. New screen-class settings always take precedence. */
	UPROPERTY(meta=(DeprecatedProperty,DeprecationMessage="Use Entry Screen Class; None selects the native fallback.")) bool bUseNativeArcadeEntry = true;
	UPROPERTY(EditDefaultsOnly, Category="Front End") TObjectPtr<UHALFrontEndConfig> FrontEndConfig;
	UPROPERTY(EditDefaultsOnly, Category = "Network Menu")
	FString EntryMapPath = TEXT("/Game/Maps/MVPB_NetEntry");

	UPROPERTY(EditDefaultsOnly, Category = "Network Menu")
	FString MatchMapPath = TEXT("/Game/Maps/MVPB_NetTest");

	/** UE's default listen port for this milestone; Host and Join use the same fixed value. */
	static constexpr int32 MatchPort = 7777;

	UPROPERTY(meta=(DeprecatedProperty,DeprecationMessage="Use Entry Screen Class with HALArcadeEntryWidget as parent."))
	TSubclassOf<UHALNetworkMenuWidget> MenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Network Menu|Widgets")
	TSubclassOf<UHALMatchLobbyWidget> MatchLobbyWidgetClass;

private:
	friend struct FHALWidgetExtensionTestAccess;
	UClass* ResolveEntryScreen() const;
	UClass* ResolveRoomScreen(APlayerController* LocalController) const;
	void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	void ShowEntryMenu();
	void ReplaceVisibleWidget(UUserWidget* NewWidget);
	void SetConnectionMessage(const FString& Message);
	void OpenEntryAfterDisconnect();
	void EndTravelPresentation();
	void FinishConnection();
	bool bPracticeRoomActive = false, bReturningToEntry = false;
	bool bConnecting=false;
	FString LastHostAddress=TEXT("127.0.0.1");
	FTSTicker::FDelegateHandle ConnectionTicker;
	TSharedPtr<SWidget> TravelOverlay;
	UPROPERTY(Transient) FHALRoundTravel PendingRoundTravel;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> VisibleWidget;

	UPROPERTY(Transient)
	FText ConnectionMessage;
};
