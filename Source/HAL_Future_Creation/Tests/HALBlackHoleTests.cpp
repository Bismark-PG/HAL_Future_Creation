#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "../HALBlackHole.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../TestVehiclePawn.h"
#include "Components/PrimitiveComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "PBDRigidsSolver.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Chaos/SimCallbackObject.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include <limits>

namespace
{
	struct FBlackHoleProbeInput : Chaos::FSimCallbackInput
	{
		UPrimitiveComponent* Body = nullptr;
		int32 Sample = 0;
		void Reset() { Body = nullptr; Sample = 0; }
	};
	struct FBlackHoleProbeOutput : Chaos::FSimCallbackOutput
	{
		FVector Velocity = FVector::ZeroVector;
		int32 Sample = 0;
		void Reset() { Sample = 0; }
	};
	class FBlackHoleVelocityProbe : public Chaos::TSimCallbackObject<FBlackHoleProbeInput, FBlackHoleProbeOutput, Chaos::ESimCallbackOptions::PostSolve>
	{
		virtual void OnPreSimulate_Internal() override {}
		virtual void OnPostSolve_Internal() override
		{
			const auto* Input = GetConsumerInput_Internal();
			if (!Input || !Input->Body) { return; }
			auto Handle = Input->Body->GetBodyInstance()->GetBodyInstanceAsyncPhysicsTickHandle();
			if (!Handle.IsValid()) { return; }
			auto& Out = GetProducerOutputData_Internal(); Out.Velocity = Handle->V(); Out.Sample = Input->Sample;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBlackHoleRulesTest, "HAL.FutureCreation.BlackHole.RulesAndBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBlackHoleRulesTest::RunTest(const FString& Parameters)
{
	FHALBlackHoleConfig C;
	TestTrue(TEXT("Defaults valid"), C.IsValid());
	TestEqual(TEXT("Seed radius"), C.RadiusAt(0), 1.f);
	TestEqual(TEXT("Expansion endpoint"), C.RadiusAt(2), 700.f);
	TestEqual(TEXT("Pulse never shrinks range"), C.RadiusAt(6), 700.f);
	TestEqual(TEXT("No pulse while expanding"), C.PulseIndexAt(1.9), 0);
	TestEqual(TEXT("Preparation starts without immediate pulse"), C.PulseIndexAt(2), 0);
	TestEqual(TEXT("First pulse after interval"), C.PulseIndexAt(4), 1);
	TestEqual(TEXT("Second pulse"), C.PulseIndexAt(6), 2);
	TestEqual(TEXT("Lifetime includes expansion"), C.StageAt(9.99), EHALBlackHoleStage::PreparingPulse);
	TestEqual(TEXT("End wins over pulse at lifetime endpoint"), C.StageAt(10), EHALBlackHoleStage::Ended);
	TestEqual(TEXT("No pulse at end"), C.PulseIndexAt(10), 0);
	TestTrue(TEXT("Inward force is planar"), C.InwardDirection(FVector::ZeroVector, FVector(100,0,100), 3).Equals(FVector(-1,0,0)));
	TestTrue(TEXT("Exact center has no undefined direction"), C.InwardDirection(FVector::ZeroVector, FVector::ZeroVector, 0).IsZero());
	TestTrue(TEXT("Outside expanding seed"), C.InwardDirection(FVector::ZeroVector, FVector(100,0,0), 0).IsZero());
	TestTrue(TEXT("Outside vertical tolerance"), C.InwardDirection(FVector::ZeroVector, FVector(100,0,201), 3).IsZero());
	TestTrue(TEXT("Outside maximum radius"), C.InwardDirection(FVector::ZeroVector, FVector(701,0,0), 3).IsZero());
	TestTrue(TEXT("End has no pull"), C.InwardDirection(FVector::ZeroVector, FVector(100,0,0), 10).IsZero());
	const auto Weak = [&](FVector P, FVector V) { return C.ContinuousDeltaVelocity(FVector::ZeroVector,P,V,3,.02f); };
	TestTrue(TEXT("Idle is gently pulled"), Weak(FVector(200,0,0),FVector::ZeroVector).Equals(FVector(-2.4,0,0),.001));
	TestTrue(TEXT("Only inward speed sets cap"), Weak(FVector(200,0,0),FVector(-199,800,300)).Equals(FVector(-1,0,0),.001));
	TestTrue(TEXT("Weak pull stops adding at limit"), Weak(FVector(200,0,0),FVector(-200,0,0)).IsZero());
	TestTrue(TEXT("Yellow speed never braked"), Weak(FVector(200,0,0),FVector(-900,0,0)).IsZero());
	TestTrue(TEXT("Outward drive gets bounded opposition"), Weak(FVector(200,0,0),FVector(500,0,0)).Equals(FVector(-2.4,0,0),.001));
	TestTrue(TEXT("Center halves acceleration"), Weak(FVector(50,0,0),FVector::ZeroVector).Equals(FVector(-1.2,0,0),.001));
	TestTrue(TEXT("Center lowers speed ceiling"), Weak(FVector(50,0,0),FVector(-100,0,0)).IsZero());
	TestTrue(TEXT("Center has no invented direction"), Weak(FVector::ZeroVector,FVector::ZeroVector).IsZero());
	FVector Coast = FVector::ZeroVector;
	for (int32 Index = 0; Index < 500; ++Index) { Coast += Weak(FVector(500,0,0),Coast); }
	TestTrue(TEXT("Weak pull converges to bounded speed"), Coast.Equals(FVector(-200,0,0),.01));
	FVector Escape(100,0,0);
	for (int32 Index = 0; Index < 100; ++Index) { Escape += FVector(6,0,0) + Weak(FVector(500,0,0),Escape); }
	TestTrue(TEXT("Outward thrust can overcome weak pull"), Escape.X > 100);
	const auto Assisted = [&](float Speed, FVector Forward, float Throttle)
	{ return C.ContinuousDeltaVelocity(FVector::ZeroVector,FVector(200,0,0),FVector(Speed,0,0),3,.02f,Forward,Throttle); };
	TestTrue(TEXT("Full outward throttle retains 30 percent at rest"), Assisted(0,FVector::ForwardVector,1).Equals(FVector(-.72,0,0),.001));
	TestTrue(TEXT("After yellow inward motion can recover without cancelling its velocity"), Assisted(-100,FVector::ForwardVector,1).Equals(FVector(-.72,0,0),.001));
	TestTrue(TEXT("Released throttle still pulled normally"), Assisted(0,FVector::ForwardVector,0).Equals(FVector(-2.4,0,0),.001));
	TestTrue(TEXT("Facing inward gives no escape assistance"), Assisted(0,-FVector::ForwardVector,1).Equals(FVector(-2.4,0,0),.001));
	TestTrue(TEXT("Sideways throttle gives no escape assistance"), Assisted(0,FVector::RightVector,1).Equals(FVector(-2.4,0,0),.001));
	TestTrue(TEXT("Half recovery smoothly restores attraction"), Assisted(75,FVector::ForwardVector,1).Equals(FVector(-1.56,0,0),.001));
	TestTrue(TEXT("At recovery threshold attraction is normal"), Assisted(150,FVector::ForwardVector,1).Equals(FVector(-2.4,0,0),.001));
	TestTrue(TEXT("Analog throttle gives partial assistance"), Assisted(0,FVector::ForwardVector,.5f).Equals(FVector(-1.56,0,0),.001));
	C.LowSpeedAttractionScale = 1;
	TestTrue(TEXT("Scale one disables escape assistance"), Assisted(0,FVector::ForwardVector,1).Equals(FVector(-2.4,0,0),.001));
	C.LowSpeedAttractionScale = .3f;
	C.ExpansionSeconds = 0;
	TestEqual(TEXT("Zero expansion full radius immediately"), C.RadiusAt(0), C.MaximumRadius);
	C.PulseInterval = 0; TestFalse(TEXT("Zero interval rejected"), C.IsValid());
	C = {}; C.PulseDeltaSpeed = std::numeric_limits<float>::infinity(); TestFalse(TEXT("Infinite pulse rejected"), C.IsValid());
	C = {}; C.ContinuousAcceleration = -1; TestFalse(TEXT("Outward force rejected"), C.IsValid());
	C = {}; C.ContinuousSpeedLimit = 0; TestFalse(TEXT("Invalid weak speed cap rejected"), C.IsValid());
	C = {}; C.EscapeRecoverySpeed = 0; TestFalse(TEXT("Zero recovery threshold rejected"), C.IsValid());
	C = {}; C.LowSpeedAttractionScale = -1; TestFalse(TEXT("Negative escape scale rejected"), C.IsValid());
	C = {}; C.EntrySpeedMultiplier = 1.1f; TestFalse(TEXT("Entry cannot accelerate vehicles"), C.IsValid());
	C = {}; C.EntryRetriggerDelay = -1; TestFalse(TEXT("Negative entry delay rejected"), C.IsValid());
	C = {}; C.CenterSofteningRadius = std::numeric_limits<float>::quiet_NaN(); TestFalse(TEXT("Nonfinite soft center rejected"), C.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBlackHoleLifecycleTest, "HAL.FutureCreation.BlackHole.AuthorityPhysicsAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBlackHoleLifecycleTest::RunTest(const FString& Parameters)
{
	const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(true).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	// Keep deliberately backdated start times nonnegative; -1 is the production unstarted sentinel.
	World->TimeSeconds = 100;
	ON_SCOPE_EXIT { if (World->GetBegunPlay()) { World->EndPlay(EEndPlayReason::Quit); } World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* VehicleClass = LoadClass<ATestVehiclePawn>(nullptr, TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
	if (!TestNotNull(TEXT("Saved vehicle loads read-only"), VehicleClass)) { return false; }
	auto* Vehicle = World->SpawnActor<ATestVehiclePawn>(VehicleClass, FVector(200,0,10000), FRotator::ZeroRotator, Spawn);
	Vehicle->PreInitializeComponents(); Vehicle->InitializeComponents(); Vehicle->PostInitializeComponents(); Vehicle->DispatchBeginPlay();
	auto* Body = CastChecked<UPrimitiveComponent>(Vehicle->GetRootComponent());
	Body->SetEnableGravity(false);
	Body->SetLinearDamping(0);
	Body->SetAngularDamping(0);
	Body->SetSimulatePhysics(true);
	Vehicle->FindComponentByClass<UArcadeVehicleMovementComponent>()->Deactivate();
	TestTrue(TEXT("Vehicle is valid"), Vehicle->IsConfigurationValid());
	auto* Hole = World->SpawnActor<AHALBlackHole>(FVector(0,0,10000), FRotator::ZeroRotator, Spawn);
	FHALBlackHoleConfig C; C.ExpansionSeconds = 0;
	TestTrue(TEXT("Deferred spawn can configure"), Hole->ConfigureBlackHole(C));
	Hole->DispatchBeginPlay();
	TestTrue(TEXT("Replicated Actor"), Hole->GetIsReplicated());
	TestFalse(TEXT("Runtime configuration rejected"), Hole->ConfigureBlackHole(C));
	World->InitializeActorsForPlay(FURL(), false); World->SetBegunPlay(true);
	// Register the rigid body with Chaos before asking the standalone impulse path to change its velocity.
	auto* Solver = World->GetPhysicsScene()->GetSolver();
	Solver->AdvanceAndDispatch_External(1.f / 60.f); Solver->WaitOnPendingTasks_External();
	auto* Probe = Solver->CreateAndRegisterSimCallbackObject_External<FBlackHoleVelocityProbe>();
	ON_SCOPE_EXIT { Solver->UnregisterAndFreeSimCallbackObject_External(Probe); };
	int32 Sample = 0;
	const auto ReadVelocity = [&]()
	{
		// Measure the integrated Internal velocity, without depending on GT interpolation snapshots.
		auto* Input = Probe->GetProducerInputData_External(); Input->Body = Body; Input->Sample = ++Sample;
		for (int32 Attempt = 0; Attempt < 4; ++Attempt)
		{
			Solver->AdvanceAndDispatch_External(1.f / 60.f); Solver->WaitOnPendingTasks_External();
			while (auto Output = Probe->PopFutureOutputData_External())
			{
				if (Output->Sample == Sample)
				{
					AddInfo(FString::Printf(TEXT("Integrated V=%s Actions=%u"), *Output->Velocity.ToString(), Hole->ActionSequence));
					return Output->Velocity;
				}
			}
		}
		AddError(TEXT("Physics sample not produced")); return FVector::ZeroVector;
	};
	const auto Reset = [&]() { Body->SetPhysicsLinearVelocity(FVector::ZeroVector); };
	Reset(); Hole->ApplyAttraction(1, .1f, false);
	TestTrue(TEXT("Continuous pull uses acceleration times elapsed step"), ReadVelocity().Equals(FVector(-12,0,0), .1));
	Reset(); Hole->ApplyAttraction(2, 0, true);
	TestTrue(TEXT("Instant pull adds authored delta speed"), ReadVelocity().Equals(FVector(-700,0,0), .1));
	Body->SetPhysicsLinearVelocity(FVector(-900,0,0)); Hole->ApplyAttraction(2,.02f,true);
	TestTrue(TEXT("Yellow still adds full impulse above weak speed cap"), ReadVelocity().Equals(FVector(-1600,0,0),.1));
	Reset(); Hole->ApplyAttraction(2, 0, true); Hole->ApplyAttraction(2, 0, true);
	TestTrue(TEXT("Independent physical contributions add"), ReadVelocity().Equals(FVector(-1400,0,0), .1));
	Reset(); Vehicle->SetActorLocation(FVector(800,0,10000)); Hole->ApplyAttraction(2, .1f, true);
	TestTrue(TEXT("Outside field no velocity change"), Body->GetPhysicsLinearVelocity().IsNearlyZero());
	Vehicle->SetActorLocation(FVector(200,0,10000));
	Hole->StartedAt = double(World->GetTimeSeconds()) - 2.; Hole->LastUpdateAge = 1.98;
	Reset(); Hole->UpdateField();
	TestEqual(TEXT("Scheduled pulse recorded"), Hole->PulseIndex, 1);
	const FVector First = ReadVelocity(); Hole->UpdateField();
	TestTrue(TEXT("Same time update cannot repeat pulse"), ReadVelocity().Equals(First,.01));
	Hole->StartedAt = double(World->GetTimeSeconds()) - 6.; Hole->LastUpdateAge = 2.;
	Reset(); Hole->UpdateField();
	TestEqual(TEXT("Stall advances to newest pulse"), Hole->PulseIndex, 3);
	TestTrue(TEXT("Stall caps continuous integral and does not stack missed pulses"), ReadVelocity().Equals(FVector(-712,0,0),.1));
	Vehicle->SetActorLocation(FVector(800,0,10000)); Hole->ApplyAttraction(6.1,0,false);
	Vehicle->SetActorLocation(FVector(0,0,10000)); Body->SetPhysicsLinearVelocity(FVector(1000,300,50));
	Hole->ApplyAttraction(6.2,0,false);
	TestTrue(TEXT("Entry at exact center retains 40 percent planar speed and all vertical speed"), ReadVelocity().Equals(FVector(400,120,50),.1));
	Hole->ApplyAttraction(6.3,0,false);
	TestTrue(TEXT("Remaining inside never repeats entry slowdown"), ReadVelocity().Equals(FVector(400,120,50),.1));
	Vehicle->SetActorLocation(FVector(800,0,10000)); Hole->ApplyAttraction(6.4,0,false);
	Vehicle->SetActorLocation(FVector(200,0,10000)); Body->SetPhysicsLinearVelocity(FVector(1000,0,0));
	Hole->ApplyAttraction(6.5,0,false);
	TestTrue(TEXT("Rapid edge reentry does not repeatedly brake"), ReadVelocity().Equals(FVector(1000,0,0),.1));
	Vehicle->SetActorLocation(FVector(800,0,10000)); Hole->ApplyAttraction(6.6,0,false);
	Vehicle->SetActorLocation(FVector(200,0,10000)); Body->SetPhysicsLinearVelocity(FVector(1000,0,0));
	Hole->ApplyAttraction(7.3,0,true);
	TestTrue(TEXT("Rearmed entry slows first, then yellow adds full 700"), ReadVelocity().Equals(FVector(-300,0,0),.1));
	Hole->StartedAt = double(World->GetTimeSeconds()) - 8.;
	Reset(); Hole->UpdateField();
	TestTrue(TEXT("Lifetime destroys Actor"), Hole->IsActorBeingDestroyed());
	TestTrue(TEXT("End applies no final pulse"), Body->GetPhysicsLinearVelocity().IsNearlyZero());
	TestFalse(TEXT("Timer cleared"), World->GetTimerManager().IsTimerActive(Hole->UpdateTimer));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBlackHoleDebugTest, "HAL.FutureCreation.BlackHole.DebugGizmos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBlackHoleDebugTest::RunTest(const FString& Parameters)
{
	const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	auto* Hole = World->SpawnActor<AHALBlackHole>();
	Hole->bDebugDraw = true;
	Hole->OnConstruction(FTransform::Identity);
#if WITH_EDITORONLY_DATA
	TestEqual(TEXT("Three editor circles show radius and height tolerance"), Hole->DebugBounds.Num(), 3);
	for (int32 Layer = 0; Layer < Hole->DebugBounds.Num(); ++Layer)
	{
		const auto* Bounds = Hole->DebugBounds[Layer].Get();
		TestTrue(TEXT("Editor preview visibility flag enabled"), Bounds->GetVisibleFlag());
		TestEqual(TEXT("No debug collision"), Bounds->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		TestEqual(TEXT("64 point circle"), Bounds->GetNumberOfSplinePoints(), 64);
		const FVector Point = Bounds->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local);
		TestEqual(TEXT("Maximum authoring radius"), Point.X, double(Hole->Settings.MaximumRadius));
		TestEqual(TEXT("Height layer"), Point.Z, Layer == 0 ? 0. : Layer == 1 ? double(Hole->Settings.HalfHeight) : -double(Hole->Settings.HalfHeight));
	}
	Hole->bDebugDraw = false; Hole->OnConstruction(FTransform::Identity);
	for (const auto& Bounds : Hole->DebugBounds) { TestFalse(TEXT("Unchecked preview hidden"), Bounds->GetVisibleFlag()); }
#endif
#if ENABLE_DRAW_DEBUG
	if (!TestNotNull(TEXT("Debug line batch"), World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent))) { return false; }
	World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent)->Flush();
	Hole->DrawFieldDebug(1);
	TestEqual(TEXT("Unchecked no runtime lines"), World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent)->BatchedLines.Num(), 0);
	Hole->bDebugDraw = true;
	const auto CheckColor = [&](double Age, FColor Expected)
	{
		World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent)->Flush(); Hole->DrawFieldDebug(Age);
		TestTrue(TEXT("Filled disk submitted"), World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent)->BatchedMeshes.Num() > 0);
		TestTrue(TEXT("Circles and arrows submitted"), World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent)->BatchedLines.Num() > 64);
		if (!World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent)->BatchedLines.IsEmpty()) { TestEqual(TEXT("Stage color"), World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent)->BatchedLines[0].Color, FLinearColor(Expected)); }
	};
	CheckColor(1, FColor::Red); CheckColor(3, FColor::Purple); CheckColor(4.05, FColor::Yellow);
	World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent)->Flush(); Hole->DrawFieldDebug(10);
	TestEqual(TEXT("Ended field no lines"), World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent)->BatchedLines.Num(), 0);
#endif
	return true;
}
#endif
