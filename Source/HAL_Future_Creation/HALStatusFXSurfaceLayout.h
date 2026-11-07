#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HALStatusFXSurfaceLayout.generated.h"
class UMaterialInterface;

USTRUCT(BlueprintType)
struct FHALStatusFXSurfaceChannel
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Channel") FName Id;
	/** E.g. FX0_: Intensity/Age/Residue and recipe parameters are appended to this prefix. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Channel") FName Prefix;
};

/** Shared material contract. Defaults to two display channels; never caps logical state count. */
UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALStatusFXSurfaceLayout : public UDataAsset
{
	GENERATED_BODY()
public:
	UHALStatusFXSurfaceLayout();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface") TObjectPtr<UMaterialInterface> Material;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface", meta=(TitleProperty="Id")) TArray<FHALStatusFXSurfaceChannel> Channels;
	bool Validate(FString& Error) const;
};
