#pragma once
#include "CoreMinimal.h"
#include "HALMenuWidgetBase.h"
#include "HALPracticeRoomWidget.generated.h"

/** Native greybox controls. Only submits local Controller intents; no spawning or gameplay state. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALPracticeRoomWidget : public UHALMenuWidgetBase
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void BackMenu() override;
	virtual bool HorizontalMenu(int32 Direction) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
private:
	void RefreshInput();
	void RefreshPresentation();
	void CycleVehicle(int32 Direction);
	FText VehicleText() const;
	FText DescriptionText() const;
	FText StatusText() const;
	FText SeatText(int32 Seat) const;
	FSlateColor SeatColor(int32 Seat) const;
	bool CanSelect() const;
	bool CanConfirm() const;
	bool CanStart() const;
	bool MenuVisible() const;
	bool bInputModeKnown = false, bUsingMenu = false;
	TWeakObjectPtr<APawn> InputAvatar;
	UPROPERTY(Transient) TObjectPtr<class UHALVehicleCatalog> LoadedCatalog;
	UPROPERTY(Transient) TObjectPtr<class UTexture2D> LoadedPreview;
	FSlateBrush PreviewBrush;
	FSoftObjectPath CatalogPath,ImagePath;
	TSharedPtr<struct FStreamableHandle> CatalogLoad,ImageLoad;
	FTimerHandle InputRefreshTimer;
};
