#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HALVehicleCatalog.generated.h"

class ATestVehiclePawn;
class UTexture2D;

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALVehicleCatalogEntry
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle") FName VehicleId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle") TSoftClassPtr<ATestVehiclePawn> VehicleClass;
	/** Optional presentation metadata; it does not decide driving/config validity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Display") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Display") TSoftObjectPtr<UTexture2D> Icon;
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
