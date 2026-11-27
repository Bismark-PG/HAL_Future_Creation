#pragma once

#include "CoreMinimal.h"
#include "HALGameFlowWidgets.h"
#include "HALUltimateEnergy.h"
#include "HALMatchHUDWidget.generated.h"

/** Passive local match HUD. Reads the room's server deadline; never controls match/input state. */
UCLASS(Blueprintable)
class HAL_FUTURE_CREATION_API UHALMatchHUDWidget : public UHALGameFlowScreenWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	UFUNCTION(BlueprintPure, Category="Match|HUD") FText GetCountdownText() const;
	/** The room root supplies this snapshot; standalone compatibility HUDs continue reading the server deadline. */
	UFUNCTION(BlueprintPure, Category="Match|HUD") FHALRoomView GetMatchView() const { return GetScreenView(); }
	UFUNCTION(BlueprintImplementableEvent, Category="Match|HUD") void ReceiveMatchView(const FHALRoomView& View);
	UFUNCTION(BlueprintPure, Category="Match|HUD") FHALUltimateEnergyView GetLocalUltimateEnergy() const;
protected:
	/** Disable if the Designer layout renders its own meter from GetLocalUltimateEnergy. */
	UPROPERTY(EditDefaultsOnly, Category="Match|HUD") bool bShowNativeUltimateMeter = true;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool UsesPanelPresentation() const override { return false; }
	virtual void OnRoomViewApplied(const FHALRoomView& View) override;
	virtual void OnScreenActiveChanged(bool bActive) override;
private:
	friend struct FHALFeverNetworkTestAccess;
	TSharedRef<SWidget> MakeNativeClock();
	void RefreshCountdown();
	FTimerHandle RefreshTimer;
	TSharedPtr<class STextBlock> CountdownText;
	TSharedPtr<SWidget> ClockPanel, EnergyPanel;
	TSharedPtr<SWidget> FeverBanner, FeverBadge;
	TSharedPtr<class STextBlock> EnergyText;
	TSharedPtr<class SProgressBar> EnergyBar;
};
