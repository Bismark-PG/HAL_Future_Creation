// Copyright Epic Games, Inc. All Rights Reserved.

#include "HALNetworkGameInstance.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HALMatchLobbyWidget.h"
#include "HALNetworkMenuWidget.h"
#include "HALPracticeRoomWidget.h"
#include "HALArcadeEntryWidget.h"
#include "HALFrontEndConfig.h"
#include "HALPlayerController.h"
#include "HALPlayerState.h"
#include "HALMatchGameState.h"
#include "HALRoomComponent.h"
#include "HALPracticeRoomGameMode.h"
#include "GameFramework/WorldSettings.h"
#include "TimerManager.h"
#include "Containers/Ticker.h"
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
	ResetMatchResources();
	FinishConnection(); EndTravelPresentation();
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
		ResetMatchResources();
		bPracticeRoomActive = false; bReturningToEntry = false;
		PracticeMapPath.Reset();
		PendingRoundTravel={};
		ShowEntryMenu();
	}
}

bool UHALNetworkGameInstance::HostMatch()
{
	if (bConnecting || bReturningToEntry || bPracticeRoomActive) { return false; }
	if (!GetWorld() || MatchMapPath.IsEmpty() || !FPackageName::DoesPackageExist(MatchMapPath))
	{
		SetConnectionMessage(TEXT("Match map is not configured."));
		return false;
	}
	SetConnectionMessage(TEXT("Opening Listen Server..."));
	bConnecting=true; BeginTravelPresentation();
	// Allow the local feedback to be presented before the blocking map open.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float)
	{ if (bConnecting) { GEngine->CancelPending(GetWorld()); UGameplayStatics::OpenLevel(this,FName(*MatchMapPath),true,TEXT("listen")); } return false; }),.1f);
	return true;
}

bool UHALNetworkGameInstance::JoinMatch(const FString& HostIPv4)
{
	if (bConnecting || bReturningToEntry || bPracticeRoomActive) { return false; }
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
	LastHostAddress=Address.ToString(); bConnecting=true;
	const float Timeout=GetFrontEndConfig()->ConnectionTimeoutSeconds;
	const double Deadline=FPlatformTime::Seconds()+(FMath::IsFinite(Timeout)?FMath::Clamp(Timeout,5.f,120.f):30.f);
	ConnectionTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this,Deadline](float)
	{
		if (!bConnecting) { return false; }
		if (FPlatformTime::Seconds()<Deadline) { return true; }
		ReturnToEntry(FText::FromString(TEXT("Could not reach the room. Check the host address and try again."))); return false;
	}),.25f);
	LocalController->ClientTravel(FString::Printf(TEXT("%s:%d"), *Address.ToString(), MatchPort), TRAVEL_Absolute);
	return true;
}

void UHALNetworkGameInstance::ShowMatchLobby(APlayerController* LocalController)
{
	// Map-authored GameMode metadata is available before the first room replication on a joining client.
	// Keep the old lobby and its configured references for legacy test maps only.
	const auto* World=LocalController?LocalController->GetWorld():nullptr;
	const auto* Settings=World?World->GetWorldSettings():nullptr;
	const auto* State=World?World->GetGameState<AHALMatchGameState>():nullptr;
	if (bPracticeRoomActive || (Settings && Settings->DefaultGameMode && Settings->DefaultGameMode->IsChildOf(AHALPracticeRoomGameMode::StaticClass()))
		|| (State && State->GetRoom()->GetData().bPracticeRoom)) { return; }
	if (!LocalController || !LocalController->IsLocalController() || !LocalController->GetLocalPlayer() || !MatchLobbyWidgetClass)
	{
		return;
	}
	if (UHALMatchLobbyWidget* Lobby = CreateWidget<UHALMatchLobbyWidget>(LocalController, MatchLobbyWidgetClass))
	{
		FinishConnection(); EndTravelPresentation();
		ReplaceVisibleWidget(Lobby);
	}
}

namespace
{
	UClass* ValidScreenOrFallback(UClass* Configured,UClass* RequiredBase,UClass* Fallback=nullptr)
	{ return Configured && Configured->IsChildOf(RequiredBase) && !Configured->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)?Configured:(Fallback?Fallback:RequiredBase); }
}
UClass* UHALNetworkGameInstance::ResolveEntryScreen() const
{ return ValidScreenOrFallback(EntryScreenClass.Get(),UHALArcadeEntryWidget::StaticClass()); }
UClass* UHALNetworkGameInstance::ResolveRoomScreen(APlayerController* LocalController) const
{
	const auto* PC=Cast<AHALPlayerController>(LocalController); const auto* PS=PC?PC->GetPlayerState<AHALPlayerState>():nullptr;
	const auto* State=PC && PC->GetWorld()?PC->GetWorld()->GetGameState<AHALMatchGameState>():nullptr;
	const bool Paused=PC && PS && State && PC->WantsRoomMenu() && PS->GetRoomData().Phase==EHALRoomPlayerPhase::Ready
		&& (State->GetRoom()->GetData().Phase==EHALRoomPhase::Open || State->GetRoom()->GetData().Phase==EHALRoomPhase::Playing);
	UClass* Native=UHALPracticeRoomWidget::StaticClass();
	UClass* RoomClass=ValidScreenOrFallback(RoomScreenClass.Get(),Native);
	return Paused && PauseScreenClass?ValidScreenOrFallback(PauseScreenClass.Get(),Native,RoomClass):RoomClass;
}
void UHALNetworkGameInstance::ShowEntryMenu()
{
	APlayerController* LocalController = GetFirstLocalPlayerController();
	if (!LocalController || !LocalController->IsLocalController() || !LocalController->GetLocalPlayer())
	{
		UE_LOG(LogHALNetworkMenu, Warning, TEXT("Entry menu unavailable: local player is not attached."));
		return;
	}
	UClass* EntryClass=ResolveEntryScreen();
	if (UUserWidget* Menu = CreateWidget<UUserWidget>(LocalController, EntryClass))
	{
		FinishConnection(); EndTravelPresentation();
		Menu->SetIsFocusable(true);
		ReplaceVisibleWidget(Menu);
		LocalController->bShowMouseCursor = true;
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(Menu->TakeWidget());
		LocalController->SetInputMode(InputMode);
	}
}
void UHALNetworkGameInstance::ShowPracticeRoom(APlayerController* PC)
{
	if (bReturningToEntry || !GetWorld() || GetWorld()->bIsTearingDown || !PC || PC->IsActorBeingDestroyed() || !PC->IsLocalController() || !PC->GetLocalPlayer()) { return; }
	bPracticeRoomActive = true;
	if (const auto* State=PC->GetWorld()->GetGameState<AHALMatchGameState>(); State && State->GetRoom()->GetData().Phase==EHALRoomPhase::Open)
	{ PracticeMapPath=UWorld::RemovePIEPrefix(PC->GetWorld()->GetOutermost()->GetName()); }
	FinishConnection();
	if (!bMatchResourcesActive || bMatchResourcesReady) { EndTravelPresentation(); }
	UClass* Desired=ResolveRoomScreen(PC);
	if (!VisibleWidget || VisibleWidget->GetClass()!=Desired || VisibleWidget->GetWorld()!=PC->GetWorld() || VisibleWidget->GetOwningPlayer()!=PC)
	{ ReplaceVisibleWidget(CreateWidget<UHALPracticeRoomWidget>(PC,Desired)); }
	else if (!VisibleWidget->IsInViewport())
	{
		// Seamless travel may remove the viewport slot while retaining this local widget/controller.
		VisibleWidget->AddToViewport();
		UE_LOG(LogHALNetworkMenu,Display,TEXT("Reattached Room widget after travel: %s Mounted=%d"),*GetNameSafe(VisibleWidget),VisibleWidget->IsInViewport());
	}
	if (auto* Screen=Cast<UHALPracticeRoomWidget>(VisibleWidget)) { Screen->RefreshFromRoomState(); }
}
bool UHALNetworkGameInstance::RoomOwnsMatchHUD(const APlayerController* PC) const
{
	const auto* Screen=Cast<UHALPracticeRoomWidget>(VisibleWidget);
	return PC && Screen && Screen->GetWorld()==PC->GetWorld() && Screen->GetOwningPlayer()==PC && Screen->ManagesMatchHUD();
}
void UHALNetworkGameInstance::ReturnToEntry(const FText& Reason)
{
	if (bReturningToEntry || !GetWorld()) { return; }
	SetConnectionMessage(Reason.ToString()); bReturningToEntry = true;
	ResetMatchResources();
	FinishConnection(); EndTravelPresentation(); bPracticeRoomActive=false;
	PracticeMapPath.Reset();
	PendingRoundTravel={};
	ReplaceVisibleWidget(nullptr);
	// Defer out of network failure / RPC callbacks; OpenLevel closes the old NetDriver.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float)
	{ OpenEntryAfterDisconnect(); return false; }));
}
bool UHALNetworkGameInstance::TakeRoundTravel(const FString& Map, FHALRoundTravel& Out)
{
	if (!PendingRoundTravel.IsValid() || PendingRoundTravel.Destination!=Map) { return false; }
	Out=PendingRoundTravel; PendingRoundTravel={}; return true;
}
void UHALNetworkGameInstance::OpenEntryAfterDisconnect()
{
	// A cancelled Join can still have a PendingNetGame. A local LoadMap must not inherit it as client state.
	if (GEngine && GetWorld()) { GEngine->CancelPending(GetWorld()); }
	if (!EntryMapPath.IsEmpty()) { UGameplayStatics::OpenLevel(this,FName(*EntryMapPath),true); }
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
	if (bReturningToEntry) { return; }
	if (World && World->GetGameInstance() != this)
	{
		return;
	}
	const FString Reason = !ErrorString.IsEmpty() ? ErrorString
		: FString::Printf(TEXT("Network connection failed (%s)."), ENetworkFailure::ToString(FailureType));
	SetConnectionMessage(Reason);
	if (bPracticeRoomActive || bConnecting) { ReturnToEntry(FText::FromString(Reason)); }
}

void UHALNetworkGameInstance::OnTravelFailure(UWorld* World, const ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (bReturningToEntry) { return; }
	if (World && World->GetGameInstance() != this)
	{
		return;
	}
	const FString Reason = !ErrorString.IsEmpty() ? ErrorString
		: FString::Printf(TEXT("Could not open the destination (%s)."), ETravelFailure::ToString(FailureType));
	SetConnectionMessage(Reason);
	if (bPracticeRoomActive || bConnecting) { ReturnToEntry(FText::FromString(Reason)); }
}
