#include "HALVehicleTestPlayerController.h"

#include "HALVehicleTestGameMode.h"
#include "HALDynamicCameraComponent.h"
#include "HALPlayerState.h"
#include "HALAbilitySystemComponent.h"
#include "HALHealthAttributeSet.h"
#include "TestVehiclePawn.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "InputCoreTypes.h"
#include "UnrealClient.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** Private Slate implementation owned by the test controller; no separate UObject/widget asset. */
	class SHALVehicleTestPanel final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHALVehicleTestPanel) {} SLATE_ARGUMENT(AHALVehicleTestPlayerController*, Controller) SLATE_END_ARGS()
		void Construct(const FArguments& Args)
		{
			Controller=Args._Controller;
			if (const auto* PC=Controller.Get())
			{
				if (const auto* Mode=PC->GetWorld()->GetAuthGameMode<AHALVehicleTestGameMode>())
				{ for (const auto& Class:Mode->GetTestVehicleClasses()) { VehicleOptions.Add(MakeShared<TSubclassOf<ATestVehiclePawn>>(Class)); } }
			}
			if (!VehicleOptions.IsEmpty()) { SelectedVehicle=VehicleOptions[0]; }
			ChildSlot
			[
				SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
				+SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(12)
				[
					SNew(SBox).WidthOverride(360)
					[
						SAssignNew(PanelBorder,SBorder).Padding(12).BorderBackgroundColor(FLinearColor(0.025f,0.03f,0.04f,0.92f))
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
							[SNew(STextBlock).Text(FText::FromString(TEXT("SINGLE VEHICLE TEST")))]
							+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
							[
								SNew(SComboBox<TSharedPtr<TSubclassOf<ATestVehiclePawn>>>).IsFocusable(false)
								.OptionsSource(&VehicleOptions).InitiallySelectedItem(SelectedVehicle)
								.OnGenerateWidget_Lambda([](const auto Option){ return SNew(STextBlock).Text(VehicleLabel(Option)); })
								.OnSelectionChanged_Lambda([this](const auto Option,ESelectInfo::Type){ SelectedVehicle=Option; })
								[SNew(STextBlock).Text_Lambda([this]{ return VehicleLabel(SelectedVehicle); })]
							]
							+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
							[SNew(SButton).Tag(TEXT("HALVehicleTest.Apply")).IsFocusable(false).Text(FText::FromString(TEXT("Apply selected vehicle")))
								.OnClicked_Lambda([this]{ if (auto* PC=Controller.Get();PC && SelectedVehicle.IsValid()) { PC->ApplyTestVehicle(*SelectedVehicle); } return FReply::Handled(); })]
							+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
							[SNew(SButton).Tag(TEXT("HALVehicleTest.Camera")).IsFocusable(false)
								.Text_Lambda([this]{ const auto* PC=Controller.Get(); return FText::FromString(PC && PC->IsObservationCameraActive()?TEXT("Switch to vehicle camera [F2]"):TEXT("Switch to observation camera [F2]")); })
								.OnClicked_Lambda([this]{ if (auto* PC=Controller.Get()) { PC->ToggleTestCamera(); } return FReply::Handled(); })]
							+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
							[SNew(SButton).Tag(TEXT("HALVehicleTest.Focus")).IsFocusable(false).Text(FText::FromString(TEXT("Focus vehicle from the side")))
								.OnClicked_Lambda([this]{ if (auto* PC=Controller.Get()) { if (!PC->IsObservationCameraActive()) { PC->ToggleTestCamera(); } else { PC->FocusObservationCamera(); } } return FReply::Handled(); })]
							+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
							[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]{
								const auto* PC=Controller.Get();
								const auto* Mode=PC && PC->GetWorld()?PC->GetWorld()->GetAuthGameMode<AHALVehicleTestGameMode>():nullptr;
								const auto* PS=PC?PC->GetPlayerState<AHALPlayerState>():nullptr;
								const auto* ASC=PS?PS->GetHALAbilitySystemComponent():nullptr;
								const auto* HP=ASC?ASC->GetSet<UHALHealthAttributeSet>():nullptr;
								return FText::FromString(FString::Printf(TEXT("%s\nPawn: %s\nGAS: %s  HP: %.0f / %.0f"),Mode?*Mode->GetTestStatus():TEXT("Vehicle Test GameMode required"),*GetNameSafe(PC?PC->GetPawn():nullptr),ASC && PC && ASC->GetAvatarActor()==PC->GetPawn() && ASC->IsHealthInitialized()?TEXT("Ready"):TEXT("Waiting"),HP?HP->GetCurrentHP():0.f,HP?HP->GetMaxHP():0.f));
							})]
							+SVerticalBox::Slot().AutoHeight()
							[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("Vehicle controls stay active with either camera.\nHold RMB: mouse look. In observation view: RMB + WASD, Q/E, Shift to fly. Release RMB to drive.")))]
						]
					]
				]
			];
		}
		bool IsPointerOverPanel() const { return PanelBorder.IsValid() && PanelBorder->IsHovered(); }
	private:
		static FText VehicleLabel(const TSharedPtr<TSubclassOf<ATestVehiclePawn>>& Option)
		{
			FString Name=Option.IsValid() && Option->Get()?Option->Get()->GetName():TEXT("No configured vehicle");
			Name.RemoveFromEnd(TEXT("_C"));
			return FText::FromString(Name);
		}
		TWeakObjectPtr<AHALVehicleTestPlayerController> Controller;
		TSharedPtr<SBorder> PanelBorder;
		TArray<TSharedPtr<TSubclassOf<ATestVehiclePawn>>> VehicleOptions;
		TSharedPtr<TSubclassOf<ATestVehiclePawn>> SelectedVehicle;
	};
}

AHALVehicleTestPlayerController::AHALVehicleTestPlayerController()
{ bAutoManageActiveCameraTarget=false; }

void AHALVehicleTestPlayerController::BeginPlay()
{
	// Keep HAL inheritance for GAS lifecycle/debug commands, but skip its lobby, room delegates and readiness timer.
	APlayerController::BeginPlay();
	if (IsLocalController() && GetLocalPlayer())
	{
		if (UGameViewportClient* Viewport=GetLocalPlayer()->ViewportClient)
		{
			TestPanel=SNew(SHALVehicleTestPanel).Controller(this).Visibility(EVisibility::SelfHitTestInvisible);
			TestPanelViewport=Viewport;
			TestPanelPlayer=GetLocalPlayer();
			Viewport->AddViewportWidgetForPlayer(GetLocalPlayer(),TestPanel.ToSharedRef(),200);
		}
		ApplyTestInputMode();
	}
	RefreshTestView();
}

void AHALVehicleTestPlayerController::SetupInputComponent()
{
	// The local test panel owns UI input; do not register HAL's room-menu bindings.
	APlayerController::SetupInputComponent();
	InputComponent->BindKey(EKeys::F2,IE_Pressed,this,&ThisClass::ToggleTestCamera);
	InputComponent->BindKey(EKeys::RightMouseButton,IE_Pressed,this,&ThisClass::BeginCameraNavigation).bConsumeInput=false;
	InputComponent->BindKey(EKeys::RightMouseButton,IE_Released,this,&ThisClass::FinishCameraNavigation).bConsumeInput=false;
}

bool AHALVehicleTestPlayerController::IsPointerOnPanel() const
{ return TestPanel.IsValid() && StaticCastSharedPtr<SHALVehicleTestPanel>(TestPanel)->IsPointerOverPanel(); }

bool AHALVehicleTestPlayerController::BypassesRoomReadiness(const APawn* Pawn)
{
	const auto* PC=Pawn?Cast<AHALVehicleTestPlayerController>(Pawn->GetController()):nullptr;
	return PC && PC->IsLocalController() && AHALVehicleTestGameMode::IsTestWorld(PC->GetWorld());
}

bool AHALVehicleTestPlayerController::AllowsVehicleCameraInput(const APlayerController* Controller)
{
	const auto* PC=Cast<AHALVehicleTestPlayerController>(Controller);
	return Controller && (!Controller->bShowMouseCursor || (PC && !PC->bObservationView));
}

bool AHALVehicleTestPlayerController::AllowsVehicleCameraMouseInput(const AController* Controller)
{
	const auto* PC=Cast<AHALVehicleTestPlayerController>(Controller);
	return !PC || (!PC->bObservationView && PC->bCameraNavigation);
}

void AHALVehicleTestPlayerController::TickActor(float Dt,ELevelTick TickType,FActorTickFunction& ThisTickFunction)
{
	// Engine ticking still dispatches PlayerTick/input; the match-spectator update is intentionally excluded.
	APlayerController::TickActor(Dt,TickType,ThisTickFunction);
}

void AHALVehicleTestPlayerController::PostSeamlessTravel()
{
	// An explicit developer travel must not restart HAL's room readiness polling.
	APlayerController::PostSeamlessTravel();
}

void AHALVehicleTestPlayerController::ApplyTestInputMode()
{
	if (!GetLocalPlayer()) { return; }
	bShowMouseCursor=!bCameraNavigation;
	if (bCameraNavigation) { SetInputMode(FInputModeGameOnly()); }
	else
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
}

void AHALVehicleTestPlayerController::BeginCameraNavigation()
{
	if (IsPointerOnPanel()) { return; }
	bCameraNavigation=true;
	if (bObservationView)
	{ if (auto* Vehicle=Cast<ATestVehiclePawn>(GetPawn())) { Vehicle->SetMenuInputActive(true); } }
	ApplyTestInputMode();
}

void AHALVehicleTestPlayerController::FinishCameraNavigation()
{
	if (!bCameraNavigation) { return; }
	bCameraNavigation=false;
	if (bObservationView)
	{ if (auto* Vehicle=Cast<ATestVehiclePawn>(GetPawn())) { Vehicle->SetMenuInputActive(false); } }
	ApplyTestInputMode();
}

void AHALVehicleTestPlayerController::ToggleTestCamera()
{
	FinishCameraNavigation();
	if (!bObservationView)
	{
		if (!GetPawn()) { return; }
		if (!ObservationCamera)
		{
			FActorSpawnParameters Params; Params.ObjectFlags|=RF_Transient;
			ObservationCamera=GetWorld()->SpawnActor<ACameraActor>(Params);
			if (!ObservationCamera) { return; }
			ObservationCamera->SetReplicates(false);
		}
		bObservationView=true;
		FocusObservationCamera();
	}
	else { bObservationView=false; }
	RefreshTestView();
	ApplyTestInputMode();
}

void AHALVehicleTestPlayerController::FocusObservationCamera()
{
	const APawn* ControlledVehicle=GetPawn();
	if (!ControlledVehicle || !ObservationCamera) { return; }
	const auto* Body=Cast<UPrimitiveComponent>(ControlledVehicle->GetRootComponent());
	const FVector Focus=Body?Body->Bounds.Origin:ControlledVehicle->GetActorLocation();
	FVector Right=FVector::VectorPlaneProject(ControlledVehicle->GetActorRightVector(),FVector::UpVector).GetSafeNormal();
	if (Right.IsNearlyZero()) { Right=FVector::RightVector; }
	const FVector Location=Focus+Right*ObservationDistance+FVector::UpVector*FMath::Max(60.f,ObservationDistance*0.18f);
	ObservationCamera->SetActorLocationAndRotation(Location,(Focus-Location).Rotation());
	ObservationCamera->GetCameraComponent()->SetFieldOfView(60);
}

void AHALVehicleTestPlayerController::OnPossess(APawn* NewPawn)
{ Super::OnPossess(NewPawn); RefreshTestView(); }

void AHALVehicleTestPlayerController::RefreshTestView()
{
	if (APawn* ControlledVehicle=GetPawn())
	{
		if (auto* Camera=ControlledVehicle->FindComponentByClass<UHALDynamicCameraComponent>()) { Camera->ClearObservationInput(); }
		if (bObservationView && ObservationCamera) { FocusObservationCamera(); SetViewTarget(ObservationCamera); }
		else { SetViewTarget(ControlledVehicle); }
	}
}

void AHALVehicleTestPlayerController::ApplyTestVehicle(TSubclassOf<ATestVehiclePawn> Class)
{
	if (auto* Mode=GetWorld()->GetAuthGameMode<AHALVehicleTestGameMode>()) { Mode->ChangeTestVehicle(this,Class); }
	ApplyTestInputMode();
}

void AHALVehicleTestPlayerController::PlayerTick(float Dt)
{
	Super::PlayerTick(Dt);
	if (!bCameraNavigation || !IsInputKeyDown(EKeys::RightMouseButton)
		|| !GetWorld()->GetGameViewport() || !GetWorld()->GetGameViewport()->Viewport
		|| !GetWorld()->GetGameViewport()->Viewport->HasFocus()) { FinishCameraNavigation(); return; }
	if (!bObservationView || !ObservationCamera) { return; }
	float MouseX=0,MouseY=0; GetInputMouseDelta(MouseX,MouseY);
	FRotator Rotation=ObservationCamera->GetActorRotation();
	Rotation.Yaw+=MouseX*ObservationLookSensitivity;
	Rotation.Pitch=FMath::Clamp(Rotation.Pitch-MouseY*ObservationLookSensitivity,-89.f,89.f);
	const auto Axis=[this](FKey Positive,FKey Negative) { return float(IsInputKeyDown(Positive))-float(IsInputKeyDown(Negative)); };
	const FVector Direction=Rotation.Vector()*Axis(EKeys::W,EKeys::S)
		+FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y)*Axis(EKeys::D,EKeys::A)+FVector::UpVector*Axis(EKeys::E,EKeys::Q);
	const float Speed=ObservationMoveSpeed*(IsInputKeyDown(EKeys::LeftShift)?3.f:1.f);
	ObservationCamera->SetActorLocationAndRotation(ObservationCamera->GetActorLocation()+Direction.GetClampedToMaxSize(1)*Speed*FMath::Clamp(Dt,0.f,0.1f),Rotation);
}

void AHALVehicleTestPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (TestPanel.IsValid())
	{
		if (auto* Viewport=TestPanelViewport.Get()) { Viewport->RemoveViewportWidgetForPlayer(TestPanelPlayer.Get(),TestPanel.ToSharedRef()); }
		TestPanel.Reset(); TestPanelViewport.Reset(); TestPanelPlayer.Reset();
	}
	if (ObservationCamera) { ObservationCamera->Destroy(); ObservationCamera=nullptr; }
	Super::EndPlay(Reason);
}
