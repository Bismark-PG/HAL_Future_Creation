#pragma once
#include "CoreMinimal.h"
#include "HALMenuWidgetBase.h"
#include "HALArcadeEntryWidget.generated.h"

class SEditableTextBox;

/** Native fallback is complete without Widget Blueprint or artwork. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALArcadeEntryWidget : public UHALMenuWidgetBase
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void BackMenu() override;
	bool IsAttractPage() const { return bAttract; }
private:
	friend struct FHALFrontEndTestAccess;
	void OpenConnections();
	void RefreshEntry();
	void Join();
	bool bAttract=true,bLastBusy=false;
	TSharedPtr<SEditableTextBox> Address;
	FSlateBrush BackgroundBrush,LogoBrush;
	FTimerHandle RefreshTimer;
};
