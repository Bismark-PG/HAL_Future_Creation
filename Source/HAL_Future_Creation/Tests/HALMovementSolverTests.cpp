#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../VehicleConfigurationTypes.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTLS.h"
#include "PBDRigidsSolver.h"
#include "Chaos/SimCallbackObject.h"
#include "Chaos/Framework/PhysicsSolverBase.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

namespace
{
	struct FMovementProbeInput : Chaos::FSimCallbackInput
	{
		TFunction<void()> Work;
		UBoxComponent* Body = nullptr;
		UArcadeVehicleMovementComponent* Movement = nullptr;
		int32 Id = 0;
		void Reset() { Work = {}; Body = nullptr; Movement = nullptr; Id = 0; }
	};
	struct FMovementProbeOutput : Chaos::FSimCallbackOutput
	{
		int32 Id = 0; FVector Velocity = FVector::ZeroVector; FHALMovementStatus Status;
		bool bPhysicsContext = false; uint32 Thread = 0;
		void Reset() { Id = 0; }
	};
	class FMovementProbe : public Chaos::TSimCallbackObject<FMovementProbeInput, FMovementProbeOutput,
		Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::PostSolve>
	{
		int32 Last = 0, Pending = 0; uint32 Thread = 0; bool bPhysics = false;
		UArcadeVehicleMovementComponent* Movement = nullptr;
		FBodyInstanceAsyncPhysicsTickHandle Handle;
		virtual void OnPreSimulate_Internal() override
		{
			const auto* Input = GetConsumerInput_Internal();
			if (!Input || Input->Id == 0 || Last == Input->Id) { return; }
			Handle = Input->Body->GetBodyInstance()->GetBodyInstanceAsyncPhysicsTickHandle();
			if (!Handle.IsValid()) { return; }
			Last = Pending = Input->Id; Movement = Input->Movement;
			Thread = FPlatformTLS::GetCurrentThreadId(); bPhysics = Chaos::IsInPhysicsThreadContext();
			Handle->SetObjectState(Chaos::EObjectStateType::Dynamic, true);
			Handle->SetV(FVector::ZeroVector); Handle->SetW(FVector::ZeroVector);
			Handle->SetLinearImpulse(FVector::ZeroVector, false);
			Input->Work();
		}
		virtual void OnPostSolve_Internal() override
		{
			if (!Pending) { return; }
			auto& Output = GetProducerOutputData_Internal(); Output.Id = Pending;
			Output.Velocity = Handle->V(); Output.Status = Movement->GetMovementEffectStatus();
			Output.bPhysicsContext = bPhysics; Output.Thread = Thread; Pending = 0;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMovementSolverTest, "HAL.FutureCreation.Movement.SolverForcesAndHistoricalReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALMovementSolverTest::RunTest(const FString& Parameters)
{
	const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(true).SetTransactional(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	AActor* Actor = World->SpawnActor<AActor>();
	auto* Body = NewObject<UBoxComponent>(Actor); Actor->AddInstanceComponent(Body); Actor->SetRootComponent(Body);
	Body->SetBoxExtent(FVector(40)); Body->SetCollisionProfileName(TEXT("PhysicsActor")); Body->SetSimulatePhysics(true);
	Body->SetEnableGravity(false); Body->SetLinearDamping(0); Body->SetAngularDamping(0); Body->SetWorldLocation(FVector(0,0,2000)); Body->RegisterComponent();
	auto* Movement = NewObject<UArcadeVehicleMovementComponent>(Actor); Actor->AddInstanceComponent(Movement);
	Movement->SetUpdatedPrimitive(Body); FArcadeVehicleConfig Config; Config.Drive.CoastingDragRate = 0;
	TestTrue(TEXT("Fixture configuration"), Movement->ApplyConfiguration(Config)); Movement->RegisterComponent(); Movement->SetAsyncPhysicsTickEnabled(false);
	auto* Solver = World->GetPhysicsScene()->GetSolver(); Solver->SetThreadingMode_External(Chaos::EThreadingModeTemp::TaskGraph);
	const auto Step = [Solver]() { Solver->AdvanceAndDispatch_External(1.f/60); Solver->WaitOnPendingTasks_External(); };
	Step(); auto* Probe = Solver->CreateAndRegisterSimCallbackObject_External<FMovementProbe>();
	FGuid Epoch = FGuid::NewGuid(); Movement->ConfigureMovementEffects(Epoch, nullptr, true, 7);
	int32 Id = 0; bool bWorker = false;
	const uint32 GT = FPlatformTLS::GetCurrentThreadId();
	auto Run = [&](const TCHAR* Label, TFunction<void()> Work, double ExpectedX, double ExpectedY = 0)
	{
		auto* Input = Probe->GetProducerInputData_External(); Input->Work = MoveTemp(Work); Input->Movement = Movement; Input->Body = Body; Input->Id = ++Id;
		Step(); FHALMovementStatus Status;
		bool bFound = false;
		while (auto Output = Probe->PopFutureOutputData_External())
		{
			if (Output->Id != Id) { continue; }
			bFound = true; Status = Output->Status; bWorker |= Output->Thread != GT;
			TestTrue(FString(Label)+TEXT(" physics context"), Output->bPhysicsContext);
			TestTrue(FString(Label)+TEXT(" measured integrated velocity"), FMath::IsNearlyEqual(Output->Velocity.X, ExpectedX, 0.02));
			TestTrue(FString(Label)+TEXT(" measured lateral velocity"), FMath::IsNearlyEqual(Output->Velocity.Y, ExpectedY, 0.02));
		}
		TestTrue(FString(Label)+TEXT(" callback returned"), bFound); return Status;
	};
	auto Publish = [&](uint32 Revision, float Accel, float Speed, float Grip = 1)
	{
		FHALMovementSnapshot Value; Value.Epoch = Epoch; Value.Revision = Revision; Value.AccelerationScale = Accel; Value.SpeedScale = Speed;
		Value.GripScale = Grip;
		TestTrue(TEXT("Authority publication accepted"), Movement->QueueMovementParameters(Value));
	};
	auto Drive = [Movement,Body,Solver](bool bReverse, float Initial, int32 ReplayFrame = INDEX_NONE)
	{
		auto Handle = Body->GetBodyInstance()->GetBodyInstanceAsyncPhysicsTickHandle(); Handle->SetV(FVector(Initial,0,0));
		Movement->PrepareMovementFrame(Solver, ReplayFrame == INDEX_NONE ? Solver->GetCurrentFrame() : ReplayFrame, ReplayFrame != INDEX_NONE);
		Movement->InputCommand.Reset(); if (bReverse) { Movement->InputCommand.Brake = 1; } else { Movement->InputCommand.Throttle = 1; }
		Movement->ApplyLongitudinalForces(*Body->GetBodyInstance(), FVector::ForwardVector, Initial);
	};
	Publish(1,1,1); Run(TEXT("Neutral"), [=](){ Drive(false,0); },30);
	Publish(2,1.5f,1.25f); const auto Boosted = Run(TEXT("Boost"), [=](){ Drive(false,0); },45);
	Run(TEXT("Reverse boost"), [=](){ Drive(true,0); },-30);
	Run(TEXT("Boosted cap permits drive above basic cap"), [=](){ Drive(false,2700); },2745);
	Publish(3,1,1); Run(TEXT("Restored cap uses existing correction"), [=](){ Drive(false,2700); },2685);
	const auto Replayed = Run(TEXT("Old boosted frame after expiry"), [=](){ Drive(false,0,Boosted.LocalFrame); },45);
	TestTrue(TEXT("Production replay uses old revision"), Replayed.bReplay && Replayed.Value.Revision == 2);
	Publish(4,0,1); Run(TEXT("Zero drive preserves external velocity"), [=](){ Drive(false,100); },100);
	// Production input gating on the actual physics worker, including replay after expiry.
	auto Stun = Boosted.Value; Stun.Revision = 5; Stun.EffectiveServerFrame = INDEX_NONE; Stun.bControlsBlocked = true;
	TestTrue(TEXT("Publish control block"), Movement->QueueMovementParameters(Stun));
	auto StunStep = [Movement,Body,Solver](int32 ReplayFrame = INDEX_NONE)
	{
		auto Handle = Body->GetBodyInstance()->GetBodyInstanceAsyncPhysicsTickHandle();
		Handle->SetV(FVector(100,0,0)); Handle->SetW(FVector(0,0,.5));
		Movement->PrepareMovementFrame(Solver, ReplayFrame == INDEX_NONE ? Solver->GetCurrentFrame() : ReplayFrame, ReplayFrame != INDEX_NONE);
		FVehicleNetInputData Input; Input.Cmd.Throttle = 1; Input.Cmd.Brake = 1; Input.Cmd.Steering = 1;
		Input.Cmd.bHandbrake = Input.Cmd.bSelfRight = Input.Cmd.bLaunch = true;
		Movement->SimulateVehicleStep(Input,1.f/60);
	};
	const auto Blocked = Run(TEXT("Stun preserves external velocity"), [=](){ StunStep(); },100);
	TestTrue(TEXT("Stun reached physics state"), Blocked.Value.bControlsBlocked);
	TestTrue(TEXT("Driving inputs cleared"), Movement->InputCommand.Throttle == 0 && Movement->InputCommand.Brake == 0
		&& Movement->InputCommand.Steering == 0 && !Movement->InputCommand.bHandbrake && !Movement->InputCommand.bSelfRight && !Movement->InputCommand.bLaunch);
	Publish(6,1,1); Run(TEXT("Stun release restores driving"), [=](){ Drive(false,0); },30);
	const auto OldStun = Run(TEXT("Past stun replays after release"), [=](){ StunStep(Blocked.LocalFrame); },100);
	TestTrue(TEXT("Replay uses historical control block"), OldStun.bReplay && OldStun.Value.bControlsBlocked);
	auto Lateral = [Movement,Body,Solver](float Initial, bool bHandbrake, int32 ReplayFrame = INDEX_NONE)
	{
		auto Handle = Body->GetBodyInstance()->GetBodyInstanceAsyncPhysicsTickHandle(); Handle->SetV(FVector(0,Initial,0));
		Movement->PrepareMovementFrame(Solver,ReplayFrame == INDEX_NONE ? Solver->GetCurrentFrame() : ReplayFrame,ReplayFrame != INDEX_NONE);
		Movement->InputCommand.Reset(); Movement->InputCommand.bHandbrake = bHandbrake;
		Movement->ApplyLateralGrip(*Body->GetBodyInstance(),FVector::RightVector,Initial);
	};
	Publish(7,1,1); Run(TEXT("Normal lateral force"),[=](){ Lateral(100,false); },0,100-100*Config.Grip.LateralGripRate/60);
	Publish(8,1,1,.2f); const auto Sliding = Run(TEXT("Slippery force"),[=](){ Lateral(100,false); },0,100-100*Config.Grip.LateralGripRate*.2/60);
	Run(TEXT("Slippery acceleration cap"),[=](){ Lateral(10000,false); },0,10000-Config.Grip.MaxLateralGripAcceleration*.2/60);
	Run(TEXT("Handbrake uses same grip channel"),[=](){ Lateral(100,true); },0,100-100*Config.Grip.HandbrakeGripRate*.2/60);
	Publish(9,1,1,0); Run(TEXT("Zero grip preserves lateral inertia"),[=](){ Lateral(100,false); },0,100);
	const auto GripReplay = Run(TEXT("Historical grip after removal"),[=](){ Lateral(100,false,Sliding.LocalFrame); },0,100-100*Config.Grip.LateralGripRate*.2/60);
	TestTrue(TEXT("Real physics replay uses historical grip"),GripReplay.bReplay && FMath::IsNearlyEqual(GripReplay.Value.GripScale,.2f));
	Run(TEXT("Return to current authority after replay"), [=](){ Drive(false, 0); }, 30);
	FHALMovementSnapshot Charge;
	Charge.Epoch = Epoch; Charge.Revision = 10;
	Charge.Overdrive.Activation = FGuid::NewGuid(); Charge.Overdrive.bActive = true;
	Charge.Overdrive.RampSeconds = 0;
	TestTrue(TEXT("Publish overdrive on the normal parameter channel"), Movement->QueueMovementParameters(Charge));
	const auto Charging = Run(TEXT("Charge force"), [=](){ Drive(false, 0); }, 45);
	Run(TEXT("Charge cap permits overspeed"), [=](){ Drive(false, 3000); }, 3045);
	Charge = Charging.Definition; Charge.Revision = 11;
	Charge.bControlsBlocked = Charge.Overdrive.bStunned = true;
	TestTrue(TEXT("Publish charge stun"), Movement->QueueMovementParameters(Charge));
	FVehicleBounceBackState SameFrameBounce;
	SameFrameBounce.bActive = true; SameFrameBounce.Sequence = 1; SameFrameBounce.Direction = FVector::ForwardVector;
	SameFrameBounce.Distance = 400; SameFrameBounce.Duration = .5f;
	Movement->PendingBounceBack = SameFrameBounce; Movement->bPendingBounceBack = true;
	const auto ChargeStun = Run(TEXT("Charge stun entry clears horizontal speed"), [=](){ StunStep(); }, 0);
	TestEqual(TEXT("Cancelling bounce preserves its authored identity"), Movement->BounceBackPhysics.Sequence, SameFrameBounce.Sequence);
	Run(TEXT("Charge stun does not erase later natural motion"), [=](){ StunStep(); }, 100);
	Charge = ChargeStun.Definition; Charge.Revision = 12;
	Charge.bControlsBlocked = Charge.Overdrive.bStunned = false;
	TestTrue(TEXT("Publish charge resume"), Movement->QueueMovementParameters(Charge));
	Run(TEXT("Charge resumes after stun"), [=](){ Drive(false, 0); }, 45);
	Charge = Movement->GetMovementEffectStatus().Definition; Charge.Revision = 13; Charge.Overdrive.bActive = false;
	TestTrue(TEXT("Publish charge end"), Movement->QueueMovementParameters(Charge));
	Run(TEXT("Charge recovery uses fast finite braking"), [=](){ Drive(false, 2700); }, 2650);
	const auto ChargeReplay = Run(TEXT("Charge replays after end"), [=](){ Drive(false, 0, Charging.LocalFrame); }, 45);
	TestTrue(TEXT("Historical overdrive survives stun and expiry"), ChargeReplay.Value.Overdrive.IsDriving(ChargeReplay.ServerFrame));
	Run(TEXT("Charge stun entry replays once on its frame"), [=](){ StunStep(ChargeStun.LocalFrame); }, 0);
	Epoch = FGuid::NewGuid(); Movement->ConfigureMovementEffects(Epoch,nullptr,true,7); Publish(1,1,1);
	Run(TEXT("New Avatar uses new epoch"), [=](){ Drive(false,0); },30);
	Run(TEXT("Unknown replay adds no drive"), [=](){ Drive(false,0,0); },0);
	Run(TEXT("Unknown replay does not invent a speed cap"), [=](){ Drive(false,2700,0); },2700);
	TestTrue(TEXT("Actual worker thread exercised"), bWorker);
	Solver->UnregisterAndFreeSimCallbackObject_External(Probe); Step();
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}
#endif
