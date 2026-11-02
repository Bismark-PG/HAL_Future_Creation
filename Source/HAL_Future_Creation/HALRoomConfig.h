#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HALRoomTypes.h"
#include "HALRoomConfig.generated.h"

class UHALVehicleCatalog;

UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALRoomConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Room") FHALRoomRules Rules;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Room") TObjectPtr<UHALVehicleCatalog> Catalog;
	bool Validate(FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
