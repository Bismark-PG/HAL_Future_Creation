#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HALInputHintSubsystem.h"
#include "HALInputHintsWidget.generated.h"

class UPanelWidget;

/** Passive, reusable hints in PreMatchFlow and MatchFlow. The Designer owns each page's text and glyphs. */
UCLASS(Blueprintable)
class HAL_FUTURE_CREATION_API UHALInputHintsWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="UI|Input Hints") EHALInputHintDevice GetCurrentInputDevice() const { return CurrentDevice; }
protected:
	/** Both named panels are required in Designer subclasses. Only their visibility is changed. */
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UPanelWidget> KeyboardPanel;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UPanelWidget> XboxPanel;
	/** Designer preview only. Runtime immediately uses the owning local player's latest input. */
	UPROPERTY(EditAnywhere, Category="Input Hints|Preview") EHALInputHintDevice PreviewInputDevice=EHALInputHintDevice::KeyboardMouse;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	friend struct FHALInputHintsTestAccess;
	void BindInputSource(UHALInputHintSubsystem* Source);
	void UnbindInputSource();
	void ApplyInputDevice(EHALInputHintDevice Device);
	TWeakObjectPtr<UHALInputHintSubsystem> BoundInputSource;
	FDelegateHandle DeviceChangedHandle;
	EHALInputHintDevice CurrentDevice=EHALInputHintDevice::KeyboardMouse;
};
