#include "HALInputHintsWidget.h"

#include "Components/PanelWidget.h"
#include "Engine/LocalPlayer.h"

void UHALInputHintsWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyInputDevice(IsDesignTime()?PreviewInputDevice:CurrentDevice);
}

void UHALInputHintsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	auto* Player=GetOwningLocalPlayer();
	BindInputSource(Player?Player->GetSubsystem<UHALInputHintSubsystem>():nullptr);
}

void UHALInputHintsWidget::NativeDestruct()
{
	UnbindInputSource();
	Super::NativeDestruct();
}

void UHALInputHintsWidget::BindInputSource(UHALInputHintSubsystem* Source)
{
	UnbindInputSource(); BoundInputSource=Source;
	if (Source) { DeviceChangedHandle=Source->OnInputDeviceChanged.AddUObject(this,&ThisClass::ApplyInputDevice); }
	ApplyInputDevice(Source?Source->GetCurrentInputDevice():EHALInputHintDevice::KeyboardMouse);
}

void UHALInputHintsWidget::UnbindInputSource()
{
	if (auto* Source=BoundInputSource.Get()) { Source->OnInputDeviceChanged.Remove(DeviceChangedHandle); }
	DeviceChangedHandle.Reset(); BoundInputSource.Reset();
}

void UHALInputHintsWidget::ApplyInputDevice(EHALInputHintDevice Device)
{
	CurrentDevice=Device;
	if (KeyboardPanel) { KeyboardPanel->SetVisibility(Device==EHALInputHintDevice::KeyboardMouse?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed); }
	if (XboxPanel) { XboxPanel->SetVisibility(Device==EHALInputHintDevice::Gamepad?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed); }
}
