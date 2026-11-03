#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HALFrontEndConfig.generated.h"

class UTexture2D;

/** Local presentation only. Never supplies roster, vehicle classes or match rules. */
UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALFrontEndConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Brand") FText Title = FText::FromString(TEXT("HAL FUTURE CREATION"));
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Brand") FText Subtitle = FText::FromString(TEXT("BUMPER CARS / DODGEBALL / ARENA"));
	/** Optional artwork; a native background and title remain usable without either image. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Brand") TObjectPtr<UTexture2D> Background;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Brand") TObjectPtr<UTexture2D> Logo;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Style") FLinearColor Accent = FLinearColor(.12f,.85f,.9f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Connection", meta=(ClampMin="5",ClampMax="120",Units="s")) float ConnectionTimeoutSeconds = 30;
};
