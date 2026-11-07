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
	/** Presentation budgets only; templates must honor User.FX_MaxParticles and User.FX_SpawnRate. */
	UPROPERTY(Config, EditAnywhere, Category="Render Budget", meta=(ClampMin="0", ClampMax="8192")) int32 MaximumParticlesPerVehicle = 512;
	UPROPERTY(Config, EditAnywhere, Category="Render Budget", meta=(ClampMin="0", ClampMax="128")) int32 MaximumParticleLayersPerVehicle = 16;
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
#if WITH_EDITOR
	/** Cook-only synchronous validation. Runtime hits never load assets synchronously. */
	bool CollectCookPackages(TArray<FName>& Packages, FString& Error) const;
#endif
};
