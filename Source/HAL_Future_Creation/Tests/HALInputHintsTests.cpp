#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../HALInputHintSubsystem.h"
#include "../HALInputHintsWidget.h"
#include "Components/HorizontalBox.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "UObject/StrongObjectPtr.h"

struct FHALInputHintsTestAccess
{
	static void Key(UHALInputHintSubsystem* S,const FKey& K,bool Repeat=false) { S->RecordKey(K,Repeat); }
	static void Axis(UHALInputHintSubsystem* S,const FKey& K,float V) { S->RecordAnalog(K,V); }
	static void Mouse(UHALInputHintSubsystem* S,const FVector2D& Delta) { S->RecordMouseMovement(Delta); }
	static void ConfigurePanels(UHALInputHintsWidget* W)
	{
		W->KeyboardPanel=NewObject<UHorizontalBox>(W,TEXT("KeyboardPanel"));
		W->XboxPanel=NewObject<UHorizontalBox>(W,TEXT("XboxPanel"));
	}
	static void Bind(UHALInputHintsWidget* W,UHALInputHintSubsystem* S) { W->BindInputSource(S); }
	static void Destruct(UHALInputHintsWidget* W) { W->NativeDestruct(); }
	static bool Shows(UHALInputHintsWidget* W,EHALInputHintDevice Device)
	{
		return W->KeyboardPanel->GetVisibility()==(Device==EHALInputHintDevice::KeyboardMouse?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed)
			&& W->XboxPanel->GetVisibility()==(Device==EHALInputHintDevice::Gamepad?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
	}
};

namespace
{
	struct FInputHintFixture
	{
		TStrongObjectPtr<ULocalPlayer> Player{NewObject<ULocalPlayer>(GEngine)};
		TStrongObjectPtr<UHALInputHintSubsystem> Source{NewObject<UHALInputHintSubsystem>(Player.Get())};
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALInputHintSignalsTest,"HAL.FutureCreation.GameFlow.InputHints.SignalFiltering",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALInputHintSignalsTest::RunTest(const FString&)
{
	FInputHintFixture F; auto* S=F.Source.Get(); int32 Changes=0;
	const auto Handle=S->OnInputDeviceChanged.AddLambda([&Changes](EHALInputHintDevice) { ++Changes; });
	FHALInputHintsTestAccess::Axis(S,EKeys::Gamepad_LeftX,.1f);
	FHALInputHintsTestAccess::Axis(S,EKeys::Gamepad_RightTriggerAxis,0);
	TestEqual(TEXT("Idle axes do not select gamepad"),S->GetCurrentInputDevice(),EHALInputHintDevice::KeyboardMouse);
	FHALInputHintsTestAccess::Key(S,EKeys::Gamepad_FaceButton_Bottom);
	FHALInputHintsTestAccess::Key(S,EKeys::Gamepad_FaceButton_Bottom);
	TestEqual(TEXT("One notification per device change, not per action"),Changes,1);
	FHALInputHintsTestAccess::Mouse(S,FVector2D(.5,.5));
	FHALInputHintsTestAccess::Mouse(S,FVector2D::ZeroVector);
	FHALInputHintsTestAccess::Key(S,EKeys::W,true);
	FHALInputHintsTestAccess::Key(S,FKey());
	TestEqual(TEXT("Jitter, invalid keys and held-key repeats leave Xbox hints intact"),S->GetCurrentInputDevice(),EHALInputHintDevice::Gamepad);
	FHALInputHintsTestAccess::Mouse(S,FVector2D(3,0));
	TestEqual(TEXT("Intentional mouse movement selects keyboard/mouse"),S->GetCurrentInputDevice(),EHALInputHintDevice::KeyboardMouse);
	FHALInputHintsTestAccess::Axis(S,EKeys::Gamepad_RightTriggerAxis,.8f);
	TestEqual(TEXT("A real trigger actuation selects Xbox"),S->GetCurrentInputDevice(),EHALInputHintDevice::Gamepad);
	FHALInputHintsTestAccess::Key(S,EKeys::SpaceBar);
	FHALInputHintsTestAccess::Axis(S,EKeys::Gamepad_RightTriggerAxis,.9f);
	TestEqual(TEXT("Holding a trigger cannot immediately steal keyboard hints back"),S->GetCurrentInputDevice(),EHALInputHintDevice::KeyboardMouse);
	FHALInputHintsTestAccess::Axis(S,EKeys::Gamepad_RightTriggerAxis,.2f);
	FHALInputHintsTestAccess::Axis(S,EKeys::Gamepad_RightTriggerAxis,.8f);
	TestEqual(TEXT("The hysteresis band does not rearm the trigger"),S->GetCurrentInputDevice(),EHALInputHintDevice::KeyboardMouse);
	FHALInputHintsTestAccess::Axis(S,EKeys::Gamepad_RightTriggerAxis,.1f);
	FHALInputHintsTestAccess::Axis(S,EKeys::Gamepad_RightTriggerAxis,.8f);
	TestEqual(TEXT("Release and actuate again selects Xbox"),S->GetCurrentInputDevice(),EHALInputHintDevice::Gamepad);
	FHALInputHintsTestAccess::Key(S,EKeys::LeftMouseButton);
	FHALInputHintsTestAccess::Axis(S,EKeys::MouseX,10);
	TestEqual(TEXT("Mouse axes are not misclassified as gamepad axes"),S->GetCurrentInputDevice(),EHALInputHintDevice::KeyboardMouse);
	FHALInputHintsTestAccess::Axis(S,EKeys::Gamepad_LeftX,.7f);
	FHALInputHintsTestAccess::Key(S,EKeys::Enter);
	FHALInputHintsTestAccess::Axis(S,EKeys::Gamepad_LeftX,-.7f);
	TestEqual(TEXT("A deliberate stick reversal selects Xbox"),S->GetCurrentInputDevice(),EHALInputHintDevice::Gamepad);
	S->OnInputDeviceChanged.Remove(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALInputHintIsolationTest,"HAL.FutureCreation.GameFlow.InputHints.LocalPlayerIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALInputHintIsolationTest::RunTest(const FString&)
{
	FInputHintFixture First,Second;
	FHALInputHintsTestAccess::Key(First.Source.Get(),EKeys::Gamepad_FaceButton_Bottom);
	TestEqual(TEXT("The owning player switches to Xbox"),First.Source->GetCurrentInputDevice(),EHALInputHintDevice::Gamepad);
	TestEqual(TEXT("Another local player's presentation is independent"),Second.Source->GetCurrentInputDevice(),EHALInputHintDevice::KeyboardMouse);
	FHALInputHintsTestAccess::Key(Second.Source.Get(),EKeys::Gamepad_FaceButton_Right);
	FHALInputHintsTestAccess::Key(First.Source.Get(),EKeys::W);
	TestEqual(TEXT("Keyboard input cannot overwrite another player's Xbox state"),Second.Source->GetCurrentInputDevice(),EHALInputHintDevice::Gamepad);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALInputHintWidgetsTest,"HAL.FutureCreation.GameFlow.InputHints.SharedWidgetLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALInputHintWidgetsTest::RunTest(const FString&)
{
	FInputHintFixture F;
	TStrongObjectPtr<UHALInputHintsWidget> PreMatch(NewObject<UHALInputHintsWidget>()),Match(NewObject<UHALInputHintsWidget>());
	FHALInputHintsTestAccess::ConfigurePanels(PreMatch.Get()); FHALInputHintsTestAccess::ConfigurePanels(Match.Get());
	FHALInputHintsTestAccess::Key(F.Source.Get(),EKeys::Gamepad_FaceButton_Bottom);
	FHALInputHintsTestAccess::Bind(PreMatch.Get(),F.Source.Get()); FHALInputHintsTestAccess::Bind(Match.Get(),F.Source.Get());
	TestTrue(TEXT("PreMatch created after an input change immediately uses Xbox"),FHALInputHintsTestAccess::Shows(PreMatch.Get(),EHALInputHintDevice::Gamepad));
	TestTrue(TEXT("Match shares the state without sharing the Widget instance"),FHALInputHintsTestAccess::Shows(Match.Get(),EHALInputHintDevice::Gamepad));
	PreMatch->SetVisibility(ESlateVisibility::Collapsed);
	FHALInputHintsTestAccess::Key(F.Source.Get(),EKeys::Enter);
	PreMatch->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	TestTrue(TEXT("A hidden screen is already correct when shown again"),FHALInputHintsTestAccess::Shows(PreMatch.Get(),EHALInputHintDevice::KeyboardMouse));
	FHALInputHintsTestAccess::Bind(PreMatch.Get(),F.Source.Get());
	FHALInputHintsTestAccess::Destruct(PreMatch.Get());
	TestFalse(TEXT("Rebinding then destroying a screen leaves no duplicate subscriptions"),F.Source->OnInputDeviceChanged.IsBoundToObject(PreMatch.Get()));
	FHALInputHintsTestAccess::Key(F.Source.Get(),EKeys::Gamepad_FaceButton_Right);
	TestTrue(TEXT("Destroying PreMatch does not disconnect the Match screen"),FHALInputHintsTestAccess::Shows(Match.Get(),EHALInputHintDevice::Gamepad));
	FHALInputHintsTestAccess::Bind(PreMatch.Get(),F.Source.Get());
	TestTrue(TEXT("A reconstructed screen resynchronizes from the surviving local player"),FHALInputHintsTestAccess::Shows(PreMatch.Get(),EHALInputHintDevice::Gamepad));
	FHALInputHintsTestAccess::Destruct(PreMatch.Get()); FHALInputHintsTestAccess::Destruct(Match.Get());
	TestFalse(TEXT("All screen subscriptions are released"),F.Source->OnInputDeviceChanged.IsBound());
	return true;
}
#endif
