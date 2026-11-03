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
	virtual void NativeDestruct() override;
	/** Widget Blueprint subclasses own their entire Designer tree. Native classes keep the Slate fallback. */
	UFUNCTION(BlueprintPure, Category="UI|Layout") virtual bool UsesDesignerLayout() const;
	/** Preferred control for the active panel. Stored locally; automatic focus waits for layout readiness. */
	UFUNCTION(BlueprintCallable, Category="UI|Focus") void SetMenuFocusTarget(UWidget* Target);
	UFUNCTION(BlueprintCallable, Category="UI|Actions") void RequestMenuBack() { BackMenu(); }
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
	bool HasVisibleDesignerFocus() const;
	UPROPERTY(Transient) TObjectPtr<UWidget> DesignerFocusTarget;
	struct FControl { TWeakPtr<SWidget> Widget; FSimpleDelegate Action; };
	TArray<FControl> Controls;
	int32 FocusIndex=INDEX_NONE;
};
