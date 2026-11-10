#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../HALVehicleTestGameMode.h"
#include "../HALVehicleTestPlayerController.h"
#include "../HALMatchGameState.h"
#include "../HALRoomComponent.h"
#include "../HALPlayerState.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALHealthAttributeSet.h"
#include "../HALImpactImmunityEffects.h"
#include "../HALGameplayTags.h"
#include "../HALGameFlowPermissions.h"
#include "../HALPlayerStart.h"
#include "../TestVehiclePawn.h"
#include "../PassiveTestVehicle.h"
#include "HALCameraTestSupport.h"
#include "Camera/CameraActor.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"

struct FHALVehicleTestAccess
{
	static void Configure(AHALVehicleTestGameMode* Mode,UClass* Initial,UClass* Other)
	{ Mode->DefaultPawnClass=Initial; Mode->TestVehicleClasses={Initial,Other}; }
	static void BeginCameraNavigation(AHALVehicleTestPlayerController* PC) { PC->BeginCameraNavigation(); }
	static FVector Pivot(const AHALVehicleTestPlayerController* PC) { return PC->ObservationPivot; }
	static void Orbit(AHALVehicleTestPlayerController* PC,float X,float Y)
	{ PC->ObservationDrag=AHALVehicleTestPlayerController::EObservationDrag::Orbit; PC->ApplyObservationDrag(X,Y); PC->ObservationDrag=AHALVehicleTestPlayerController::EObservationDrag::None; }
	static void Pan(AHALVehicleTestPlayerController* PC,float X,float Y)
	{ PC->ObservationDrag=AHALVehicleTestPlayerController::EObservationDrag::Pan; PC->ApplyObservationDrag(X,Y); PC->ObservationDrag=AHALVehicleTestPlayerController::EObservationDrag::None; }
	static void ZoomDrag(AHALVehicleTestPlayerController* PC,float X,float Y)
	{ PC->ObservationDrag=AHALVehicleTestPlayerController::EObservationDrag::Zoom; PC->ApplyObservationDrag(X,Y); PC->ObservationDrag=AHALVehicleTestPlayerController::EObservationDrag::None; }
	static void Zoom(AHALVehicleTestPlayerController* PC,float Amount) { PC->ZoomObservationCamera(Amount); }
	static bool HasAltBinding(const AHALVehicleTestPlayerController* PC,FKey Key)
	{ return PC->InputComponent && PC->InputComponent->KeyBindings.ContainsByPredicate([Key](const FInputKeyBinding& B){ return B.Chord.Key==Key && B.Chord.bAlt && B.KeyEvent==IE_Pressed; }); }
	static bool HasWheelBinding(const AHALVehicleTestPlayerController* PC)
	{ return PC->InputComponent && PC->InputComponent->AxisKeyBindings.ContainsByPredicate([](const FInputAxisKeyBinding& B){ return B.AxisKey==EKeys::MouseWheelAxis; }); }
};

namespace
{
	struct FVehicleTestWorld
	{
		UWorld* World;
		AHALVehicleTestGameMode* Mode;
		AHALVehicleTestPlayerController* PC;
		UClass* Car02=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_02.BP_VehiclePawn_Car_02_C"));
		UClass* Other=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
		FVehicleTestWorld()
		{
			const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			FURL URL; URL.AddOption(TEXT("game=/Script/HAL_Future_Creation.HALVehicleTestGameMode"));
			World->SetGameMode(URL); World->InitializeActorsForPlay(URL);
			Mode=World->GetAuthGameMode<AHALVehicleTestGameMode>();
			FHALVehicleTestAccess::Configure(Mode,Car02,Other);
			World->SpawnActor<AHALPlayerStart>(FVector(0,0,1000),FRotator::ZeroRotator);
			PC=World->SpawnActor<AHALVehicleTestPlayerController>(); PC->SetAsLocalPlayerController();
			Mode->HandleStartingNewPlayer(PC);
		}
		AHALPlayerState* State() const { return PC->GetPlayerState<AHALPlayerState>(); }
		UHALAbilitySystemComponent* ASC() const { return State()->GetHALAbilitySystemComponent(); }
		ATestVehiclePawn* Vehicle() const { return Cast<ATestVehiclePawn>(PC->GetPawn()); }
		int32 VehicleCount() const { int32 Count=0; for(TActorIterator<ATestVehiclePawn> It(World);It;++It){if(!It->IsActorBeingDestroyed()){++Count;}} return Count; }
		~FVehicleTestWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALVehicleTestInitial,"HAL.FutureCreation.VehicleTest.StandalonePlayingGASAndAllInputPermissions",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALVehicleTestInitial::RunTest(const FString&)
{
	FVehicleTestWorld F;
	if (!TestNotNull(TEXT("Initial Car_02 is spawned"),F.Vehicle())) { return false; }
	TestEqual(TEXT("Initial exact requested vehicle class"),F.Vehicle()->GetClass(),F.Car02);
	TestEqual(TEXT("One vehicle"),F.VehicleCount(),1);
	TestEqual(TEXT("Offline net mode"),F.World->GetNetMode(),NM_Standalone);
	const auto* State=F.World->GetGameState<AHALMatchGameState>();
	TestEqual(TEXT("Playing without waiting or host start"),State->GetMatchPhase(),EHALMatchPhase::Playing);
	TestFalse(TEXT("No room initialized"),State->GetRoom()->IsEnabled());
	TestFalse(TEXT("No round or match deadline"),State->GetRoom()->GetData().Match.bActive);
	F.PC->DispatchBeginPlay();
	F.PC->PostSeamlessTravel();
	TestFalse(TEXT("Test startup and travel do not subscribe to room UI updates"),State->GetRoom()->OnRoomChanged.IsBound());
	TestTrue(TEXT("Room-readiness exception belongs to the dedicated local test"),AHALVehicleTestPlayerController::BypassesRoomReadiness(F.Vehicle()));
	TestTrue(TEXT("GAS owner, avatar and seeded attributes initialized"),F.ASC()->GetOwnerActor()==F.State()
		&& F.ASC()->GetAvatarActor()==F.Vehicle() && F.ASC()->IsHealthInitialized() && F.ASC()->IsCombatInitialized() && F.ASC()->IsImpactInitialized());
	TestTrue(TEXT("Configured vehicle abilities granted"),F.State()->AreVehicleAbilitiesReady(F.Vehicle()));
	TestTrue(TEXT("No room membership is required for local input identity"),F.Vehicle()->IsLocalInputAvatarLinked());
	FHALCameraTestAccess::MovementReady(F.Vehicle(),true);
	TestTrue(TEXT("Physics-ready local input starts without client acknowledgement"),F.Vehicle()->IsLocalInputInitialized());
	const auto P=HALGameFlow::Permissions(F.World,F.Vehicle());
	TestTrue(TEXT("Driving/ball/abilities/recovery/observation/interactions all permitted offline"),P.bDrive && P.bBallControl && P.bAbilities && P.bRecovery && P.bObserve && P.bWorldInteractions);
	F.PC->bShowMouseCursor=true;
	TestTrue(TEXT("Visible panel cursor does not block camera input"),AHALVehicleTestPlayerController::AllowsVehicleCameraInput(F.PC));
	auto* Normal=F.World->SpawnActor<AHALPlayerController>();
	Normal->SetAsLocalPlayerController(); Normal->DispatchBeginPlay();
	TestTrue(TEXT("Ordinary HAL controller still subscribes to room UI updates"),State->GetRoom()->OnRoomChanged.IsBound());
	Normal->bShowMouseCursor=true;
	TestFalse(TEXT("Ordinary controller still blocks camera input with a visible cursor"),AHALVehicleTestPlayerController::AllowsVehicleCameraInput(Normal));
	Normal->bShowMouseCursor=false;
	TestTrue(TEXT("Ordinary controller retains camera input with a hidden cursor"),AHALVehicleTestPlayerController::AllowsVehicleCameraInput(Normal));
	TestTrue(TEXT("Ordinary controller retains mouse camera input"),AHALVehicleTestPlayerController::AllowsVehicleCameraMouseInput(Normal));
	auto* NormalPawn=F.World->SpawnActor<APawn>(); Normal->Possess(NormalPawn);
	TestFalse(TEXT("Ordinary controller keeps room readiness even inside a test world"),AHALVehicleTestPlayerController::BypassesRoomReadiness(NormalPawn));
	Normal->Destroy(); NormalPawn->Destroy();
	TestFalse(TEXT("Ordinary teardown leaves no test room-UI subscription"),State->GetRoom()->OnRoomChanged.IsBound());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALVehicleTestSwap,"HAL.FutureCreation.VehicleTest.RuntimeSwapRetainsPlayerAndCleansOldAvatar",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALVehicleTestSwap::RunTest(const FString&)
{
	FVehicleTestWorld F;
	if (!TestNotNull(TEXT("Initial car"),F.Vehicle())) { return false; }
	auto* Original=F.Vehicle(); auto* Player=F.State(); auto* ASC=F.ASC();
	F.PC->ToggleTestCamera(); auto* Camera=F.PC->GetObservationCamera();
	ASC->ApplyGameplayEffectToSelf(GetDefault<UHALImpactImmunityCooldownEffect>(),1,ASC->MakeEffectContext());
	TestTrue(TEXT("Different configured model applies through real lifecycle"),F.Mode->ChangeTestVehicle(F.PC,F.Other));
	TestTrue(TEXT("PlayerState/ASC/controller retained"),F.State()==Player && F.ASC()==ASC && F.PC->GetPawn()==F.Vehicle());
	TestTrue(TEXT("Old Pawn retired and destroyed"),Original->IsActorBeingDestroyed() && !Original->IsConfigurationValid());
	TestTrue(TEXT("ASC avatar and grants now belong to new car"),ASC->GetAvatarActor()==F.Vehicle() && Player->AreVehicleAbilitiesReady(F.Vehicle()));
	TestEqual(TEXT("Only one vehicle after replacement"),F.VehicleCount(),1);
	TestTrue(TEXT("Observation camera persists and focuses the replacement"),F.PC->IsObservationCameraActive()
		&& F.PC->GetObservationCamera()==Camera && F.PC->GetViewTarget()==Camera);
	TestEqual(TEXT("Old Avatar cooldown cleared by vehicle lifecycle"),ASC->GetEffectTimeRemaining(HALGameplayTags::Cooldown_ImpactImmunity),0.f);
	TestTrue(TEXT("Switch back to Car_02"),F.Mode->ChangeTestVehicle(F.PC,F.Car02));
	TestEqual(TEXT("Still one car after repeated swaps"),F.VehicleCount(),1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALVehicleTestCamera,"HAL.FutureCreation.VehicleTest.SideCameraPreservesPossessionAndGAS",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALVehicleTestCamera::RunTest(const FString&)
{
	FVehicleTestWorld F;
	if (!TestNotNull(TEXT("Initial car"),F.Vehicle())) { return false; }
	auto* Pawn=F.Vehicle(); auto* ASC=F.ASC(); const auto Serial=F.State()->GetVehicleLifecycle().ChangeSerial;
	F.PC->SetupInputComponent();
	TestTrue(TEXT("Alt mouse buttons and unmodified wheel are registered"),FHALVehicleTestAccess::HasAltBinding(F.PC,EKeys::LeftMouseButton)
		&& FHALVehicleTestAccess::HasAltBinding(F.PC,EKeys::MiddleMouseButton) && FHALVehicleTestAccess::HasAltBinding(F.PC,EKeys::RightMouseButton)
		&& FHALVehicleTestAccess::HasWheelBinding(F.PC));
	TestFalse(TEXT("Panel mouse movement does not rotate the vehicle camera"),AHALVehicleTestPlayerController::AllowsVehicleCameraMouseInput(F.PC));
	FHALVehicleTestAccess::BeginCameraNavigation(F.PC);
	TestTrue(TEXT("RMB navigation enables vehicle mouse look"),AHALVehicleTestPlayerController::AllowsVehicleCameraMouseInput(F.PC));
	F.PC->FinishCameraNavigation();
	TestFalse(TEXT("Releasing RMB restores panel mouse handling"),AHALVehicleTestPlayerController::AllowsVehicleCameraMouseInput(F.PC));
	ASC->ApplyGameplayEffectToSelf(GetDefault<UHALImpactImmunityCooldownEffect>(),1,ASC->MakeEffectContext());
	F.PC->ToggleTestCamera(); auto* Camera=F.PC->GetObservationCamera();
	if (!TestNotNull(TEXT("Independent observation camera"),Camera)) { return false; }
	F.PC->bShowMouseCursor=true;
	TestFalse(TEXT("Observation view does not feed visible-cursor input to the vehicle camera"),AHALVehicleTestPlayerController::AllowsVehicleCameraInput(F.PC));
	TestFalse(TEXT("Observation view does not feed mouse input to the vehicle camera"),AHALVehicleTestPlayerController::AllowsVehicleCameraMouseInput(F.PC));
	TestTrue(TEXT("Side view rather than top view"),FMath::Abs(Camera->GetActorRotation().Pitch)<30.f);
	TestTrue(TEXT("Camera focus is beside the car"),FMath::Abs(FVector::DotProduct(Camera->GetActorLocation()-Pawn->GetActorLocation(),Pawn->GetActorRightVector()))>500.f);
	const FVector FirstPivot=FHALVehicleTestAccess::Pivot(F.PC),FirstLocation=Camera->GetActorLocation();
	const double FirstDistance=FVector::Distance(FirstLocation,FirstPivot);
	FHALVehicleTestAccess::Orbit(F.PC,120,30);
	TestTrue(TEXT("Orbit moves the camera around its focus with a constant radius"),!Camera->GetActorLocation().Equals(FirstLocation,0.01)
		&& FMath::IsNearlyEqual(FVector::Distance(Camera->GetActorLocation(),FirstPivot),FirstDistance,0.01)
		&& FVector::DotProduct(Camera->GetActorForwardVector(),(FirstPivot-Camera->GetActorLocation()).GetSafeNormal())>0.999);
	const FVector BeforePan=Camera->GetActorLocation(); const FRotator BeforePanRotation=Camera->GetActorRotation();
	FHALVehicleTestAccess::Pan(F.PC,25,-10);
	TestTrue(TEXT("Pan translates the focus and camera together without changing angle"),!Camera->GetActorLocation().Equals(BeforePan,0.01)
		&& (Camera->GetActorLocation()-BeforePan).Equals(FHALVehicleTestAccess::Pivot(F.PC)-FirstPivot,0.01)
		&& Camera->GetActorRotation().Equals(BeforePanRotation,0.01));
	const FVector PanPivot=FHALVehicleTestAccess::Pivot(F.PC);
	FHALVehicleTestAccess::ZoomDrag(F.PC,40,20);
	TestTrue(TEXT("Zoom drag reduces the focus distance while retaining the focus"),FVector::Distance(Camera->GetActorLocation(),PanPivot)<FirstDistance
		&& FHALVehicleTestAccess::Pivot(F.PC).Equals(PanPivot));
	FHALVehicleTestAccess::ZoomDrag(F.PC,-40,-20);
	TestTrue(TEXT("Opposite zoom drag restores distance"),FMath::IsNearlyEqual(FVector::Distance(Camera->GetActorLocation(),PanPivot),FirstDistance,0.01));
	FHALVehicleTestAccess::Zoom(F.PC,1000);
	TestTrue(TEXT("Large zoom keeps the camera on the viewing side of its focus"),FVector::Distance(Camera->GetActorLocation(),PanPivot)>0
		&& FVector::DotProduct(PanPivot-Camera->GetActorLocation(),Camera->GetActorForwardVector())>0);
	FHALVehicleTestAccess::Zoom(F.PC,-1000); FHALVehicleTestAccess::Zoom(F.PC,-1000);
	const FVector FarLocation=Camera->GetActorLocation(); FHALVehicleTestAccess::Zoom(F.PC,-1000);
	TestTrue(TEXT("Zoom-out reaches a finite distance limit"),!FarLocation.ContainsNaN() && Camera->GetActorLocation().Equals(FarLocation,0.01));
	F.PC->FocusObservationCamera();
	TestTrue(TEXT("Side focus recenters the orbit after pan and zoom"),FHALVehicleTestAccess::Pivot(F.PC).Equals(FirstPivot,0.01)
		&& Camera->GetActorLocation().Equals(FirstLocation,0.01));
	TestTrue(TEXT("Controller possesses same car"),F.PC->GetPawn()==Pawn);
	TestEqual(TEXT("ASC avatar unaffected"),ASC->GetAvatarActor(),static_cast<AActor*>(Pawn));
	TestTrue(TEXT("Vehicle inputs still available from side view"),Pawn->IsLocalInputAvatarLinked() && HALGameFlow::Permissions(F.World,Pawn).bAbilities);
	F.PC->ToggleTestCamera();
	TestEqual(TEXT("Returns to actual vehicle camera target"),F.PC->GetViewTarget(),static_cast<AActor*>(Pawn));
	TestEqual(TEXT("Camera changes do not reset vehicle lifecycle"),F.State()->GetVehicleLifecycle().ChangeSerial,Serial);
	TestTrue(TEXT("Camera changes do not reset GAS cooldown"),ASC->GetEffectTimeRemaining(HALGameplayTags::Cooldown_ImpactImmunity)>0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALVehicleTestFailure,"HAL.FutureCreation.VehicleTest.InvalidAndBlockedReplacementRetainsCurrentCar",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALVehicleTestFailure::RunTest(const FString&)
{
	FVehicleTestWorld F;
	if (!TestNotNull(TEXT("Initial car"),F.Vehicle())) { return false; }
	auto* Original=F.Vehicle(); const auto Serial=F.State()->GetVehicleLifecycle().ChangeSerial;
	TestFalse(TEXT("Unlisted native class refused"),F.Mode->ChangeTestVehicle(F.PC,ATestVehiclePawn::StaticClass()));
	AActor* Blocker=F.World->SpawnActor<AActor>();
	auto* Box=NewObject<UBoxComponent>(Blocker); Blocker->SetRootComponent(Box); Box->SetBoxExtent(FVector(500));
	Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent(); Blocker->SetActorLocation(FVector(0,0,1000));
	TestFalse(TEXT("Real blocking geometry refuses replacement"),F.Mode->ChangeTestVehicle(F.PC,F.Other));
	TestTrue(TEXT("Original possession and GAS preserved before commit"),F.PC->GetPawn()==Original && F.ASC()->GetAvatarActor()==Original);
	TestEqual(TEXT("Rejected requests do not consume lifecycle serial"),F.State()->GetVehicleLifecycle().ChangeSerial,Serial);
	TestEqual(TEXT("Rejected candidate is destroyed"),F.VehicleCount(),1);
	return true;
}
#endif
