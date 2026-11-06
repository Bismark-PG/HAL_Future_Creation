#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "../HALVehiclePresentationComponent.h"
#include "../HALVehiclePresentationConfig.h"
#include "../TestVehiclePawn.h"
#include "HALVehiclePresentationTestSupport.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include <limits>

namespace
{
	struct FFXWorld
	{
		UWorld* World;
		FFXWorld()
		{
			const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
				.ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		}
		~FFXWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
		UStaticMeshComponent* Static(AActor* Owner, USceneComponent* Parent = nullptr)
		{
			auto* Part = NewObject<UStaticMeshComponent>(Owner);
			Part->SetMobility(EComponentMobility::Movable);
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			if (Parent) { Part->SetupAttachment(Parent); } else { Owner->SetRootComponent(Part); }
			Part->RegisterComponent(); return Part;
		}
	};
	struct FFXCar
	{
		AActor* Owner;
		UStaticMeshComponent* Body;
		UStaticMeshComponent* Fallback;
		USkeletalMeshComponent* Rig;
		UHALVehiclePresentationComponent* Presentation;
		explicit FFXCar(FFXWorld& Fixture)
		{
			Owner = Fixture.World->SpawnActor<AActor>();
			Body = Fixture.Static(Owner);
			Body->SetHiddenInGame(true); // A hidden physics parent must not hide an independently visible child.
			Fallback = Fixture.Static(Owner, Body);
			Rig = NewObject<USkeletalMeshComponent>(Owner);
			Rig->SetupAttachment(Body); Rig->SetCollisionEnabled(ECollisionEnabled::NoCollision); Rig->RegisterComponent();
			Presentation = NewObject<UHALVehiclePresentationComponent>(Owner); Presentation->RegisterComponent();
			Presentation->Configure(nullptr, Body, Fallback, Rig, true);
		}
		FHALVehicleFXTargetSnapshot Capture() const
		{ FHALVehicleFXTargetSnapshot Result; Presentation->CaptureStatusFXTargets(Result); return Result; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALFXBoundsTest, "HAL.FutureCreation.StatusFX.SpacesAndInvalidGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALFXBoundsTest::RunTest(const FString&)
{
	const FBox Local(FVector(10,20,-5), FVector(30,50,15));
	const FTransform Mesh(FRotator(0,90,0), FVector(100,200,300), FVector(2,-1,3));
	const FTransform Vehicle(FRotator(0,90,0), FVector(50,100,200), FVector(2,4,5));
	FBox Bounds;
	TestTrue(TEXT("Nonzero origin, rotations, mirrored nonuniform scales accepted"), HALVehicleFX::BuildBounds(Local, Mesh, Vehicle, Bounds));
	TestTrue(TEXT("Analytically derived minimum"), Bounds.Min.Equals(FVector(60,-25,17), 0.0001));
	TestTrue(TEXT("Analytically derived maximum"), Bounds.Max.Equals(FVector(80,-17.5,29), 0.0001));
	FHALVehicleFXTarget Target; Target.MeshToWorld = Mesh;
	TestTrue(TEXT("Local point maps into physical body's space"), Target.MeshPointToVehicle(Local.Min, Vehicle).Equals(FVector(60,-17.5,17), 0.0001));
	FTransform ZeroScale = Mesh; ZeroScale.SetScale3D(FVector(1,0,1));
	TestFalse(TEXT("Zero mesh scale rejected"), HALVehicleFX::BuildBounds(Local, ZeroScale, Vehicle, Bounds));
	TestFalse(TEXT("Failure clears previously valid bounds"), bool(Bounds.IsValid));
	TestFalse(TEXT("Zero physical frame scale rejected"), HALVehicleFX::BuildBounds(Local, Mesh, ZeroScale, Bounds));
	TestFalse(TEXT("Empty bounds rejected"), HALVehicleFX::BuildBounds(FBox(ForceInit), Mesh, Vehicle, Bounds));
	TestFalse(TEXT("Inverted bounds rejected"), HALVehicleFX::BuildBounds(FBox(Local.Max,Local.Min), Mesh, Vehicle, Bounds));
	TestTrue(TEXT("Flat cosmetic geometry supported"), HALVehicleFX::BuildBounds(FBox(FVector(0,0,0),FVector(10,10,0)), Mesh, Vehicle, Bounds));
	FBox Bad = Local; Bad.Max.X = std::numeric_limits<double>::infinity();
	TestFalse(TEXT("Nonfinite bounds rejected"), HALVehicleFX::BuildBounds(Bad, Mesh, Vehicle, Bounds));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALFXPrimaryTest, "HAL.FutureCreation.StatusFX.PrimarySelectionAndReconfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALFXPrimaryTest::RunTest(const FString&)
{
	FFXWorld World; FFXCar Car(World);
	const auto Original = Car.Capture();
	if (!TestEqual(TEXT("Static fallback with no rig config"), Original.Targets.Num(), 1)) { return false; }
	TestTrue(TEXT("Uses visual child rather than hidden physics body"), Original.Targets[0].Mesh.Get() == Car.Fallback);
	TestEqual(TEXT("Reserved primary identity"), Original.Targets[0].Id, FName(TEXT("Body")));
	TestTrue(TEXT("Current initial bindings"), Car.Presentation->AreStatusFXBindingsCurrent(Original));
	int32 Notifications = 0;
	Car.Presentation->OnStatusFXBindingsChanged().AddLambda([&Notifications]() { ++Notifications; });
	ON_SCOPE_EXIT { Car.Presentation->OnStatusFXBindingsChanged().Clear(); };
	auto* Config = LoadObject<UHALVehiclePresentationConfig>(nullptr, TEXT("/Game/Data/Car_Presentation/DA_Car_01_Presentation.DA_Car_01_Presentation"));
	if (!TestNotNull(TEXT("Saved skeletal presentation"), Config)) { return false; }
	Car.Presentation->Configure(Config, Car.Body, Car.Fallback, Car.Rig, true);
	const auto Skeletal = Car.Capture();
	if (!TestEqual(TEXT("Exactly one primary; fallback excluded"), Skeletal.Targets.Num(), 1)) { return false; }
	TestTrue(TEXT("Configured skeletal mesh selected"), Skeletal.Targets[0].Mesh.Get() == Car.Rig);
	TestEqual(TEXT("Sampling kind is skeletal"), Skeletal.Targets[0].Kind, EHALVehicleFXMeshKind::Skeletal);
	TestFalse(TEXT("Old binding invalid after reconfigure"), Car.Presentation->AreStatusFXBindingsCurrent(Original));
	TestFalse(TEXT("Preview mode has no active presentation tick"), Car.Presentation->IsComponentTickEnabled());
	TestFalse(TEXT("Physics body unchanged"), Car.Body->IsSimulatingPhysics());
	Car.Rig->SetHiddenInGame(true);
	TestEqual(TEXT("Hidden primary excluded, no arbitrary fallback search"), Car.Capture().Targets.Num(), 0);
	TestFalse(TEXT("Direct visibility change detected without revision event"), Car.Presentation->AreStatusFXBindingsCurrent(Skeletal));
	Car.Rig->SetHiddenInGame(false);
	Car.Presentation->Configure(nullptr, Car.Body, Car.Fallback, Car.Rig, true);
	TestTrue(TEXT("Removing optional rig restores static path"), Car.Capture().Targets[0].Mesh.Get() == Car.Fallback);
	Car.Owner->SetActorHiddenInGame(true);
	TestEqual(TEXT("Hidden owner produces no target"), Car.Capture().Targets.Num(), 0);
	Car.Owner->SetActorHiddenInGame(false);
	Car.Presentation->ClearStatusFXBindings();
	TestEqual(TEXT("Retirement clears all targets while visual still exists"), Car.Capture().Targets.Num(), 0);
	TestTrue(TEXT("Lifecycle emitted invalidation"), Notifications > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALFXPartsTest, "HAL.FutureCreation.StatusFX.MultipartLifetimeAndFreshGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALFXPartsTest::RunTest(const FString&)
{
	FFXWorld World; FFXCar Car(World); FFXCar Other(World);
	auto* Z = World.Static(Car.Owner, Car.Body); auto* A = World.Static(Car.Owner, Car.Body);
	A->SetRelativeTransform(FTransform(FRotator(25,40,10), FVector(120,60,35), FVector(0.5,2,1)));
	TestFalse(TEXT("Physics body cannot register as visual"), Car.Presentation->RegisterStatusFXPart(TEXT("Physics"), Car.Body));
	TestFalse(TEXT("Reserved identity rejected"), Car.Presentation->RegisterStatusFXPart(TEXT("Body"), A));
	TestFalse(TEXT("Empty identity rejected"), Car.Presentation->RegisterStatusFXPart(NAME_None, A));
	TestFalse(TEXT("Foreign vehicle rejected"), Car.Presentation->RegisterStatusFXPart(TEXT("Foreign"), Other.Fallback));
	TestTrue(TEXT("Register reverse lexical order"), Car.Presentation->RegisterStatusFXPart(TEXT("ZPart"), Z));
	TestTrue(TEXT("Second part"), Car.Presentation->RegisterStatusFXPart(TEXT("APart"), A));
	const auto First = Car.Capture();
	if (!TestEqual(TEXT("Body plus two parts"), First.Targets.Num(), 3)) { return false; }
	TestEqual(TEXT("Order independent of registration/enumeration"), First.Targets[1].Id, FName(TEXT("APart")));
	TestTrue(TEXT("Idempotent assembly call"), Car.Presentation->RegisterStatusFXPart(TEXT("APart"), A));
	TestTrue(TEXT("No invalidation for identical registration"), Car.Presentation->AreStatusFXBindingsCurrent(First));
	TestFalse(TEXT("Same component cannot occupy multiple roles"), Car.Presentation->RegisterStatusFXPart(TEXT("Duplicate"), A));
	TestFalse(TEXT("Live Id cannot be replaced silently"), Car.Presentation->RegisterStatusFXPart(TEXT("APart"), Z));
	const auto PhysicsTransform = Car.Body->GetComponentTransform();
	const auto Collision = Car.Body->GetCollisionEnabled();
	A->AddLocalOffset(FVector(50,0,10));
	const auto Moved = Car.Capture();
	TestTrue(TEXT("Binding stable across animation/movement"), Car.Presentation->AreStatusFXBindingsCurrent(First));
	TestFalse(TEXT("Fresh geometry rather than cached bounds"), First.Targets[1].VehicleBounds.Equals(Moved.Targets[1].VehicleBounds));
	TestTrue(TEXT("Union contains transformed part"), Moved.VehicleBounds.IsInsideOrOn(Moved.Targets[1].VehicleBounds.Min));
	TestTrue(TEXT("Queries do not move physics body"), PhysicsTransform.Equals(Car.Body->GetComponentTransform()));
	TestEqual(TEXT("Queries do not change collision"), Car.Body->GetCollisionEnabled(), Collision);
	A->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	TestFalse(TEXT("Same component asset replacement invalidates identity"), Car.Presentation->AreStatusFXBindingsCurrent(Moved));
	A->SetRelativeScale3D(FVector::ZeroVector);
	TestEqual(TEXT("Degenerate part excluded"), Car.Capture().Targets.Num(), 2);
	Z->DestroyComponent();
	TestEqual(TEXT("Destroyed part skipped safely"), Car.Capture().Targets.Num(), 1);
	Car.Presentation->UnregisterStatusFXPart(TEXT("APart"));
	Car.Presentation->Configure(nullptr, Car.Body, Car.Fallback, Car.Rig, true);
	TestEqual(TEXT("Reconfigure removes previous assembly's parts"), Car.Capture().Targets.Num(), 1);
	Car.Fallback->UnregisterComponent();
	TestEqual(TEXT("Unregistered components excluded"), Car.Capture().Targets.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALFXSavedCarsTest, "HAL.FutureCreation.StatusFX.SavedVehiclesAndDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALFXSavedCarsTest::RunTest(const FString&)
{
	FFXWorld World;
	for (const TCHAR* Path : { TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"),
		TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_02.BP_VehiclePawn_Car_02_C"),
		TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_03.BP_VehiclePawn_Car_03_C") })
	{
		auto* Class = LoadClass<ATestVehiclePawn>(nullptr, Path);
		if (!TestNotNull(Path, Class)) { continue; }
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Pawn = World.World->SpawnActor<ATestVehiclePawn>(Class, FVector(0,0,1000), FRotator::ZeroRotator, Params);
		if (!TestNotNull(TEXT("Spawn saved car"), Pawn)) { continue; }
		Pawn->PreInitializeComponents(); Pawn->InitializeComponents(); Pawn->PostInitializeComponents();
		TestTrue(TEXT("Saved vehicle initializes successfully"), Pawn->IsConfigurationValid());
		auto* Presentation = Pawn->FindComponentByClass<UHALVehiclePresentationComponent>();
		FHALVehicleFXTargetSnapshot Snapshot; Presentation->CaptureStatusFXTargets(Snapshot);
		if (TestEqual(TEXT("Saved car produces one primary target"), Snapshot.Targets.Num(), 1))
		{
			TestTrue(TEXT("Rig chosen from actual initialized presentation"), Snapshot.Targets[0].Mesh.Get() == Presentation->GetPresentationMesh());
			TestTrue(TEXT("Actual local bounds include geometry"), Snapshot.Targets[0].LocalBounds.GetVolume() > 0);
		}
		Pawn->RetireForVehicleTransition();
		Presentation->CaptureStatusFXTargets(Snapshot);
		TestEqual(TEXT("Retired vehicle no longer exports visual bindings"), Snapshot.Targets.Num(), 0);
	}
	// A live, generic visual fixture exercises the production diagnostic command as well.
	FFXCar Car(World);
	TestTrue(TEXT("One-shot diagnostics available"), IConsoleManager::Get().ProcessUserConsoleInput(TEXT("hal.VehicleFXTargets"), *GLog, World.World));
	return true;
}
#endif
