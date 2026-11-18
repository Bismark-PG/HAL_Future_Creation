#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HALMatchHUDWidget.generated.h"

/** Passive local match HUD. Reads the room's server deadline; never controls match/input state. */
UCLASS(Blueprintable)
class HAL_FUTURE_CREATION_API UHALMatchHUDWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	UFUNCTION(BlueprintPure, Category="Match|HUD") FText GetCountdownText() const;
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	void RefreshCountdown();
	FTimerHandle RefreshTimer;
	TSharedPtr<class STextBlock> CountdownText;
};
