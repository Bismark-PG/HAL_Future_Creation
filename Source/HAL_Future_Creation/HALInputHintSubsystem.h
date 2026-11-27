#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "HALInputHintSubsystem.generated.h"

class FSlateApplication;
class IInputProcessor;
struct FInputEvent;
struct FPointerEvent;

/** Presentation only: all supported gamepads currently use the authored Xbox glyphs. */
UENUM(BlueprintType)
enum class EHALInputHintDevice : uint8 { KeyboardMouse, Gamepad };

DECLARE_MULTICAST_DELEGATE_OneParam(FHALInputHintDeviceChanged, EHALInputHintDevice);

/** Last meaningful input for one local player. Survives travel; never consumes or submits gameplay input. */
UCLASS(Config=Game)
class HAL_FUTURE_CREATION_API UHALInputHintSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	UFUNCTION(BlueprintPure, Category="UI|Input Hints") EHALInputHintDevice GetCurrentInputDevice() const { return CurrentDevice; }
	FHALInputHintDeviceChanged OnInputDeviceChanged;

private:
	friend class FHALInputHintProcessor;
	friend struct FHALInputHintsTestAccess;
	/** Normalized stick/trigger magnitude required to select gamepad hints. */
	UPROPERTY(Config, EditDefaultsOnly, Category="Input Hints", meta=(ClampMin="0.01", ClampMax="1"))
	float AnalogActivationThreshold=.25f;
	/** An axis must return below this magnitude before another movement in the same direction counts. */
	UPROPERTY(Config, EditDefaultsOnly, Category="Input Hints", meta=(ClampMin="0", ClampMax="1"))
	float AnalogReleaseThreshold=.15f;
	/** Minimum cursor movement per event, in Slate screen pixels; clicks and wheel input count immediately. */
	UPROPERTY(Config, EditDefaultsOnly, Category="Input Hints", meta=(ClampMin="0", ClampMax="100", DisplayName="Minimum Mouse Movement (pixels)"))
	float MinimumMouseMovement=2.f;
	bool IsRelevantInput(FSlateApplication& SlateApp, const FInputEvent& Event, const FPointerEvent* Pointer=nullptr) const;
	void RecordKey(const FKey& Key, bool bRepeat);
	void RecordAnalog(const FKey& Key, float Value);
	void RecordMouseMovement(const FVector2D& Delta);
	void SetCurrentDevice(EHALInputHintDevice Device);
	EHALInputHintDevice CurrentDevice=EHALInputHintDevice::KeyboardMouse;
	TMap<FKey,int8> ActiveAnalogDirections;
	TSharedPtr<IInputProcessor> InputProcessor;
};
