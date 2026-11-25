#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "../HALNetworkGameInstance.h"
#include "Engine/World.h"
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
	static void ResetResources(UHALNetworkGameInstance* GI) { GI->ResetMatchResources(); }
	static void CollectResources(UHALNetworkGameInstance* GI,const TArray<UObject*>& Roots) { GI->CollectMatchResources(Roots); }
	static bool ResourcesActive(UHALNetworkGameInstance* GI) { return GI->bMatchResourcesActive; }
	static bool ResourcesReady(UHALNetworkGameInstance* GI) { return GI->bMatchResourcesReady; }
	static const FString& ResourceError(UHALNetworkGameInstance* GI) { return GI->MatchResourceError; }
	static const TArray<FSoftObjectPath>& ResourcePaths(UHALNetworkGameInstance* GI) { return GI->MatchResourcePaths; }
	static int32 ResidentCount(UHALNetworkGameInstance* GI) { return GI->MatchResidentTextures.Num(); }
	static bool HasResidentTexture(UHALNetworkGameInstance* GI,UTexture2D* Texture) { return GI->MatchResidentTextures.Contains(Texture); }
	static void StartResourceFixture(UHALNetworkGameInstance* GI,const TArray<FSoftObjectPath>& Paths)
	{
		GI->ResetMatchResources(); GI->bMatchResourcesActive=true;
		GI->MatchResourceMap=UWorld::RemovePIEPrefix(GI->GetWorld()->GetOutermost()->GetName());
		GI->bMatchRosterCollected=true; GI->bMatchWorldCollected=true;
		for (const auto& Path:Paths) { GI->AddMatchResourcePath(Path); }
	}
	static void SetResourceRoster(UHALNetworkGameInstance* GI,UObject* Catalog,const TArray<FName>& Vehicles)
	{ GI->bMatchRosterCollected=false; GI->MatchCatalogPath=FSoftObjectPath(Catalog); GI->MatchVehicleIds=Vehicles; GI->MatchResourceAssets.Add(Catalog); }
};
#endif
