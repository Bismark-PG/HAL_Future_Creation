#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HALCameraDebugWidget.generated.h"

/** Native read-only overlay; no Blueprint layout or gameplay mutation. */
UCLASS(NotBlueprintable)
class HAL_FUTURE_CREATION_API UHALCameraDebugWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
};
