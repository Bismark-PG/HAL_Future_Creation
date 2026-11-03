#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HALMenuWidgetBase.generated.h"

/** Small local focus ring shared by the native entry and room. No gameplay/input mappings. */
UCLASS(Abstract)
class HAL_FUTURE_CREATION_API UHALMenuWidgetBase : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
	virtual FNavigationReply NativeOnNavigation(const FGeometry& Geometry,const FNavigationEvent& Event,const FNavigationReply& Default) override;
	void FocusMenu(int32 Direction=0);
	bool ActivateMenu();
protected:
	void RegisterMenuControl(const TSharedRef<SWidget>& Widget,FSimpleDelegate Action);
	void ResetMenuControls();
	virtual void BackMenu() {}
	virtual bool HorizontalMenu(int32 Direction) { return false; }
	bool IsMenuControlFocused() const;
private:
	struct FControl { TWeakPtr<SWidget> Widget; FSimpleDelegate Action; };
	TArray<FControl> Controls;
	int32 FocusIndex=INDEX_NONE;
};
