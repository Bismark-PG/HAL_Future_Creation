#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HALVehicleCatalog.generated.h"

class ATestVehiclePawn;
class UTexture2D;
class USkeletalMesh;

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALVehicleCatalogEntry
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle") FName VehicleId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle") TSoftClassPtr<ATestVehiclePawn> VehicleClass;
	/** Optional presentation metadata; it does not decide driving/config validity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Display") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Display") TSoftObjectPtr<UTexture2D> Icon;
	/** Optional static selection artwork, loaded locally. Never spawns a preview Pawn. Icon is the fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Display") TSoftObjectPtr<UTexture2D> PreviewImage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Display", meta=(MultiLine=true)) FText Description;
	/** Editor-baked, surface-area-weighted LOD0 Base Color. Rebuild after changing the mesh/materials. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Display|HUD") FLinearColor HUDDominantColor = FLinearColor::White;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Display|HUD") bool bHasHUDDominantColor = false;
	/** Provenance only; runtime HUD does not load the mesh to choose an outline colour. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Display|HUD") TSoftObjectPtr<USkeletalMesh> HUDColorSource;
};

/** Server allowlist. Entries refer to existing BP->Definition; no copied vehicle tuning. */
UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALVehicleCatalog : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicles", meta=(TitleProperty="VehicleId")) TArray<FHALVehicleCatalogEntry> Vehicles;
	bool Validate(FString& Error) const;
	UClass* Resolve(FName VehicleId, FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
