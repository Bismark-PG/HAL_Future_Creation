#include "HALVehicleStatusFXSubsystem.h"
#include "HALVehicleStatusFXCatalog.h"
#include "HALVehicleStatusFXDefinition.h"
#include "HALVehicleStatusFXSettings.h"
#include "VehicleDefinition.h"
#include "TestVehiclePawn.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

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
	FAutoConsoleCommandWithWorld GHALStatusFXStatus(TEXT("hal.StatusFXStatus"), TEXT("Print shared status-FX catalog readiness and each live vehicle's effective catalog."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			auto* GI = World ? World->GetGameInstance() : nullptr;
			auto* FX = GI ? GI->GetSubsystem<UHALVehicleStatusFXSubsystem>() : nullptr;
			if (!FX) { UE_LOG(LogHALStatusFX, Display, TEXT("StatusFX: no local GameInstance subsystem (run in a game window).")); return; }
			FX->LogStatus();
			for (TActorIterator<ATestVehiclePawn> It(World); It; ++It)
			{
				const auto Catalog = UHALVehicleStatusFXSubsystem::SelectCatalog(It->GetVehicleDefinition());
				FString Error;
				const auto State = FX->GetCatalogState(Catalog, Error);
				UE_LOG(LogHALStatusFX, Display, TEXT("StatusFX Vehicle=%s Catalog=%s State=%s Error=%s"), *It->GetName(), *Catalog.ToString(),
					*StaticEnum<EHALStatusFXLoadState>()->GetNameStringByValue(int64(State)), *Error);
			}
		}));
}
#endif
