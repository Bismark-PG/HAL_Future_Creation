#pragma once
#include "Blueprint/UserWidget.h"
#include "HALBatarangWidget.generated.h"

class AHALBatarang;

/** Local caster's mark. The world-space dart itself is visible to all participants. */
UCLASS()
class UHALBatarangWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<AHALBatarang> Dart;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Cull,
		FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool bEnabled) const override;
};
