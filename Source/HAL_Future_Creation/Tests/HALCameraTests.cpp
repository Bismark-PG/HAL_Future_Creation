#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALCameraTestSupport.h"
#include "HALLegacyCombatTestRules.h"
#include "../BasicBallActor.h"
#include "../HALPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "Math/InverseRotationMatrix.h"
#include "SceneView.h"
#include "UObject/UnrealType.h"

namespace
{
	UHALCameraConfig* CameraConfig()
	{
		auto* C = NewObject<UHALCameraConfig>(); C->bReadyForUse = true;
		C->BaseCamera.BoomTransform = FTransform(FRotator(-75,0,0)); return C;
	}
	FHALCameraFrame Step(const UHALCameraConfig& C, FHALCameraState& S, float Dt = 1.f/60,
		FVector2D Mouse = FVector2D::ZeroVector, FVector2D Stick = FVector2D::ZeroVector, bool bAccept = true)
	{ return HALCamera::Advance(C,FVector::ZeroVector,{},FVector2D(960,540),Dt,Mouse,Stick,bAccept,true,S); }
	bool Inside(const FHALCameraFrame& F, const UHALCameraConfig& C)
	{
		const FVector2D Half = C.HardFrameSize*.5-FVector2D(C.OwnScreenPadding);
		return F.bValid && FMath::Abs(F.OwnScreen.X-.5) <= Half.X+1.e-5 && FMath::Abs(F.OwnScreen.Y-.5) <= Half.Y+1.e-5;
	}
	struct FCameraWorld
	{
		FScopedHALLegacyCombatTestRules Rules;
		UWorld* World;
		FCameraWorld()
		{
			const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
				.ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Options);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		}
		ATestVehiclePawn* Vehicle(FVector At)
		{
			auto* Class = LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
			FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Pawn = World->SpawnActor<ATestVehiclePawn>(Class,At,FRotator::ZeroRotator,Spawn);
			Pawn->PreInitializeComponents(); Pawn->InitializeComponents(); Pawn->PostInitializeComponents();
			auto* PC = World->SpawnActor<APlayerController>(); PC->SetAsLocalPlayerController();
			auto* PS = World->SpawnActor<AHALPlayerState>(); PS->SetOwner(PC); PC->SetPlayerState(PS);
			PS->PreInitializeComponents(); PS->InitializeComponents(); PS->PostInitializeComponents(); PS->DispatchBeginPlay();
			PC->Possess(Pawn); return Pawn;
		}
		ABasicBallActor* Ball(FVector At)
		{
			FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<ABasicBallActor>(At,FRotator::ZeroRotator,Spawn);
		}
		~FCameraWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCameraConfigTest,"HAL.FutureCreation.Camera.ConfigurationAndMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCameraConfigTest::RunTest(const FString&)
{
	auto* C = CameraConfig(); FString Error;
	TestTrue(TEXT("Usable native defaults"),C->Validate(Error));
	C->bReadyForUse = false; TestFalse(TEXT("Explicit migration required"),C->Validate(Error)); C->bReadyForUse = true;
	C->SoftFrameSize.X = .8; TestFalse(TEXT("Soft cannot exceed hard"),C->Validate(Error)); C->SoftFrameSize.X = .4;
	C->BaseCamera.TargetArmLength = C->MaximumDistance+1; TestFalse(TEXT("Base within zoom limits"),C->Validate(Error)); C->BaseCamera.TargetArmLength = 1200;
	C->BaseCamera.bDoCollisionTest = true; TestFalse(TEXT("Unsupported arm collision is explicit"),C->Validate(Error)); C->BaseCamera.bDoCollisionTest = false;
	C->BaseCamera.BoomTransform = FTransform(FRotator::ZeroRotator); TestFalse(TEXT("Horizontal camera rejected"),C->Validate(Error));
#if WITH_EDITOR
	auto* Saved = LoadObject<UVehicleLocalConfig>(nullptr,TEXT("/Game/Data/MVP_B/DA_Local_TestBaseline_v1.DA_Local_TestBaseline_v1"));
	if (!TestNotNull(TEXT("Saved legacy config"),Saved)) { return false; }
	const bool Dirty = Saved->GetPackage()->IsDirty();
	C->LegacySource = Saved; C->CopyLegacyCamera();
	TestTrue(TEXT("Copy full actual rig"),C->BaseCamera.BoomTransform.Equals(Saved->Camera.BoomTransform));
	TestEqual(TEXT("Actual saved FOV"),C->BaseCamera.FieldOfView,Saved->Camera.FieldOfView);
	TestEqual(TEXT("Actual saved base distance"),C->BaseCamera.TargetArmLength,Saved->Camera.TargetArmLength);
	TestEqual(TEXT("Source remains unchanged"),Saved->GetPackage()->IsDirty(),Dirty);
	TestTrue(TEXT("Migrated profile valid"),C->Validate(Error));
#endif
	FHALCameraTargetRule Rule;
	TestEqual(TEXT("Far targets ignored"),Rule.Evaluate(Rule.ZeroWeightDistance),0.f);
	TestTrue(TEXT("Smooth middle falloff"),FMath::IsNearlyEqual(Rule.Evaluate((Rule.FullWeightDistance+Rule.ZeroWeightDistance)*.5),Rule.Weight*.5));
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCameraPrimaryEditTest,"HAL.FutureCreation.Camera.PlacedPrimaryBallEditability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCameraPrimaryEditTest::RunTest(const FString&)
{
	FCameraWorld F;
	auto* Ball = F.Ball(FVector::ZeroVector); auto* Other = F.Ball(FVector(800,0,0));
	if (!TestNotNull(TEXT("Transient placed ball"),Ball) || !TestNotNull(TEXT("Other unmarked ball"),Other)) { return false; }
	auto* Primary = FindFProperty<FBoolProperty>(Ball->GetClass(),TEXT("bPrimaryCameraTarget"));
	auto* Source = FindFProperty<FEnumProperty>(Ball->GetClass(),TEXT("ConfigurationSource"));
	auto* Definition = FindFProperty<FObjectProperty>(Ball->GetClass(),TEXT("Definition"));
	auto* LegacyDamage = FindFProperty<FFloatProperty>(Ball->GetClass(),TEXT("VehicleHitDamage"));
	if (!TestNotNull(TEXT("Primary property"),Primary) || !TestNotNull(TEXT("Source property"),Source)
		|| !TestNotNull(TEXT("Definition property"),Definition) || !TestNotNull(TEXT("Legacy tuning property"),LegacyDamage)) { return false; }
	Source->GetUnderlyingProperty()->SetIntPropertyValue(Source->ContainerPtrToValuePtr<void>(Ball),uint64(EVehicleConfigurationSource::Definition));
	TestTrue(TEXT("Data Asset mode allows the placed primary marker"),Ball->CanEditChange(Primary));
	TestTrue(TEXT("Definition reference stays editable"),Ball->CanEditChange(Definition));
	TestFalse(TEXT("Legacy damage stays locked in Data Asset mode"),Ball->CanEditChange(LegacyDamage));
	TestTrue(TEXT("Marker remains editable, replicated and disabled on class defaults"),Primary->HasAllPropertyFlags(CPF_Edit | CPF_Net | CPF_DisableEditOnTemplate));
	TestFalse(TEXT("Marker is not read-only or disabled on instances"),Primary->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnInstance));
	Primary->SetPropertyValue_InContainer(Ball,true);
	TestTrue(TEXT("Editing one placed marker is readable by camera targeting"),Ball->IsPrimaryCameraTarget());
	TestFalse(TEXT("Other balls are not implicitly marked"),Other->IsPrimaryCameraTarget());
	TestFalse(TEXT("Default and debug-spawned balls remain unmarked"),GetDefault<ABasicBallActor>()->IsPrimaryCameraTarget());
	Source->GetUnderlyingProperty()->SetIntPropertyValue(Source->ContainerPtrToValuePtr<void>(Ball),uint64(EVehicleConfigurationSource::Legacy));
	TestTrue(TEXT("Legacy mode still permits its original tuning"),Ball->CanEditChange(LegacyDamage));
	return true;
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCameraProjectionTest,"HAL.FutureCreation.Camera.EngineProjectionParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCameraProjectionTest::RunTest(const FString&)
{
	auto* C = CameraConfig(); C->BaseCamera.BoomTransform = FTransform(FRotator(-65,35,0),FVector(10,20,30));
	C->BaseCamera.CameraTransform = FTransform(FRotator(-3,2,0),FVector(10,0,0)); C->BaseCamera.SocketOffset = FVector(5,10,15);
	for (const float Aspect : { 16.f/9, 4.f/3, 9.f/16 })
	for (const float FOV : { 50.f,90.f,120.f })
	{
		C->BaseCamera.FieldOfView = FOV; const auto V = HALCamera::View(*C,FVector(20,40,50),1200,Aspect);
		const FMatrix Rotation = FInverseRotationMatrix(V.Rotation)*FMatrix(FPlane(0,0,1,0),FPlane(1,0,0,0),FPlane(0,1,0,0),FPlane(0,0,0,1));
		const FMatrix VP = FTranslationMatrix(-V.Location)*Rotation*V.CalculateProjectionMatrix();
		for (const FVector Point : { FVector(0,0,0),FVector(120,-350,200),FVector(-200,300,-80) })
		{
			FVector2D Ours, Engine;
			TestTrue(TEXT("Own projection valid"),HALCamera::Project(V,Point,Ours));
			TestTrue(TEXT("Engine projection valid"),FSceneView::ProjectWorldToScreen(Point,FIntRect(0,0,1000,1000),VP,Engine));
			TestTrue(TEXT("Normalized projection matches engine matrix"),Ours.Equals(Engine/1000,1.e-5));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCameraFrameTest,"HAL.FutureCreation.Camera.FinalFrameAndOutliers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCameraFrameTest::RunTest(const FString&)
{
	auto* C = CameraConfig(); FHALCameraState S; FHALCameraFrame F;
	TArray<FHALCameraTarget> Targets { {FVector(0,5000,800),3,1,TEXT("Ball")},{FVector(50000,50000,0),0,1,TEXT("Ignored")} };
	for (int32 I = 0; I < 180; ++I)
	{
		const FVector Own(I*80,I*30,800*FMath::Sin(I*.1));
		const FVector2D Size = I%2 ? FVector2D(540,960) : FVector2D(1280,720);
		F = HALCamera::Advance(*C,Own,Targets,Size,1.f/60,FVector2D(500,-300),{},true,true,S);
		if (!Inside(F,*C)) { AddError(FString::Printf(TEXT("Final own frame outside at sample %d: %.6f/%.6f"),I,F.OwnScreen.X,F.OwnScreen.Y)); return false; }
		TestTrue(TEXT("Distance respects limits"),F.Distance >= C->MinimumDistance && F.Distance <= C->MaximumDistance);
	}
	FHALCameraState A,B;
	const auto Empty = HALCamera::Advance(*C,{}, {},FVector2D(960,540),.02,{}, {},true,true,A);
	const auto Ignored = HALCamera::Advance(*C,{},{{FVector(50000,0,0),0,1,TEXT("Far")}},FVector2D(960,540),.02,{}, {},true,true,B);
	TestEqual(TEXT("Zero weight outlier cannot force zoom"),Empty.DesiredDistance,Ignored.DesiredDistance);
	S = {}; Step(*C,S); const auto Low = HALCamera::Advance(*C,{},{{FVector(50000,0,0),.001f,1,TEXT("Weak")}},FVector2D(960,540),.02,{}, {},true,true,S);
	TestTrue(TEXT("Tiny weight has bounded zoom contribution"),Low.DesiredDistance < C->BaseCamera.TargetArmLength+2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCameraInputTest,"HAL.FutureCreation.Camera.InputRatesReturnAndBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCameraInputTest::RunTest(const FString&)
{
	auto* C = CameraConfig(); FHALCameraState A,B; Step(*C,A); Step(*C,B);
	for (int32 I = 0; I < 30; ++I) { Step(*C,A,1.f/30,FVector2D(1,0)); }
	for (int32 I = 0; I < 120; ++I) { Step(*C,B,1.f/120,FVector2D(.25,0)); }
	TestTrue(TEXT("Mouse delta independent of frame time"),A.ManualOffset.Equals(B.ManualOffset,1.e-6));
	A = {}; B = {}; Step(*C,A); Step(*C,B);
	for (int32 I = 0; I < 30; ++I) { Step(*C,A,1.f/30,{},FVector2D(.4,0)); }
	for (int32 I = 0; I < 120; ++I) { Step(*C,B,1.f/120,{},FVector2D(.4,0)); }
	TestTrue(TEXT("Stick speed integrates time"),A.ManualOffset.Equals(B.ManualOffset,1.e-5));
	const auto Before = A.ManualOffset;
	for (int32 I = 0; I < 20; ++I) { Step(*C,A,.02); }
	TestTrue(TEXT("Delay before return"),A.ManualOffset.Equals(Before,1.e-6));
	for (int32 I = 0; I < 80; ++I) { Step(*C,A,.02); }
	TestTrue(TEXT("Smooth return reaches near zero"),A.ManualOffset.Size() < Before.Size()*.02);
	for (int32 I = 0; I < 120; ++I) { Step(*C,A,.02,FVector2D(100,100)); }
	const auto AtLimit = A.ManualOffset; const auto Reverse = Step(*C,A,.02,FVector2D(-2,-2));
	TestTrue(TEXT("Boundary feedback reverses immediately"),Reverse.ManualOffset.X < AtLimit.X && Reverse.ManualOffset.Y > AtLimit.Y);
	Step(*C,A,.02,{}, {},false); TestTrue(TEXT("UI/focus gate clears offset"),A.ManualOffset.IsNearlyZero());
	Step(*C,A,.02,FVector2D(20,0)); Step(*C,A,.5); TestTrue(TEXT("Hitch clears stale observation"),A.ManualOffset.IsNearlyZero());
	Step(*C,A,.02,FVector2D(20,0));
	HALCamera::Advance(*C,FVector(10000,0,0),{},FVector2D(960,540),.02,{}, {},true,true,A);
	TestTrue(TEXT("Teleport reseeds observation"),A.ManualOffset.IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCameraLocalTest,"HAL.FutureCreation.Camera.LocalOwnershipTargetsAndLiveTuning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCameraLocalTest::RunTest(const FString&)
{
	FCameraWorld F; auto* A = F.Vehicle(FVector(0,0,80)); auto* B = F.Vehicle(FVector(0,1000,80));
	auto* CA = A->FindComponentByClass<UHALDynamicCameraComponent>(); auto* CB = B->FindComponentByClass<UHALDynamicCameraComponent>();
	auto* Config = FHALCameraTestAccess::Enable(A); FHALCameraTestAccess::Enable(B,70,1500);
	auto* Ball = F.Ball(FVector(800,0,80)); auto* Distractor = F.Ball(FVector(-800,0,80));
	Ball->SetPrimaryCameraTarget(true); FMinimalViewInfo VA,VB;
	TestTrue(TEXT("Local view computed"),FHALCameraTestAccess::Compute(CA,.02,VA));
	FHALCameraTestAccess::Compute(CB,.02,VB);
	TestEqual(TEXT("Only explicitly marked ball"),CA->GetDebugFrame().MainBallCandidates,1);
	TestEqual(TEXT("Own/player/mainball candidates"),CA->GetDebugFrame().Targets.Num(),3);
	TestEqual(TEXT("Local FOV A"),VA.FOV,90.f); TestEqual(TEXT("Local FOV B independent"),VB.FOV,70.f);
	CA->AddMouseDelta(FVector2D(40,0)); FHALCameraTestAccess::Compute(CA,.02,VA); FHALCameraTestAccess::Compute(CB,.02,VB);
	TestTrue(TEXT("Manual state is local"),!CA->GetDebugFrame().ManualOffset.IsNearlyZero() && CB->GetDebugFrame().ManualOffset.IsNearlyZero());
	Config->BaseCamera.FieldOfView = 65; Config->BaseCamera.TargetArmLength = 1800; Config->OtherPlayers.Weight = 0;
	Config->MainBall.Weight = 0; Config->DistanceDeadZone = 0;
	for (int32 I = 0; I < 250; ++I) { FHALCameraTestAccess::Compute(CA,.02,VA); }
	TestEqual(TEXT("Live FOV in actual output"),VA.FOV,65.f);
	TestTrue(TEXT("Live base distance in actual output"),FMath::IsNearlyEqual(CA->GetDebugFrame().Distance,1800.f,1.f));
	Distractor->SetPrimaryCameraTarget(true); FHALCameraTestAccess::Compute(CA,.02,VA);
	TestEqual(TEXT("Duplicate marks are diagnosed"),CA->GetDebugFrame().MainBallCandidates,2);
	TestNull(TEXT("No arbitrary ball on duplicate"),CA->GetDebugFrame().Targets.FindByPredicate([](const FHALCameraTarget& T) { return T.Label == TEXT("MainBall"); }));
	Ball->Destroy(); Distractor->Destroy(); FHALCameraTestAccess::Compute(CA,.02,VA);
	TestEqual(TEXT("Destroyed primary clears"),CA->GetDebugFrame().MainBallCandidates,0);
	TestFalse(TEXT("No viewport safely uses legacy"),CA->CalculateView(.02,VA));
	Cast<APlayerController>(A->GetController())->UnPossess();
	TestFalse(TEXT("Unpossessed view not evaluated"),FHALCameraTestAccess::Compute(CA,.02,VA));
	return true;
}
#endif
