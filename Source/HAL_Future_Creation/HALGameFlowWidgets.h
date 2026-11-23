#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HALFrontEndViewTypes.h"
#include "HALGameFlowWidgets.generated.h"

class UHALPracticeRoomWidget;
class UHALVehicleCatalog;
class UWidget;

UENUM()
enum class EHALGameFlowPanel : uint8 { Selection, Practice, LocalMenu, Loading, Countdown, Spectator, Results, Transition };
UENUM()
enum class EHALGameFlowUIAction : uint8 { Previous, Next, Confirm, CancelSelection, Start, Resume, Leave, Return, PreviousSpectator, NextSpectator, Back };

/** Designer children share display data and send semantic requests to their registered root. */
UCLASS(Blueprintable)
class HAL_FUTURE_CREATION_API UHALGameFlowScreenWidget : public UUserWidget, public IHALRoomScreenInterface
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category="Game Flow") EHALGameFlowPanel Panel=EHALGameFlowPanel::Transition;
	UPROPERTY(EditDefaultsOnly, Category="Game Flow") TMap<FName,EHALGameFlowUIAction> Actions;
	UPROPERTY(EditDefaultsOnly, Category="Game Flow|Preview") TSubclassOf<AActor> PreviewClass;
	/** Receives match-phase views as one room child; this widget selects its own Switcher pages. */
	UPROPERTY(EditDefaultsOnly, Category="Game Flow|Match Flow") bool bMatchFlowContainer=false;
	/** Select an existing direct Switcher child and forward the cached Room view. Null clears the page. */
	UFUNCTION(BlueprintCallable, Category="Game Flow|Match Flow") bool ShowFlowScreen(UHALGameFlowScreenWidget* Screen);
	void SetRoomRoot(UHALPracticeRoomWidget* Root);
	void PrepareView(const FHALRoomView& View);
	void FinishView();
	void UpdateActive(bool bActive);
	virtual void ApplyView_Implementation(const FHALRoomView& View) override;
	virtual void SetScreenActive_Implementation(bool bActive) override;
	virtual UWidget* GetDefaultFocusTarget_Implementation() const override;
	UFUNCTION(BlueprintPure, Category="Game Flow") FHALRoomView GetScreenView() const { return LatestView; }
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	const FHALRoomView& GetCachedRoomView() const { return LatestView; }
	bool UsesRoomView() const { return bUsesRoomView; }
	bool IsRoomScreenActive() const { return bScreenActive; }
	/** Specialized HUDs share the view/lifecycle contract without the generic panel presentation. */
	virtual bool UsesPanelPresentation() const { return true; }
	virtual void OnRoomViewApplied(const FHALRoomView& View) {}
	virtual void OnScreenActiveChanged(bool bActive) {}
private:
	void UpdateFlowScreenActive(bool bActive);
	void BindActions();
	void Request(EHALGameFlowUIAction Action);
	UPROPERTY(Transient) TObjectPtr<UHALPracticeRoomWidget> RoomRoot;
	UPROPERTY(Transient) FHALRoomView LatestView;
	UPROPERTY(Transient) TObjectPtr<AActor> OwnedPreview;
	UPROPERTY(Transient) TObjectPtr<UHALGameFlowScreenWidget> ActiveFlowScreen;
	bool bUsesRoomView=false, bScreenActive=false;
	UFUNCTION() void Previous() { Request(EHALGameFlowUIAction::Previous); }
	UFUNCTION() void Next() { Request(EHALGameFlowUIAction::Next); }
	UFUNCTION() void Confirm() { Request(EHALGameFlowUIAction::Confirm); }
	UFUNCTION() void CancelSelection() { Request(EHALGameFlowUIAction::CancelSelection); }
	UFUNCTION() void Start() { Request(EHALGameFlowUIAction::Start); }
	UFUNCTION() void Resume() { Request(EHALGameFlowUIAction::Resume); }
	UFUNCTION() void Leave() { Request(EHALGameFlowUIAction::Leave); }
	UFUNCTION() void Return() { Request(EHALGameFlowUIAction::Return); }
	UFUNCTION() void PreviousSpectator() { Request(EHALGameFlowUIAction::PreviousSpectator); }
	UFUNCTION() void NextSpectator() { Request(EHALGameFlowUIAction::NextSpectator); }
	UFUNCTION() void Back() { Request(EHALGameFlowUIAction::Back); }
};

UCLASS(Blueprintable)
class HAL_FUTURE_CREATION_API UHALGameFlowSeatWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void ApplySeat(const FHALRoomSeatView& Seat,const TArray<FHALVehicleDisplayView>& Vehicles);
};

UCLASS(Blueprintable)
class HAL_FUTURE_CREATION_API UHALGameFlowParticipantsWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Game Flow") void ApplySeats(const TArray<FHALRoomSeatView>& Seats);
	void ApplyRoomView(const FHALRoomView& View);
private:
	UPROPERTY(Transient) TArray<FHALVehicleDisplayView> Vehicles;
};

/** One match participant, driven by immutable identity and the room's HP snapshot. */
UCLASS(Blueprintable)
class HAL_FUTURE_CREATION_API UHALGameFlowPlayerStatusRow : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category="Game Flow|Bindings") FName NumberTextName;
	UPROPERTY(EditDefaultsOnly, Category="Game Flow|Bindings") FName HealthTextName;
	UPROPERTY(EditDefaultsOnly, Category="Game Flow|Bindings") FName HealthBarName;
	UPROPERTY(EditDefaultsOnly, Category="Game Flow|Bindings") FName IconName;
	UPROPERTY(EditDefaultsOnly, Category="Game Flow|Bindings") FName BackgroundName;
	void ApplyParticipant(const FHALRoundParticipant& Participant,const FHALRoomView& View,const UHALVehicleCatalog* Catalog);
private:
	FName IconVehicle;
};
