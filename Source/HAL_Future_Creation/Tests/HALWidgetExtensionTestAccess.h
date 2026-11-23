#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "../HALNetworkGameInstance.h"
#include "HALWidgetExtensionTestWidgets.h"
struct FHALWidgetExtensionTestAccess
{
	static void Configure(UHALNetworkGameInstance* GI,bool Custom)
	{ GI->EntryScreenClass=Custom?UHALContractEntryWidget::StaticClass():nullptr;
		GI->RoomScreenClass=Custom?UHALContractSelectionWidget::StaticClass():nullptr;
		GI->PauseScreenClass=Custom?UHALContractPauseWidget::StaticClass():nullptr; }
	static UClass* Entry(UHALNetworkGameInstance* GI) { return GI->ResolveEntryScreen(); }
	static UClass* Room(UHALNetworkGameInstance* GI,APlayerController* PC) { return GI->ResolveRoomScreen(PC); }
	static void ShowEntry(UHALNetworkGameInstance* GI) { GI->ShowEntryMenu(); }
	static void ConfigureTravel(UHALNetworkGameInstance* GI,UClass* LoadingClass,const FString& PracticeMap)
	{ GI->MatchTravelLoadingClass=LoadingClass; GI->PracticeMapPath=PracticeMap; GI->bPracticeRoomActive=!PracticeMap.IsEmpty(); }
	static bool MatchTravel(UHALNetworkGameInstance* GI,const FString& Source,const FString& Destination,bool Seamless=true)
	{ return GI->IsMatchTravel(Source,Destination,Seamless); }
	static UUserWidget* TravelWidget(UHALNetworkGameInstance* GI) { return GI->TravelLoadingWidget; }
	static void EndTravel(UHALNetworkGameInstance* GI) { GI->EndTravelPresentation(); }
};
#endif
