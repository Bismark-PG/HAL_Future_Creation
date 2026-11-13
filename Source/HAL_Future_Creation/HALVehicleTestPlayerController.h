#pragma once

#include "CoreMinimal.h"
#include "HALPlayerController.h"
#include "HALVehicleTestPlayerController.generated.h"

class ACameraActor;
class ATestVehiclePawn;
class UGameViewportClient;
class ULocalPlayer;
class SWidget;

/** Local test UI and camera input. Vehicle ownership stays on the car in either view. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALVehicleTestPlayerController : public AHALPlayerController
{
	GENERATED_BODY()
public:
	AHALVehicleTestPlayerController();
	/** Shared consumers delegate only these test exceptions here; ordinary controllers retain their rules. */
	static bool BypassesRoomReadiness(const APawn* Pawn);
	static bool AllowsVehicleCameraInput(const APlayerController* Controller);
	static bool AllowsVehicleCameraMouseInput(const AController* Controller);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void SetupInputComponent() override;
	virtual void TickActor(float DeltaSeconds,ELevelTick TickType,FActorTickFunction& ThisTickFunction) override;
	virtual void PostSeamlessTravel() override;
	virtual void PlayerTick(float DeltaSeconds) override;
	void ToggleTestCamera();
	void FocusObservationCamera();
	void ApplyTestVehicle(TSubclassOf<ATestVehiclePawn> Class);
	void FinishCameraNavigation();
	void RefreshTestView();
	bool IsObservationCameraActive() const { return bObservationView; }
	ACameraActor* GetObservationCamera() const { return ObservationCamera; }

protected:
	virtual void OnPossess(APawn* NewPawn) override;
	UPROPERTY(EditDefaultsOnly,Category="Vehicle Test|Camera",meta=(ClampMin="0.01",ClampMax="10")) float ObservationPanSensitivity=1.f;
	UPROPERTY(EditDefaultsOnly,Category="Vehicle Test|Camera",meta=(Units="cm",ClampMin="100",ClampMax="10000")) float ObservationDistance=650;
	UPROPERTY(EditDefaultsOnly,Category="Vehicle Test|Camera",meta=(ClampMin="0.01",ClampMax="5")) float ObservationLookSensitivity=0.15f;
	UPROPERTY(EditDefaultsOnly,Category="Vehicle Test|Camera",meta=(ClampMin="0.0001",ClampMax="0.1")) float ObservationZoomSensitivity=0.005f;

private:
	friend struct FHALVehicleTestAccess;
	enum class EObservationDrag : uint8 { None, Orbit, Zoom, Pan };
	void BeginCameraNavigation();
	void BeginObservationOrbit();
	void BeginObservationZoom();
	void BeginObservationPan();
	void BeginObservationDrag(EObservationDrag Drag);
	void ApplyObservationDrag(float MouseX,float MouseY);
	void ZoomObservationCamera(float Amount);
	void OnObservationWheel(float Steps);
	bool IsAltHeld() const;
	bool IsCameraViewportFocused() const;
	void ApplyTestInputMode();
	bool IsPointerOnPanel() const;
	TSharedPtr<SWidget> TestPanel;
	TWeakObjectPtr<UGameViewportClient> TestPanelViewport;
	TWeakObjectPtr<ULocalPlayer> TestPanelPlayer;
	UPROPERTY(Transient) TObjectPtr<ACameraActor> ObservationCamera;
	bool bObservationView=false;
	bool bCameraNavigation=false;
	EObservationDrag ObservationDrag=EObservationDrag::None;
	FVector ObservationPivot=FVector::ZeroVector;
};
