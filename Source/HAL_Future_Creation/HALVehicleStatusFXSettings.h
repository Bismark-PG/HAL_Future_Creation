#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HALVehicleStatusFXSettings.generated.h"

class UHALVehicleStatusFXCatalog;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Vehicle Status FX"))
class HAL_FUTURE_CREATION_API UHALVehicleStatusFXSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	/** None disables the new shared FX path. Ordinary vehicles inherit this catalog. */
	UPROPERTY(Config, EditAnywhere, Category="Shared Configuration") TSoftObjectPtr<UHALVehicleStatusFXCatalog> DefaultCatalog;
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
#if WITH_EDITOR
	/** Cook-only synchronous validation. Runtime hits never load assets synchronously. */
	bool CollectCookPackages(TArray<FName>& Packages, FString& Error) const;
#endif
};
