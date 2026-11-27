#include "HALNetworkGameInstance.h"
#include "HALFrontEndConfig.h"
#include "HALArcadeStyle.h"
#include "HALGameFlowWidgets.h"
#include "HALMatchGameState.h"
#include "HALRoomComponent.h"
#include "HALPlayerState.h"
#include "HALVehicleCatalog.h"
#include "HALPracticeRoomWidget.h"
#include "HALMatchBallLifecycle.h"
#include "HALBallComicFXSettings.h"
#include "HALImpactBurstSettings.h"
#include "HALVehicleDoodleStatusComponent.h"
#include "HALVehicleStatusFXSettings.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Engine/LevelStreaming.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "Components/ActorComponent.h"
#include "Abilities/GameplayAbility.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "Misc/App.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"

namespace HALMatchLoading
{
	// Texture objects are shared by PIE worlds. Restore the prior flags only after their last local lease.
	TMap<TWeakObjectPtr<UTexture2D>,int32> TextureLeases;
	TMap<TWeakObjectPtr<UTexture2D>,uint8> TextureFlags;
	void Acquire(UTexture2D* Texture)
	{
		int32& Count=TextureLeases.FindOrAdd(Texture);
		if (Count++==0)
		{
			TextureFlags.Add(Texture,uint8(Texture->bForceMiplevelsToBeResident)|(uint8(Texture->bIgnoreStreamingMipBias)<<1));
			Texture->bForceMiplevelsToBeResident=true; Texture->bIgnoreStreamingMipBias=true;
		}
	}
	void Release(UTexture2D* Texture)
	{
		int32* Count=TextureLeases.Find(Texture);
		if (Count && --*Count==0)
		{
			const uint8 Flags=TextureFlags.FindChecked(Texture);
			Texture->bForceMiplevelsToBeResident=(Flags&1)!=0; Texture->bIgnoreStreamingMipBias=(Flags&2)!=0;
			TextureLeases.Remove(Texture); TextureFlags.Remove(Texture);
		}
	}
}

const UHALFrontEndConfig* UHALNetworkGameInstance::GetFrontEndConfig() const
{ return FrontEndConfig?FrontEndConfig.Get():GetDefault<UHALFrontEndConfig>(); }
void UHALNetworkGameInstance::FinishConnection()
{
	bConnecting=false;
	FTSTicker::GetCoreTicker().RemoveTicker(ConnectionTicker); ConnectionTicker.Reset();
}
void UHALNetworkGameInstance::CancelConnection()
{
	if (bConnecting) { ReturnToEntry(FText::FromString(TEXT("Connection cancelled. Create or join a room to try again."))); }
}
bool UHALNetworkGameInstance::IsMatchTravel(const FString& SourceMap,const FString& DestinationURL,bool bSeamless) const
{
	if (!bSeamless || !bPracticeRoomActive || PracticeMapPath.IsEmpty()
		|| UWorld::RemovePIEPrefix(SourceMap)!=PracticeMapPath) { return false; }
	const FURL Destination(nullptr,*DestinationURL,TRAVEL_Absolute);
	const FString Target=UWorld::RemovePIEPrefix(Destination.Map);
	return Destination.Valid && !DestinationURL.IsEmpty() && Target.StartsWith(TEXT("/"))
		&& Target!=PracticeMapPath && Target!=EntryMapPath;
}
void UHALNetworkGameInstance::HideTravelWidgets(const FString& DestinationURL,bool bSeamless)
{
	const bool bMatchTravel=IsMatchTravel(GetWorld()?GetWorld()->GetOutermost()->GetName():FString(),DestinationURL,bSeamless);
	ResetMatchResources();
	if (bMatchTravel) { BeginMatchResources(DestinationURL); }
	EndTravelPresentation(); ReplaceVisibleWidget(nullptr); BeginTravelPresentation(bMatchTravel);
}
void UHALNetworkGameInstance::BeginTravelPresentation(bool bMatchTravel)
{
	if (TravelOverlay || !GetGameViewportClient()) { return; }
	UClass* LoadingClass=MatchTravelLoadingClass.Get();
	if (bMatchTravel && LoadingClass && !LoadingClass->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists))
	{
		// A fresh static placeholder belongs to the GameInstance, never to the departing Room/PlayerController.
		TravelLoadingWidget=CreateWidget<UUserWidget>(this,LoadingClass);
		if (TravelLoadingWidget)
		{
			TravelLoadingWidget->SetPlayerContext(FLocalPlayerContext());
			TravelLoadingWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			TravelOverlay=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.Padding(0).BorderBackgroundColor(HALArcade::Ink())[TravelLoadingWidget->TakeWidget()];
			GetGameViewportClient()->AddViewportWidgetContent(TravelOverlay.ToSharedRef(),1000);
			return;
		}
	}
	// Keep the legacy overlay for Create/Join/return and when no match placeholder is configured.
	TravelOverlay=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(HALArcade::Ink()).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight().Padding(20)[HALArcade::Text(GetFrontEndConfig()->Title,34,GetFrontEndConfig()->Accent)]
		+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(20)[HALArcade::Text(TEXT("TRAVELLING TO THE ARENA"),24)]
		+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[HALArcade::Text(TEXT("Your vehicle will be ready shortly."),17,HALArcade::Muted())]];
	GetGameViewportClient()->AddViewportWidgetContent(TravelOverlay.ToSharedRef(),1000);
}
void UHALNetworkGameInstance::EndTravelPresentation()
{
	if (TravelOverlay && GetGameViewportClient()) { GetGameViewportClient()->RemoveViewportWidgetContent(TravelOverlay.ToSharedRef()); }
	TravelOverlay.Reset();
	TravelLoadingWidget=nullptr;
}

void UHALNetworkGameInstance::AddMatchResourcePath(const FSoftObjectPath& Path)
{
	if (!Path.IsNull() && !MatchResourcePaths.Contains(Path))
	{ MatchResourcePaths.Add(Path); MatchPendingPaths.Add(Path); }
}

void UHALNetworkGameInstance::FailMatchResources(const FString& Reason)
{
	if (!MatchResourceError.IsEmpty()) { return; }
	MatchResourceError=Reason;
	UE_LOG(LogTemp,Error,TEXT("Match preload failed: %s. Keeping Map Loading until travel recovery."),*Reason);
}

void UHALNetworkGameInstance::BeginMatchResources(const FString& DestinationURL)
{
	const auto* State=GetWorld()?GetWorld()->GetGameState<AHALMatchGameState>():nullptr;
	if (!State || !State->GetRoom()->GetData().bPracticeRoom) { return; } // Native legacy travel keeps its old behavior.
	bMatchResourcesActive=true;
	MatchResourceMap=UWorld::RemovePIEPrefix(FURL(nullptr,*DestinationURL,TRAVEL_Absolute).Map);
	const auto& Room=State->GetRoom()->GetData(); MatchCatalogPath=Room.Catalog.ToSoftObjectPath();
	AddMatchResourcePath(MatchCatalogPath);
	for (const auto& Player:Room.Members)
	{
		if (!Player) { continue; }
		MatchVehicleIds.AddUnique(Player->GetRoomData().VehicleId);
		AddMatchResourcePath(FSoftObjectPath(Player->GetClass())); // This round's granted abilities.
	}
	// Read the already configured Room template, but only root its MatchFlow container and children.
	if (const auto* WidgetClass=Cast<UWidgetBlueprintGeneratedClass>(RoomScreenClass.Get()))
	{
		if (auto* Tree=WidgetClass->GetWidgetTreeArchetype())
		{
			Tree->ForEachWidget([this](UWidget* Widget)
			{ if (const auto* Flow=Cast<UHALGameFlowScreenWidget>(Widget); Flow && Flow->bMatchFlowContainer)
				{ AddMatchResourcePath(FSoftObjectPath(Flow->GetClass())); UE_LOG(LogTemp,Display,TEXT("Match preload UI root: %s"),*Flow->GetClass()->GetPathName()); } });
		}
	}
	if (MatchTravelLoadingClass) { AddMatchResourcePath(FSoftObjectPath(MatchTravelLoadingClass.Get())); }
	const auto* Balls=GetDefault<UHALMatchBallSettings>();
	for (const auto& Arena:Balls->Arenas)
	{ if (Arena.MatchMap.GetLongPackageName()==MatchResourceMap) { AddMatchResourcePath(Arena.PrimaryBallClass.ToSoftObjectPath()); } }
	AddMatchResourcePath(Balls->ReturnRingMaterial.ToSoftObjectPath()); AddMatchResourcePath(Balls->ExplosiveBallType.ToSoftObjectPath());
	AddMatchResourcePath(Balls->BlackHoleBallType.ToSoftObjectPath()); AddMatchResourcePath(Balls->MeteorBallType.ToSoftObjectPath());
	AddMatchResourcePath(Balls->TimedBombBallType.ToSoftObjectPath()); AddMatchResourcePath(Balls->SpecialBallSupply.ToSoftObjectPath());
	AddMatchResourcePath(Balls->HomingBallType.ToSoftObjectPath());
	const auto& Comic=GetDefault<UHALBallComicFXSettings>()->Comic;
	if (Comic.bEnabled)
	{
		if (Comic.bTrail) { AddMatchResourcePath(Comic.TrailSystem.ToSoftObjectPath()); AddMatchResourcePath(Comic.TrailWedgeMaterial.ToSoftObjectPath()); AddMatchResourcePath(Comic.TrailWedgeMesh.ToSoftObjectPath()); }
		if (Comic.bAim) { AddMatchResourcePath(Comic.AimMaterial.ToSoftObjectPath()); AddMatchResourcePath(Comic.AimMesh.ToSoftObjectPath()); }
		if (Comic.bHoldRings) { AddMatchResourcePath(Comic.HoldBallMaterial.ToSoftObjectPath()); AddMatchResourcePath(Comic.HoldRoofMaterial.ToSoftObjectPath()); AddMatchResourcePath(Comic.HoldRoofMesh.ToSoftObjectPath()); }
	}
	const auto& Burst=GetDefault<UHALImpactBurstSettings>()->Burst;
	if (Burst.bEnabled) { AddMatchResourcePath(Burst.Material.ToSoftObjectPath()); if (Burst.bSparks) { AddMatchResourcePath(Burst.SparkSystem.ToSoftObjectPath()); } }
	const auto& Doodle=GetDefault<UHALVehicleDoodleStatusSettings>()->Status;
	if (Doodle.bEnabled) { AddMatchResourcePath(Doodle.StunMaterial.ToSoftObjectPath()); if (Doodle.bLowHealth) { AddMatchResourcePath(Doodle.LowHealthMaterial.ToSoftObjectPath()); } }
	const auto* FX=GetDefault<UHALVehicleStatusFXSettings>();
	AddMatchResourcePath(FX->DefaultCatalog.ToSoftObjectPath()); AddMatchResourcePath(FX->DamageFeedbackConfig.ToSoftObjectPath());
	UE_LOG(LogTemp,Display,TEXT("Match preload started: Map=%s Vehicles=%d Roots=%d"),*MatchResourceMap,MatchVehicleIds.Num(),MatchResourcePaths.Num());
	if (!MatchPendingPaths.IsEmpty())
	{
		MatchLoadingPaths=MoveTemp(MatchPendingPaths); MatchPendingPaths.Reset();
		MatchResourceLoad=UAssetManager::GetStreamableManager().RequestAsyncLoad(MatchLoadingPaths,FStreamableDelegate(),FStreamableManager::AsyncLoadHighPriority);
		if (!MatchResourceLoad) { FailMatchResources(TEXT("Could not start match asset loading")); }
	}
}

void UHALNetworkGameInstance::CollectMatchResources(const TArray<UObject*>& Roots)
{
	TArray<UObject*> Queue=Roots;
	for (int32 Index=0;Index<Queue.Num();++Index)
	{
		UObject* Object=Queue[Index];
		if (!IsValid(Object) || Object->IsEditorOnly() || MatchResourceVisited.Contains(Object)) { continue; }
		MatchResourceVisited.Add(Object);
		if (auto* Class=Cast<UClass>(Object))
		{
			if (Class->IsChildOf(UGameInstance::StaticClass()) || Class->IsChildOf(UHALPracticeRoomWidget::StaticClass())
				|| Class->IsChildOf(AController::StaticClass()) || Class->IsChildOf(AGameModeBase::StaticClass()) || Class->IsChildOf(AGameStateBase::StaticClass())) { continue; }
			if (Class->HasAnyClassFlags(CLASS_Native) && !Class->IsChildOf(AActor::StaticClass())
				&& !Class->IsChildOf(UActorComponent::StaticClass()) && !Class->IsChildOf(UGameplayAbility::StaticClass())) { continue; }
			Queue.Add(Class->GetDefaultObject());
			if (auto* BlueprintClass=Cast<UBlueprintGeneratedClass>(Class))
			{
				// Blueprint literals can reference art used later by a spawn/brush change.
				for (UObject* Reference:Class->ScriptAndPropertyObjectReferences) { Queue.Add(Reference); }
				for (TFieldIterator<UFunction> It(Class,EFieldIterationFlags::None);It;++It)
				{ for (UObject* Reference:It->ScriptAndPropertyObjectReferences) { Queue.Add(Reference); } }
				Queue.Add(BlueprintClass->SimpleConstructionScript);
				Queue.Add(BlueprintClass->GetInheritableComponentHandler(false));
				for (UActorComponent* Template:BlueprintClass->ComponentTemplates) { Queue.Add(Template); }
				if (Class->GetSuperClass() && !Class->GetSuperClass()->HasAnyClassFlags(CLASS_Native)) { Queue.Add(Class->GetSuperClass()); }
			}
			if (const auto* WidgetClass=Cast<UWidgetBlueprintGeneratedClass>(Class)) { Queue.Add(WidgetClass->GetWidgetTreeArchetype()); }
			MatchResourceAssets.Add(Class); continue;
		}
		if (Object->IsA<UWorld>() || Object->IsA<ULevel>() || Object->IsA<UPackage>() || Object->IsA<UField>() || Object->IsA<UBlueprint>()
			|| Object->IsA<UGameInstance>() || Object->IsA<AController>() || Object->IsA<AGameModeBase>() || Object->IsA<AGameStateBase>()
			|| Object->IsA<UHALVehicleCatalog>() || Object->IsA<UHALPracticeRoomWidget>()) { continue; }
		// Runtime actor instances stay owned by their World; retain only their referenced assets/templates.
		if (const auto* Actor=Cast<AActor>(Object); !Actor || Actor->IsTemplate()) { MatchResourceAssets.AddUnique(Object); }
		if (auto* Texture=Cast<UTexture2D>(Object))
		{
			if (!Texture->IsTemplate()) { MatchResidentTextures.Add(Texture); HALMatchLoading::Acquire(Texture); }
			continue;
		}
		if (Object->IsA<AActor>() || Object->IsA<UActorComponent>()) { Queue.Add(Object->GetClass()); }
		if (auto* Widget=Cast<UUserWidget>(Object)) { Queue.Add(Widget->WidgetTree); Queue.Add(Widget->GetClass()); }
		TArray<UObject*> References;
		FReferenceFinder Finder(References,nullptr,false,true,false,true); Finder.FindReferences(Object);
		Queue.Append(References);
	}
}

bool UHALNetworkGameInstance::UpdateMatchResources(APlayerController* PC)
{
	if (!bMatchResourcesActive) { return true; }
	if (!MatchResourceError.IsEmpty()) { return false; }
	if (bMatchResourcesReady) { return true; }
	if (MatchResourceLoad)
	{
		if (!MatchResourceLoad->HasLoadCompleted()) { return false; }
		TArray<UObject*> Loaded;
		for (const auto& Path:MatchLoadingPaths)
		{
			auto* Asset=Path.ResolveObject();
			if (!Asset) { FailMatchResources(FString::Printf(TEXT("Missing asset %s"),*Path.ToString())); return false; }
			Loaded.Add(Asset); MatchResourceAssets.AddUnique(Asset);
		}
		MatchResourceLoad.Reset(); MatchLoadingPaths.Reset(); CollectMatchResources(Loaded);
	}
	if (!bMatchRosterCollected && MatchCatalogPath.ResolveObject())
	{
		const auto* Catalog=Cast<UHALVehicleCatalog>(MatchCatalogPath.ResolveObject());
		if (!Catalog) { FailMatchResources(TEXT("The round catalog is not a vehicle catalog")); return false; }
		for (const FName Id:MatchVehicleIds)
		{
			const auto* Entry=Catalog->Vehicles.FindByPredicate([Id](const FHALVehicleCatalogEntry& E){ return E.VehicleId==Id; });
			if (!Entry || Entry->VehicleClass.IsNull()) { FailMatchResources(FString::Printf(TEXT("Missing round vehicle %s"),*Id.ToString())); return false; }
			AddMatchResourcePath(Entry->VehicleClass.ToSoftObjectPath());
			// Match HUD falls back to PreviewImage only when the participant has no dedicated icon.
			AddMatchResourcePath((Entry->Icon.IsNull()?Entry->PreviewImage:Entry->Icon).ToSoftObjectPath());
		}
		bMatchRosterCollected=true;
	}
	if (!MatchPendingPaths.IsEmpty())
	{
		MatchLoadingPaths=MoveTemp(MatchPendingPaths); MatchPendingPaths.Reset();
		MatchResourceLoad=UAssetManager::GetStreamableManager().RequestAsyncLoad(MatchLoadingPaths,FStreamableDelegate(),FStreamableManager::AsyncLoadHighPriority);
		if (!MatchResourceLoad) { FailMatchResources(TEXT("Could not start match asset loading")); }
		return false;
	}
	UWorld* World=PC?PC->GetWorld():GetWorld();
	if (!World || UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())!=MatchResourceMap || !bMatchRosterCollected) { return false; }
	if (World->IsVisibilityRequestPending()) { return false; }
	for (ULevelStreaming* Level:World->GetStreamingLevels()) { if (Level && Level->IsStreamingStatePending()) { return false; } }
	if (auto* Partition=World->GetSubsystem<UWorldPartitionSubsystem>(); Partition && !Partition->IsAllStreamingCompleted()) { return false; }
	if (!bMatchWorldCollected)
	{
		const auto* State=World->GetGameState<AHALMatchGameState>();
		if (!State || State->GetRoom()->GetData().Phase!=EHALRoomPhase::Loading) { return false; }
		TArray<UObject*> Roots;
		for (TActorIterator<AActor> It(World);It;++It)
		{
			if (It->IsA<AController>() || It->IsA<AGameModeBase>() || It->IsA<AGameStateBase>() || It->IsA<APlayerState>()) { continue; }
			Roots.Add(*It); TArray<UActorComponent*> Components; It->GetComponents(Components);
			for (auto* Component:Components) { Roots.Add(Component); }
		}
		CollectMatchResources(Roots); bMatchWorldCollected=true;
	}
	if (FApp::CanEverRender())
	{
		bool bTexturesReady=true;
		for (UTexture2D* Texture:MatchResidentTextures)
		{
#if WITH_EDITOR
			if (Texture->IsCompiling()) { bTexturesReady=false; continue; }
#endif
			if (!Texture->GetResource() || Texture->HasPendingInitOrStreaming()) { bTexturesReady=false; continue; }
			const auto& State=Texture->GetStreamableResourceState();
			const int32 Wanted=FMath::Max(0,int32(State.MaxNumLODs)-Texture->NumCinematicMipLevels);
			if (State.IsValid() && State.bSupportsStreaming && State.NumResidentLODs<Wanted)
			{ Texture->StreamIn(Wanted,true); bTexturesReady=false; }
		}
		if (!bTexturesReady) { return false; }
	}
	bMatchResourcesReady=true;
	UE_LOG(LogTemp,Display,TEXT("Match preload ready: Map=%s Roots=%d Textures=%d RenderChecks=%d"),*MatchResourceMap,MatchResourcePaths.Num(),MatchResidentTextures.Num(),FApp::CanEverRender());
	EndTravelPresentation(); return true;
}

void UHALNetworkGameInstance::ResetMatchResources()
{
	if (MatchResourceLoad) { MatchResourceLoad->CancelHandle(); MatchResourceLoad.Reset(); }
	for (UTexture2D* Texture:MatchResidentTextures) { HALMatchLoading::Release(Texture); }
	MatchResidentTextures.Reset(); MatchResourceAssets.Reset(); MatchResourceVisited.Reset();
	MatchResourcePaths.Reset(); MatchPendingPaths.Reset(); MatchLoadingPaths.Reset(); MatchVehicleIds.Reset();
	MatchCatalogPath.Reset(); MatchResourceMap.Reset(); MatchResourceError.Reset();
	bMatchResourcesActive=bMatchResourcesReady=bMatchWorldCollected=bMatchRosterCollected=false;
}
