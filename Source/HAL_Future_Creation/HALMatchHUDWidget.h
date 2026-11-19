#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HALUltimateEnergy.h"
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
	UFUNCTION(BlueprintPure, Category="Match|HUD") FHALUltimateEnergyView GetLocalUltimateEnergy() const;
protected:
	/** Disable if the Designer layout renders its own meter from GetLocalUltimateEnergy. */
	UPROPERTY(EditDefaultsOnly, Category="Match|HUD") bool bShowNativeUltimateMeter = true;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	TSharedRef<SWidget> MakeNativeClock();
	void RefreshCountdown();
	FTimerHandle RefreshTimer;
	TSharedPtr<class STextBlock> CountdownText;
	TSharedPtr<SWidget> ClockPanel, EnergyPanel;
	TSharedPtr<class STextBlock> EnergyText;
	TSharedPtr<class SProgressBar> EnergyBar;
};
