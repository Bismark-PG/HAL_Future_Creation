#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "HALVehicleStatusFXSubsystem.generated.h"

class UHALVehicleStatusFXCatalog;
class UHALVehicleStatusFXDefinition;
class UVehicleDefinition;
struct FStreamableHandle;

UENUM()
enum class EHALStatusFXLoadState : uint8 { Unconfigured, NotRequested, Loading, Ready, Failed };

/** Per-GameInstance immutable catalog lease. World travel does not discard preloaded shared assets. */
USTRUCT()
struct FHALStatusFXCatalogRecord
{
	GENERATED_BODY()
	UPROPERTY(Transient) TObjectPtr<UHALVehicleStatusFXCatalog> Catalog;
	EHALStatusFXLoadState State = EHALStatusFXLoadState::Loading;
	FString Error;
	TSharedPtr<FStreamableHandle> Handle;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FHALStatusFXCatalogChanged, const FSoftObjectPath&, EHALStatusFXLoadState);

/** Shared asset preparation only. Owns no MID/Niagara instances or authoritative gameplay state. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALVehicleStatusFXSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	static TSoftObjectPtr<UHALVehicleStatusFXCatalog> SelectCatalog(const UVehicleDefinition* Vehicle);
	void PrepareForVehicle(const UVehicleDefinition* Vehicle);
	/** Idempotent asynchronous request; failed requests remain failed until a new GameInstance. */
	void PrepareCatalog(TSoftObjectPtr<UHALVehicleStatusFXCatalog> Catalog);
	EHALStatusFXLoadState GetCatalogState(TSoftObjectPtr<UHALVehicleStatusFXCatalog> Catalog, FString& OutError) const;
	/** Cache-only lookup. Never starts loading at hit/Tag time. */
	const UHALVehicleStatusFXDefinition* FindEffect(FGameplayTag Tag, const UVehicleDefinition* Vehicle = nullptr) const;
	FHALStatusFXCatalogChanged& OnCatalogChanged() { return CatalogChanged; }
	void LogStatus() const;
private:
	void CompleteCatalog(const FSoftObjectPath& Path);
	UPROPERTY(Transient) TMap<FSoftObjectPath, FHALStatusFXCatalogRecord> Catalogs;
	FHALStatusFXCatalogChanged CatalogChanged;
	bool bStopped = false;
};
