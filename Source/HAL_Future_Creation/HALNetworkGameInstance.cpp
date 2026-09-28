// Copyright Epic Games, Inc. All Rights Reserved.

#include "HALNetworkGameInstance.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HALMatchLobbyWidget.h"
#include "HALNetworkMenuWidget.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogHALNetworkMenu, Log, All);

void UHALNetworkGameInstance::Init()
{
	Super::Init();
	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &UHALNetworkGameInstance::OnNetworkFailure);
		GEngine->OnTravelFailure().AddUObject(this, &UHALNetworkGameInstance::OnTravelFailure);
	}
}

void UHALNetworkGameInstance::Shutdown()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().RemoveAll(this);
		GEngine->OnTravelFailure().RemoveAll(this);
	}
	ReplaceVisibleWidget(nullptr);
	Super::Shutdown();
}

void UHALNetworkGameInstance::LoadComplete(const float LoadTime, const FString& MapName)
{
	Super::LoadComplete(LoadTime, MapName);
	if (!EntryMapPath.IsEmpty() && MapName.EndsWith(FPackageName::GetShortName(EntryMapPath)))
	{
		ShowEntryMenu();
	}
}

bool UHALNetworkGameInstance::HostMatch()
{
	if (!GetWorld() || MatchMapPath.IsEmpty())
	{
		SetConnectionMessage(TEXT("Match map is not configured."));
		return false;
	}
	SetConnectionMessage(TEXT("Opening Listen Server..."));
	UGameplayStatics::OpenLevel(this, FName(*MatchMapPath), true, TEXT("listen"));
	return true;
}

bool UHALNetworkGameInstance::JoinMatch(const FString& HostIPv4)
{
	FIPv4Address Address;
	if (!FIPv4Address::Parse(HostIPv4.TrimStartAndEnd(), Address))
	{
		SetConnectionMessage(TEXT("Enter a numeric host IPv4 address, such as 192.168.1.10."));
		return false;
	}

	APlayerController* LocalController = GetFirstLocalPlayerController();
	if (!LocalController || !LocalController->IsLocalController())
	{
		SetConnectionMessage(TEXT("No local player is available to join the match."));
		return false;
	}

	SetConnectionMessage(FString::Printf(TEXT("Connecting to %s:%d..."), *Address.ToString(), MatchPort));
	LocalController->ClientTravel(FString::Printf(TEXT("%s:%d"), *Address.ToString(), MatchPort), TRAVEL_Absolute);
	return true;
}

void UHALNetworkGameInstance::ShowMatchLobby(APlayerController* LocalController)
{
	if (!LocalController || !LocalController->IsLocalController() || !MatchLobbyWidgetClass)
	{
		return;
	}
	if (UHALMatchLobbyWidget* Lobby = CreateWidget<UHALMatchLobbyWidget>(LocalController, MatchLobbyWidgetClass))
	{
		ReplaceVisibleWidget(Lobby);
	}
}

void UHALNetworkGameInstance::ShowEntryMenu()
{
	APlayerController* LocalController = GetFirstLocalPlayerController();
	if (!LocalController || !LocalController->IsLocalController() || !MenuWidgetClass)
	{
		UE_LOG(LogHALNetworkMenu, Warning, TEXT("Entry menu unavailable: local controller or MenuWidgetClass is missing."));
		return;
	}
	if (UHALNetworkMenuWidget* Menu = CreateWidget<UHALNetworkMenuWidget>(LocalController, MenuWidgetClass))
	{
		ReplaceVisibleWidget(Menu);
		LocalController->bShowMouseCursor = true;
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(Menu->TakeWidget());
		LocalController->SetInputMode(InputMode);
	}
}

void UHALNetworkGameInstance::ReplaceVisibleWidget(UUserWidget* NewWidget)
{
	if (VisibleWidget)
	{
		VisibleWidget->RemoveFromParent();
	}
	VisibleWidget = NewWidget;
	if (VisibleWidget)
	{
		VisibleWidget->AddToViewport();
	}
}

void UHALNetworkGameInstance::SetConnectionMessage(const FString& Message)
{
	ConnectionMessage = FText::FromString(Message);
	UE_LOG(LogHALNetworkMenu, Display, TEXT("%s"), *Message);
}

void UHALNetworkGameInstance::OnNetworkFailure(UWorld* World, UNetDriver*, const ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (World && World->GetGameInstance() != this)
	{
		return;
	}
	const FString Reason = !ErrorString.IsEmpty() ? ErrorString
		: FString::Printf(TEXT("Network connection failed (%s)."), ENetworkFailure::ToString(FailureType));
	SetConnectionMessage(Reason);
}

void UHALNetworkGameInstance::OnTravelFailure(UWorld* World, const ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (World && World->GetGameInstance() != this)
	{
		return;
	}
	const FString Reason = !ErrorString.IsEmpty() ? ErrorString
		: FString::Printf(TEXT("Could not open the destination (%s)."), ETravelFailure::ToString(FailureType));
	SetConnectionMessage(Reason);
}
