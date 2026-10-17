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
	auto Run = [&](const TCHAR* Label, TFunction<void()> Work, double ExpectedX)
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
		}
		TestTrue(FString(Label)+TEXT(" callback returned"), bFound); return Status;
	};
	auto Publish = [&](uint32 Revision, float Accel, float Speed)
	{
		FHALMovementSnapshot Value; Value.Epoch = Epoch; Value.Revision = Revision; Value.AccelerationScale = Accel; Value.SpeedScale = Speed;
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
