#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HALVehicleHUDStyleLibrary.generated.h"

class UImage;
class UHALVehicleCatalog;

namespace HALVehicleHUD
{
	struct FColorAreaSample { FColor Color; double Area = 0; };
	/** RGB bins merge small texture variations; ties always use the lowest RGB key. */
	HAL_FUTURE_CREATION_API bool DominantColor(TConstArrayView<FColorAreaSample> Samples, FLinearColor& OutColor);
	HAL_FUTURE_CREATION_API FLinearColor BrightOutline(FLinearColor Color);
}

/** Cosmetic UI only. Uses the participant's selected VehicleId, never the local player's Pawn. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALVehicleHUDStyleLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** False uses neutral white, including missing IDs/cache, so a reused row cannot keep the previous car's tint. */
	UFUNCTION(BlueprintPure, Category="Vehicle|HUD")
	static bool GetVehicleOutlineColor(const UHALVehicleCatalog* Catalog, FName VehicleId, FLinearColor& Color);

	/** The Image brush must use the supplied UI outline material or one of its instances. Changes only OutlineColor. */
	UFUNCTION(BlueprintCallable, Category="Vehicle|HUD")
	static bool ApplyVehicleOutline(UImage* Background, const UHALVehicleCatalog* Catalog, FName VehicleId);
};
