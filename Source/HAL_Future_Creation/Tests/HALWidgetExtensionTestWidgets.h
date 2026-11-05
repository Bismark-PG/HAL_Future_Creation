#pragma once
#include "../HALArcadeEntryWidget.h"
#include "../HALPracticeRoomWidget.h"
#include "HALWidgetExtensionTestWidgets.generated.h"

class UButton;
class UVerticalBox;
class UTextBlock;

/** In-memory UMG fixtures. No Blueprint assets are created, edited or saved by tests. */
UCLASS(NotBlueprintable,Transient,HideDropdown)
class UHALContractEntryWidget : public UHALArcadeEntryWidget
{
	GENERATED_BODY()
public:
	virtual bool UsesDesignerLayout() const override { return true; }
	virtual void NativeOnInitialized() override;
	virtual void ReceiveEntryView_Implementation(const FHALEntryView& View) override;
	UPROPERTY(Transient) TObjectPtr<UButton> HostButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StateText;
	int32 Updates=0;
private:
	UFUNCTION() void ClickHost() { HostRoom(); }
};
UCLASS(NotBlueprintable,Transient,HideDropdown)
class UHALContractRoomWidget : public UHALPracticeRoomWidget
{
	GENERATED_BODY()
public:
	virtual bool UsesDesignerLayout() const override { return true; }
	virtual void NativeOnInitialized() override;
	virtual void ReceiveRoomView_Implementation(const FHALRoomView& View) override;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> Panel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StateText;
	UPROPERTY(Transient) TObjectPtr<UButton> NextButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ConfirmButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CancelButton;
	UPROPERTY(Transient) TObjectPtr<UButton> StartButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ResumeButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ReturnButton;
	UPROPERTY(Transient) TObjectPtr<UButton> LeaveButton;
	int32 Updates=0;
	bool LastWasResults=false;
	FHALRoundMatchData LastMatch;
private:
	UFUNCTION() void ClickNext() { if (GetRoomView().bCanCycleSpectator) { CycleSpectator(1); } else { CycleVehicle(1); } }
	UFUNCTION() void ClickConfirm() { ConfirmSelection(); }
	UFUNCTION() void ClickCancel() { CancelSelection(); }
	UFUNCTION() void ClickStart() { StartRoomMatch(); }
	UFUNCTION() void ClickResume() { ResumeDriving(); }
	UFUNCTION() void ClickReturn() { ReturnToRoom(); }
	UFUNCTION() void ClickLeave() { LeaveCurrentRoom(); }
};
// Sibling subclasses ensure Pause does not have to derive from the configured selection screen.
UCLASS(NotBlueprintable,Transient,HideDropdown)
class UHALContractSelectionWidget : public UHALContractRoomWidget { GENERATED_BODY() };
UCLASS(NotBlueprintable,Transient,HideDropdown)
class UHALContractPauseWidget : public UHALContractRoomWidget { GENERATED_BODY() };
