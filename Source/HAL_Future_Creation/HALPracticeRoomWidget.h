#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HALPracticeRoomWidget.generated.h"

/** Native greybox controls. Only submits local Controller intents; no spawning or gameplay state. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALPracticeRoomWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
private:
	void RefreshInput();
	void CycleVehicle(int32 Direction);
	FText VehicleText() const;
	FText StatusText() const;
	FText SeatText(int32 Seat) const;
	FSlateColor SeatColor(int32 Seat) const;
	bool CanSelect() const;
	bool CanConfirm() const;
	bool CanStart() const;
	bool MenuVisible() const;
	bool bInputModeKnown = false, bUsingMenu = false;
	FTimerHandle InputRefreshTimer;
};
