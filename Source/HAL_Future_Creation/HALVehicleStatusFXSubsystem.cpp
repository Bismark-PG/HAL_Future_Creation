#include "HALVehicleStatusFXSubsystem.h"
#include "HALVehicleStatusFXCatalog.h"
#include "HALVehicleStatusFXDefinition.h"
#include "HALVehicleStatusFXSettings.h"
#include "HALVehicleStatusFXComponent.h"
#include "VehicleDefinition.h"
#include "TestVehiclePawn.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogHALStatusFX, Log, All);

bool UHALVehicleStatusFXSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UHALVehicleStatusFXSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bStopped = false;
	PrepareForVehicle(nullptr);
}

void UHALVehicleStatusFXSubsystem::Deinitialize()
{
	check(IsInGameThread());
	bStopped = true;
	for (auto& Pair : Catalogs) { if (Pair.Value.Handle) { Pair.Value.Handle->CancelHandle(); } }
	Catalogs.Empty();
	CatalogChanged.Clear();
	Super::Deinitialize();
}

TSoftObjectPtr<UHALVehicleStatusFXCatalog> UHALVehicleStatusFXSubsystem::SelectCatalog(const UVehicleDefinition* Vehicle)
{
	if (Vehicle && Vehicle->StatusFXCatalogOverride) { return Vehicle->StatusFXCatalogOverride.Get(); }
	return GetDefault<UHALVehicleStatusFXSettings>()->DefaultCatalog;
}

void UHALVehicleStatusFXSubsystem::PrepareForVehicle(const UVehicleDefinition* Vehicle)
{
	PrepareCatalog(GetDefault<UHALVehicleStatusFXSettings>()->DefaultCatalog);
	PrepareCatalog(SelectCatalog(Vehicle));
}

void UHALVehicleStatusFXSubsystem::PrepareCatalog(TSoftObjectPtr<UHALVehicleStatusFXCatalog> Catalog)
{
	check(IsInGameThread());
	const FSoftObjectPath Path = Catalog.ToSoftObjectPath();
	if (bStopped || Path.IsNull() || Catalogs.Contains(Path)) { return; }
	Catalogs.Add(Path);
	if (Catalog.Get()) { CompleteCatalog(Path); return; } // Already loaded, no synchronous I/O.
	const auto Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Path,
		FStreamableDelegate::CreateWeakLambda(this, [this, Path]() { if (!bStopped) { CompleteCatalog(Path); } }));
	if (bStopped) { if (Handle) { Handle->CancelHandle(); } return; }
	if (auto* Record = Catalogs.Find(Path))
	{
		if (Record->State == EHALStatusFXLoadState::Loading) { Record->Handle = Handle; }
		else if (Handle) { Handle->ReleaseHandle(); }
	}
	if (!Handle) { CompleteCatalog(Path); }
}

void UHALVehicleStatusFXSubsystem::CompleteCatalog(const FSoftObjectPath& Path)
{
	if (bStopped) { return; }
	auto* Record = Catalogs.Find(Path);
	if (!Record || Record->State != EHALStatusFXLoadState::Loading) { return; }
	auto* Catalog = Cast<UHALVehicleStatusFXCatalog>(Path.ResolveObject());
	FString Error;
	if (!Catalog || !Catalog->Validate(Error))
	{
		Record->State = EHALStatusFXLoadState::Failed;
		Record->Error = Catalog ? Error : TEXT("missing catalog package or wrong asset class");
		UE_LOG(LogHALStatusFX, Warning, TEXT("Status FX catalog failed [%s]: %s. Gameplay remains available."), *Path.ToString(), *Record->Error);
	}
	else
	{
		Record->Catalog = Catalog;
		Record->State = EHALStatusFXLoadState::Ready;
		UE_LOG(LogHALStatusFX, Display, TEXT("Status FX catalog ready [%s], Effects=%d (assets loaded; not GPU prewarm)."), *Path.ToString(), Catalog->Effects.Num());
	}
	const auto State = Record->State;
	// The UPROPERTY catalog holds hard dependencies; release the streaming lease after completion.
	if (Record->Handle) { Record->Handle->ReleaseHandle(); Record->Handle.Reset(); }
	CatalogChanged.Broadcast(Path, State);
}

EHALStatusFXLoadState UHALVehicleStatusFXSubsystem::GetCatalogState(TSoftObjectPtr<UHALVehicleStatusFXCatalog> Catalog, FString& OutError) const
{
	check(IsInGameThread());
	OutError.Reset();
	if (Catalog.IsNull()) { return EHALStatusFXLoadState::Unconfigured; }
	if (const auto* Record = Catalogs.Find(Catalog.ToSoftObjectPath())) { OutError = Record->Error; return Record->State; }
	return EHALStatusFXLoadState::NotRequested;
}

const UHALVehicleStatusFXDefinition* UHALVehicleStatusFXSubsystem::FindEffect(FGameplayTag Tag, const UVehicleDefinition* Vehicle) const
{
	check(IsInGameThread());
	const auto* Record = Catalogs.Find(SelectCatalog(Vehicle).ToSoftObjectPath());
	if (!Record || Record->State != EHALStatusFXLoadState::Ready || !Record->Catalog) { return nullptr; }
	if (const auto* Effect = Record->Catalog->FindExact(Tag)) { return Effect; }
	// A valid partial vehicle override only replaces explicitly listed tags.
	// An invalid/loading override never silently changes to the default look.
	if (Vehicle && Vehicle->StatusFXCatalogOverride)
	{
		const auto* Default = Catalogs.Find(GetDefault<UHALVehicleStatusFXSettings>()->DefaultCatalog.ToSoftObjectPath());
		return Default && Default->State == EHALStatusFXLoadState::Ready && Default->Catalog ? Default->Catalog->FindExact(Tag) : nullptr;
	}
	return nullptr;
}

void UHALVehicleStatusFXSubsystem::LogStatus() const
{
	check(IsInGameThread());
	const auto Default = SelectCatalog(nullptr);
	FString Error;
	const auto State = GetCatalogState(Default, Error);
	UE_LOG(LogHALStatusFX, Display, TEXT("StatusFX DefaultCatalog=%s State=%s Catalogs=%d Error=%s"), *Default.ToString(),
		*StaticEnum<EHALStatusFXLoadState>()->GetNameStringByValue(int64(State)), Catalogs.Num(), *Error);
	for (const auto& Pair : Catalogs)
	{
		UE_LOG(LogHALStatusFX, Display, TEXT("StatusFX Catalog=%s State=%s Effects=%d Error=%s"), *Pair.Key.ToString(),
			*StaticEnum<EHALStatusFXLoadState>()->GetNameStringByValue(int64(Pair.Value.State)),
			Pair.Value.Catalog ? Pair.Value.Catalog->Effects.Num() : 0, *Pair.Value.Error);
	}
}

#if !UE_BUILD_SHIPPING
namespace
{
	TArray<ATestVehiclePawn*> SelectFXVehicles(UWorld* World, const FString& Selector)
	{
		TArray<ATestVehiclePawn*> Result;
		if (!World || !World->IsGameWorld()) { return Result; }
		int32 Id = INDEX_NONE;
		const bool bId = LexTryParseString(Id, *Selector) && Id >= 0;
		for (TActorIterator<ATestVehiclePawn> It(World); It; ++It)
		{
			const auto* PS = It->GetPlayerState();
			if (Selector == TEXT("all") || (Selector == TEXT("local") && It->IsLocallyControlled()) || (bId && PS && PS->GetPlayerId() == Id))
			{ Result.Add(*It); }
		}
		return Result;
	}
	FAutoConsoleCommandWithWorldAndArgs GHALStatusFXStatus(TEXT("hal.StatusFXStatus"), TEXT("hal.StatusFXStatus [all|local|PlayerId]: catalog, source, instance and dormant-resource diagnostics."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() > 1) { UE_LOG(LogHALStatusFX, Display, TEXT("Usage: hal.StatusFXStatus [all|local|PlayerId]")); return; }
			auto* GI = World ? World->GetGameInstance() : nullptr;
			auto* FX = GI ? GI->GetSubsystem<UHALVehicleStatusFXSubsystem>() : nullptr;
			if (FX) { FX->LogStatus(); }
			const auto Cars = SelectFXVehicles(World, Args.IsEmpty() ? TEXT("all") : Args[0]);
			if (Cars.IsEmpty()) { UE_LOG(LogHALStatusFX, Display, TEXT("StatusFX: no matching live vehicle; run in a game window. PlayerId is printed by hal.StatusFXStatus.")); }
			for (auto* Car : Cars)
			{
				if (auto* Component = Car->FindComponentByClass<UHALVehicleStatusFXComponent>()) { Component->LogDiagnostics(); }
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs GHALStatusFXDebug(TEXT("hal.StatusFXDebug"), TEXT("hal.StatusFXDebug <local|PlayerId> <State.Visual.Tag> <SourceName> <0|1>: local cosmetic source only; no RPC/GAS writes."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() != 4 || (Args[3] != TEXT("0") && Args[3] != TEXT("1")) || Args[2].Len() > 64 || Args[2].IsEmpty())
			{ UE_LOG(LogHALStatusFX, Display, TEXT("Usage: hal.StatusFXDebug <local|PlayerId> <State.Visual.Tag> <SourceName> <0|1>")); return; }
			const auto Cars = SelectFXVehicles(World, Args[0]);
			if (Cars.IsEmpty()) { UE_LOG(LogHALStatusFX, Display, TEXT("StatusFX debug: no matching vehicle.")); return; }
			const auto Tag = FGameplayTag::RequestGameplayTag(FName(*Args[1]), false);
			for (auto* Car : Cars)
			{
				auto* Component = Car->FindComponentByClass<UHALVehicleStatusFXComponent>();
				const bool bAccepted = Component && Component->SetDebugSource(FName(*Args[2]), Tag, Args[3] == TEXT("1"));
				UE_LOG(LogHALStatusFX, Display, TEXT("StatusFX debug Vehicle=%s Source=%s Tag=%s Active=%s Accepted=%d LocalOnly=1"),
					*Car->GetName(), *Args[2], *Args[1], *Args[3], bAccepted);
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs GHALStatusFXDebugClear(TEXT("hal.StatusFXDebugClear"), TEXT("hal.StatusFXDebugClear [local|PlayerId|all]: remove local debug sources; preserve ASC/external sources."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() > 1) { UE_LOG(LogHALStatusFX, Display, TEXT("Usage: hal.StatusFXDebugClear [local|PlayerId|all]")); return; }
			for (auto* Car : SelectFXVehicles(World, Args.IsEmpty() ? TEXT("local") : Args[0]))
			{
				if (auto* Component = Car->FindComponentByClass<UHALVehicleStatusFXComponent>()) { Component->ClearDebugSources(); }
				UE_LOG(LogHALStatusFX, Display, TEXT("StatusFX debug cleared Vehicle=%s LocalOnly=1"), *Car->GetName());
			}
		}));
}
#endif
