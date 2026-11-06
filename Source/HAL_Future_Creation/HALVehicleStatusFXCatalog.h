#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "HALVehicleStatusFXCatalog.generated.h"

class UHALVehicleStatusFXDefinition;

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALStatusFXCatalogEntry
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect", meta=(Categories="State.Visual")) FGameplayTag Tag;
	/** Hard references deliberately make all recipes/resources reachable from the cooked catalog. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect") TObjectPtr<UHALVehicleStatusFXDefinition> Definition;
};

/** Exact visual-tag mapping. Authoring order never selects a parent/first-match recipe. */
UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALVehicleStatusFXCatalog : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects", meta=(TitleProperty="Tag")) TArray<FHALStatusFXCatalogEntry> Effects;
	bool Validate(FString& Error) const;
	const UHALVehicleStatusFXDefinition* FindExact(FGameplayTag Tag) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
