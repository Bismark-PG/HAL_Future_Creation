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
	/** Opt in after creating a distinct match map; legacy GF-3 remains lock-only until configured. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Round Travel") bool bEnableRoundTravel=false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Round Travel", meta=(EditCondition="bEnableRoundTravel")) TSoftObjectPtr<UWorld> MatchMap;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Round Travel", meta=(Units="s",ClampMin="0.5",ClampMax="10")) float CountdownSeconds=3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Round Travel", meta=(Units="s",ClampMin="1",ClampMax="180")) float LoadTimeoutSeconds=45;
	bool Validate(FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
