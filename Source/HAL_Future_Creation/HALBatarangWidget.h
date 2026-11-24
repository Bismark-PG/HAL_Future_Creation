#pragma once
#include "Blueprint/UserWidget.h"
#include "HALBatarangWidget.generated.h"

class AHALBatarang;
class ATestVehiclePawn;

USTRUCT(BlueprintType)
struct FHALBatarangPinView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Batarang") bool bPinned = false;
	UPROPERTY(BlueprintReadOnly, Category="Batarang") bool bCanDetonate = false;
	UPROPERTY(BlueprintReadOnly, Category="Batarang") float RemainingSeconds = 0;
	/** 0..1 synchronised to the embedded blade. Drives the wings even when the bearing is hidden. */
	UPROPERTY(BlueprintReadOnly, Category="Batarang") float FusePulse = 0;
	/** Cleared together with PushDirection while the mark is concealed. */
	UPROPERTY(BlueprintReadOnly, Category="Batarang") TObjectPtr<ATestVehiclePawn> VisibleVehicle;
	UPROPERTY(BlueprintReadOnly, Category="Batarang") FVector PushDirection = FVector::ZeroVector;
};

/** Private to the caster. A designer widget with a root replaces the native artwork. */
UCLASS(Blueprintable)
class HAL_FUTURE_CREATION_API UHALBatarangWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<AHALBatarang> Dart;
	UFUNCTION(BlueprintPure, Category="Batarang|UI") FHALBatarangPinView GetPinView() const;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Cull,
		FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool bEnabled) const override;
private:
	bool bUseNativeArt = true;
};
