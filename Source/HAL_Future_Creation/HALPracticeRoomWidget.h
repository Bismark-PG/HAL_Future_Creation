#pragma once
#include "CoreMinimal.h"
#include "HALMenuWidgetBase.h"
#include "HALFrontEndViewTypes.h"
#include "HALPracticeRoomWidget.generated.h"

/** Native greybox controls. Only submits local Controller intents; no spawning or gameplay state. */
UCLASS(Blueprintable)
class HAL_FUTURE_CREATION_API UHALPracticeRoomWidget : public UHALMenuWidgetBase
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void BackMenu() override;
	virtual bool HorizontalMenu(int32 Direction) override;
	/** GameInstance calls this on replicated room changes; the timer still handles late assets/input binding. */
	void RefreshFromRoomState();
	UFUNCTION(BlueprintPure, Category="UI|Room") FHALRoomView GetRoomView() const;
	UFUNCTION(BlueprintCallable, Category="UI|Room") void SelectVehicleById(FName VehicleId);
	UFUNCTION(BlueprintCallable, Category="UI|Room") void CycleVehicle(int32 Direction);
	UFUNCTION(BlueprintCallable, Category="UI|Room") void ConfirmSelection();
	UFUNCTION(BlueprintCallable, Category="UI|Room") void CancelSelection();
	UFUNCTION(BlueprintCallable, Category="UI|Room") void StartRoomMatch();
	UFUNCTION(BlueprintCallable, Category="UI|Room") void ResumeDriving();
	UFUNCTION(BlueprintCallable, Category="UI|Room") void LeaveCurrentRoom();
	UFUNCTION(BlueprintCallable, Category="UI|Room") void ReturnToRoom();
	UFUNCTION(BlueprintCallable, Category="UI|Match") void CycleSpectator(int32 Direction);
	UFUNCTION(BlueprintNativeEvent, Category="UI|Room") void ReceiveRoomView(const FHALRoomView& View);
	virtual void ReceiveRoomView_Implementation(const FHALRoomView& View) {}
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
private:
	void PublishRoomView();
	bool CanCancel() const;
	bool CanResume() const;
	bool CanReturnToRoom() const;
	void RefreshInput();
	/** UI ownership depends on lifecycle initialization, not a transient physics recovery. */
	bool ShouldUseMenuInput();
	void RefreshPresentation();
	FText VehicleText() const;
	FText DescriptionText() const;
	FText StatusText() const;
	FText MatchResultText() const;
	FText DrivingText() const;
	FText SeatText(int32 Seat) const;
	FSlateColor SeatColor(int32 Seat) const;
	bool CanSelect() const;
	bool CanConfirm() const;
	bool CanStart() const;
	bool MenuVisible() const;
	bool bInputModeKnown = false, bUsingMenu = false;
	bool bRefreshingView=false;
	bool bHasPublishedView=false;
	uint8 LastInputDiagnostic=MAX_uint8;
	UPROPERTY(Transient) FHALRoomView PublishedView;
	TWeakObjectPtr<APawn> InputAvatar;
	UPROPERTY(Transient) TObjectPtr<class UHALVehicleCatalog> LoadedCatalog;
	UPROPERTY(Transient) TObjectPtr<class UTexture2D> LoadedPreview;
	FSlateBrush PreviewBrush;
	FSoftObjectPath CatalogPath,ImagePath;
	TSharedPtr<struct FStreamableHandle> CatalogLoad,ImageLoad;
	FTimerHandle InputRefreshTimer;
};
