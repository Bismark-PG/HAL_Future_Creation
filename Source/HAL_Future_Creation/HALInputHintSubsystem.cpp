#include "HALInputHintSubsystem.h"

#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SViewport.h"

DEFINE_LOG_CATEGORY_STATIC(LogHALInputHints,Log,All);

/** Observe before UMG/GameOnly routing, and always let the original event continue. */
class FHALInputHintProcessor final : public IInputProcessor
{
public:
	explicit FHALInputHintProcessor(UHALInputHintSubsystem* InOwner):Owner(InOwner) {}
	virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override {}
	virtual const TCHAR* GetDebugName() const override { return TEXT("HALInputHints"); }
	virtual bool HandleKeyDownEvent(FSlateApplication& App,const FKeyEvent& Event) override
	{
		if (auto* S=Owner.Get(); S && S->IsRelevantInput(App,Event)) { S->RecordKey(Event.GetKey(),Event.IsRepeat()); }
		return false;
	}
	virtual bool HandleAnalogInputEvent(FSlateApplication& App,const FAnalogInputEvent& Event) override
	{
		if (auto* S=Owner.Get(); S && S->IsRelevantInput(App,Event)) { S->RecordAnalog(Event.GetKey(),Event.GetAnalogValue()); }
		return false;
	}
	virtual bool HandleMouseMoveEvent(FSlateApplication& App,const FPointerEvent& Event) override
	{
		if (auto* S=Owner.Get(); S && S->IsRelevantInput(App,Event,&Event)) { S->RecordMouseMovement(Event.GetCursorDelta()); }
		return false;
	}
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& App,const FPointerEvent& Event) override
	{
		if (auto* S=Owner.Get(); S && S->IsRelevantInput(App,Event,&Event)) { S->RecordKey(Event.GetEffectingButton(),false); }
		return false;
	}
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& App,const FPointerEvent& Event) override
	{ return HandleMouseButtonDownEvent(App,Event); }
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& App,const FPointerEvent& Event,const FPointerEvent*) override
	{
		if (auto* S=Owner.Get(); S && !FMath::IsNearlyZero(Event.GetWheelDelta()) && S->IsRelevantInput(App,Event,&Event))
		{ S->SetCurrentDevice(EHALInputHintDevice::KeyboardMouse); }
		return false;
	}
private:
	TWeakObjectPtr<UHALInputHintSubsystem> Owner;
};

void UHALInputHintSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (FSlateApplication::IsInitialized())
	{
		InputProcessor=MakeShared<FHALInputHintProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor,EInputPreProcessorType::PreGame);
	}
}

void UHALInputHintSubsystem::Deinitialize()
{
	if (InputProcessor && FSlateApplication::IsInitialized()) { FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor); }
	InputProcessor.Reset(); ActiveAnalogDirections.Reset(); OnInputDeviceChanged.Clear();
	Super::Deinitialize();
}

bool UHALInputHintSubsystem::IsRelevantInput(FSlateApplication& App,const FInputEvent& Event,const FPointerEvent* Pointer) const
{
	const auto* Player=GetLocalPlayer();
	const auto* Viewport=Player?Player->ViewportClient.Get():nullptr;
	if (!Viewport || !Viewport->GetWorld() || !Viewport->GetWorld()->IsGameWorld() || !App.IsActive()) { return false; }
	const auto User=Player->GetSlateUser();
	const auto ViewportWidget=Viewport->GetGameViewportWidget();
	if (!User || User->GetUserIndex()!=Event.GetUserIndex() || !ViewportWidget) { return false; }
	if (Pointer)
	{
		if (Pointer->IsTouchEvent()) { return false; }
		const auto Path=App.LocateWindowUnderMouse(Pointer->GetScreenSpacePosition(),App.GetInteractiveTopLevelWindows(),false,Event.GetUserIndex());
		return Path.ContainsWidget(ViewportWidget.Get());
	}
	const auto Focused=App.GetUserFocusedWidget(Event.GetUserIndex()); FWidgetPath Path;
	return Focused && App.GeneratePathToWidgetUnchecked(Focused.ToSharedRef(),Path) && Path.ContainsWidget(ViewportWidget.Get());
}

void UHALInputHintSubsystem::RecordKey(const FKey& Key,bool bRepeat)
{
	if (!Key.IsValid() || Key.IsAnalog() || bRepeat) { return; }
	SetCurrentDevice(Key.IsGamepadKey()?EHALInputHintDevice::Gamepad:EHALInputHintDevice::KeyboardMouse);
}

void UHALInputHintSubsystem::RecordAnalog(const FKey& Key,float Value)
{
	if (!Key.IsGamepadKey() || !FMath::IsFinite(Value)) { return; }
	const float Activation=FMath::Clamp(AnalogActivationThreshold,.01f,1.f);
	const float Release=FMath::Clamp(AnalogReleaseThreshold,0.f,Activation);
	int8& Direction=ActiveAnalogDirections.FindOrAdd(Key);
	if (FMath::Abs(Value)<=Release) { Direction=0; return; }
	if (FMath::Abs(Value)<Activation) { return; }
	const int8 NewDirection=Value>0?1:-1;
	// A held trigger/stick cannot immediately steal the hints back after a keyboard press.
	if (Direction!=NewDirection) { Direction=NewDirection; SetCurrentDevice(EHALInputHintDevice::Gamepad); }
}

void UHALInputHintSubsystem::RecordMouseMovement(const FVector2D& Delta)
{
	if (Delta.ContainsNaN() || Delta.IsNearlyZero()) { return; }
	const float Minimum=FMath::Max(0.f,MinimumMouseMovement);
	if (Delta.SizeSquared()>=FMath::Square(Minimum)) { SetCurrentDevice(EHALInputHintDevice::KeyboardMouse); }
}

void UHALInputHintSubsystem::SetCurrentDevice(EHALInputHintDevice Device)
{
	if (CurrentDevice==Device) { return; }
	CurrentDevice=Device;
	UE_LOG(LogHALInputHints,Verbose,TEXT("%s hints: %s"),*GetNameSafe(GetLocalPlayer()),Device==EHALInputHintDevice::Gamepad?TEXT("Xbox"):TEXT("KeyboardMouse"));
	OnInputDeviceChanged.Broadcast(Device);
}
