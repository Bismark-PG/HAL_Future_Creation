#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HALRoomTypes.h"
#include "HALRoomConfig.generated.h"

class UHALVehicleCatalog;
class UHALMatchFeverDefinition;

UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALRoomConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Room") FHALRoomRules Rules;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Room") TObjectPtr<UHALVehicleCatalog> Catalog;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Match Rules", meta=(ShowOnlyInnerProperties)) FHALMatchRules MatchRules;
	/** Optional shared Fever settings for matches using this room profile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Match Rules") TObjectPtr<UHALMatchFeverDefinition> MatchFever;
	/** Starting points for each formal round. Practice always starts full. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Match Rules", meta=(ClampMin="0", ClampMax="100"))
	float StartingUltimateEnergy = 0.f;
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
