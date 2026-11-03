#pragma once
#include "CoreMinimal.h"
#include "HALMenuWidgetBase.h"
#include "HALFrontEndViewTypes.h"
#include "HALArcadeEntryWidget.generated.h"

class SEditableTextBox;

/** Native fallback is complete without Widget Blueprint or artwork. */
UCLASS(Blueprintable)
class HAL_FUTURE_CREATION_API UHALArcadeEntryWidget : public UHALMenuWidgetBase
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void BackMenu() override;
	bool IsAttractPage() const { return bAttract; }
	UFUNCTION(BlueprintPure, Category="UI|Entry") FHALEntryView GetEntryView() const;
	UFUNCTION(BlueprintCallable, Category="UI|Entry") void OpenConnections();
	UFUNCTION(BlueprintCallable, Category="UI|Entry") bool HostRoom();
	UFUNCTION(BlueprintCallable, Category="UI|Entry") bool JoinRoom(const FString& HostIPv4);
	UFUNCTION(BlueprintCallable, Category="UI|Entry") void CancelConnection();
	UFUNCTION(BlueprintNativeEvent, Category="UI|Entry") void ReceiveEntryView(const FHALEntryView& View);
	virtual void ReceiveEntryView_Implementation(const FHALEntryView& View) {}
private:
	friend struct FHALFrontEndTestAccess;
	void PublishEntryView();
	void RefreshEntry();
	void Join();
	bool bAttract=true,bLastBusy=false;
	bool bHasPublishedView=false;
	UPROPERTY(Transient) FHALEntryView PublishedView;
	TSharedPtr<SEditableTextBox> Address;
	FSlateBrush BackgroundBrush,LogoBrush;
	FTimerHandle RefreshTimer;
};
