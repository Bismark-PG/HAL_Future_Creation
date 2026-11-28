#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HALRecoveryPromptWidget.generated.h"

/** Local-player overlay beside the overturned chassis. Created only during a recovery attempt. */
UCLASS(NotBlueprintable)
class HAL_FUTURE_CREATION_API UHALRecoveryPromptWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Cull,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;
};
