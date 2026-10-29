#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTLS.h"
#include "PBDRigidsSolver.h"
#include "Chaos/SimCallbackObject.h"
#include "Framework/Threading.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

namespace
{
	struct FImpactProbeInput : Chaos::FSimCallbackInput
	{
		UArcadeVehicleMovementComponent* Movement = nullptr;
		UPrimitiveComponent* Body = nullptr;
		TInstancedStruct<FNetworkPhysicsActionPayload> Action;
		FVector InitialVelocity = FVector::ZeroVector;
		FVector InitialAngularVelocity = FVector::ZeroVector;
		int32 CaseId = 0;
		int32 Steps = 1;
		void Reset() { Movement = nullptr; Body = nullptr; Action.Reset(); CaseId = 0; }
	};
	struct FImpactProbeOutput : Chaos::FSimCallbackOutput
	{
		FVector Velocity = FVector::ZeroVector;
		FVector AngularVelocity = FVector::ZeroVector;
		FVector Travel = FVector::ZeroVector;
		int32 CaseId = 0;
		bool bPhysicsContext = false;
		uint32 ThreadId = 0;
		void Reset() { CaseId = 0; }
	};
	/** Executes the production handler in a real Solver step, then measures after integration. */
	class FImpactProbeCallback : public Chaos::TSimCallbackObject<FImpactProbeInput, FImpactProbeOutput,
		Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::PostSolve>
	{
		int32 LastCase = 0;
		int32 PendingCase = 0;
		bool bPrePhysicsContext = false;
		uint32 PreThreadId = 0;
		FBodyInstanceAsyncPhysicsTickHandle Handle;
		FVector Start;
		int32 StepsLeft=0;
		UArcadeVehicleMovementComponent* BounceMovement=nullptr;
		virtual void OnPreSimulate_Internal() override
		{
			const auto* Input = GetConsumerInput_Internal();
			if (StepsLeft>0 && BounceMovement)
			{ BounceMovement->AsyncPhysicsTickComponent(GetDeltaTime_Internal(),GetSimTime_Internal()); return; }
			if (!Input || Input->CaseId == 0 || Input->CaseId == LastCase || !Input->Body || !Input->Movement) { return; }
			Handle = Input->Body->GetBodyInstance()->GetBodyInstanceAsyncPhysicsTickHandle();
			if (!Handle.IsValid()) { return; }
			LastCase = PendingCase = Input->CaseId;
			bPrePhysicsContext = Chaos::IsInPhysicsThreadContext();
			PreThreadId = FPlatformTLS::GetCurrentThreadId();
			// Deliberately diverge PT state from GT snapshots. Never fake thread context on the GT.
			Handle->SetObjectState(Chaos::EObjectStateType::Dynamic, true);
			Handle->SetV(Input->InitialVelocity);
			Handle->SetW(Input->InitialAngularVelocity);
			Handle->SetLinearImpulse(FVector::ZeroVector, false);
			Handle->SetAngularImpulse(FVector::ZeroVector, false);
			Start=Handle->X(); StepsLeft=Input->Steps;
			Input->Movement->ApplyAction_Internal(Input->Action);
			BounceMovement=Input->Action.GetPtr<FVehicleBounceBackAction>() ? Input->Movement : nullptr;
			if (BounceMovement) { BounceMovement->AsyncPhysicsTickComponent(GetDeltaTime_Internal(),GetSimTime_Internal()); }
		}
		virtual void OnPostSolve_Internal() override
		{
			if (PendingCase == 0 || !Handle.IsValid() || --StepsLeft>0) { return; }
			auto& Output = GetProducerOutputData_Internal();
			Output.CaseId = PendingCase;
			Output.Velocity = Handle->V();
			Output.AngularVelocity = Handle->W();
			Output.Travel=Handle->X()-Start;
			Output.bPhysicsContext = bPrePhysicsContext;
			Output.ThreadId = PreThreadId;
			PendingCase = 0;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALImpactPhysicsThreadTest, "HAL.FutureCreation.Impact.SolverThreadExecutionAndReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALImpactPhysicsThreadTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
		.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
		.ShouldSimulatePhysics(true).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Isolated physics world"), World)) { return false; }
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	AActor* Actor = World->SpawnActor<AActor>();
	UBoxComponent* Body = NewObject<UBoxComponent>(Actor);
	Actor->AddInstanceComponent(Body);
	Actor->SetRootComponent(Body);
	Body->SetBoxExtent(FVector(50));
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	Body->SetSimulatePhysics(true);
	Body->SetEnableGravity(false);
	Body->SetLinearDamping(0);
	Body->SetAngularDamping(0);
	Body->SetWorldLocation(FVector(0, 0, 2000));
	Body->RegisterComponent();
	auto* Movement = NewObject<UArcadeVehicleMovementComponent>(Actor);
	Actor->AddInstanceComponent(Movement);
	Movement->SetUpdatedPrimitive(Body);
	Movement->RegisterComponent();
	Movement->SetAsyncPhysicsTickEnabled(false);
	// Only the explicit action callback runs: driving/ground forces do not obscure its result.
	Movement->bShouldRunPhysics.Store(true);
	auto* Solver = World->GetPhysicsScene()->GetSolver();
	Solver->SetThreadingMode_External(Chaos::EThreadingModeTemp::TaskGraph);
	const auto Step = [Solver]()
	{
		Solver->AdvanceAndDispatch_External(1.0f / 60.0f);
		Solver->WaitOnPendingTasks_External();
	};
	Step(); // Register the body before the first probe.
	auto* Probe = Solver->CreateAndRegisterSimCallbackObject_External<FImpactProbeCallback>();
	int32 CaseId = 0;
	const uint32 GameThreadId = FPlatformTLS::GetCurrentThreadId();
	bool bObservedWorker = false;
	const auto RunCase = [&](const TCHAR* Label, const auto& Action, const FVector& InitialV,
		const FVector& InitialW, const FVector& ExpectedV, const FVector& ExpectedW, int32 Steps=1, FVector Travel=FVector::ZeroVector)
	{
		auto* Input = Probe->GetProducerInputData_External();
		Input->Movement = Movement;
		Input->Body = Body;
		Input->CaseId = ++CaseId;
		using ActionType=std::decay_t<decltype(Action)>;
		Input->Action = TInstancedStruct<FNetworkPhysicsActionPayload>::Make<ActionType>(Action);
		Input->Steps=Steps;
		Movement->BounceBackPhysics={};
		Input->InitialVelocity = InitialV;
		Input->InitialAngularVelocity = InitialW;
		bool bMeasured = false;
		for (int32 Attempt = 0; Attempt < Steps+4 && !bMeasured; ++Attempt)
		{
			Step();
			while (auto Output = Probe->PopFutureOutputData_External())
			{
				if (Output->CaseId != CaseId) { continue; }
				bMeasured = true;
				if (Steps>1) { TestTrue(TEXT("Unobstructed bounce covers configured horizontal distance"),FVector(Output->Travel.X,Output->Travel.Y,0).Equals(Travel,.5)); }
				TestTrue(FString::Printf(TEXT("%s executes in Solver physics context"), Label), Output->bPhysicsContext);
				// TaskGraph can execute some solver work on the frozen GT; require actual worker coverage overall.
				bObservedWorker |= Output->ThreadId != GameThreadId;
				TestTrue(FString::Printf(TEXT("%s integrated V: expected %s, got %s"), Label, *ExpectedV.ToString(), *Output->Velocity.ToString()),
					Output->Velocity.Equals(ExpectedV, 0.1f));
				TestTrue(FString::Printf(TEXT("%s integrated W: expected %s, got %s"), Label, *ExpectedW.ToString(), *Output->AngularVelocity.ToString()),
					Output->AngularVelocity.Equals(ExpectedW, 0.01f));
			}
		}
		TestTrue(FString::Printf(TEXT("%s produced a Solver output"), Label), bMeasured);
	};
	FVehicleImpactAction Action;
	Action.EventSequence = 1;
	Action.DeltaVelocity = FVector(250, 0, 0);
	RunCase(TEXT("Light preserves falling and rotation"), Action, FVector(10, 0, -30), FVector(.2, .3, .7),
		FVector(260, 0, -30), FVector(.2, .3, .7));
	Action.EventSequence = 2;
	Action.DeltaVelocity = FVector(600, 0, 0);
	Action.bMinimumUpwardSpeed = true;
	Action.MinimumUpwardSpeed = 500;
	Action.bSetPlanarAngularVelocity = true;
	Action.PlanarAngularVelocity = FVector(0, 6, 0);
	RunCase(TEXT("Medium reads current PT velocity"), Action, FVector(10, 0, 100), FVector(.2, .3, .7),
		FVector(610, 0, 500), FVector(0, 6, .7));
	Action.EventSequence = 3;
	Action.DeltaVelocity = FVector(900, 0, 0);
	Action.MinimumUpwardSpeed = 750;
	Action.PlanarAngularVelocity = FVector(0, 5.13, 0);
	RunCase(TEXT("Heavy"), Action, FVector(100, 0, -20), FVector(-1, 2, .75), FVector(1000, 0, 750), FVector(0, 5.13, .75));
	RunCase(TEXT("Replay same Heavy from restored state"), Action, FVector(100, 0, -20), FVector(-1, 2, .75),
		FVector(1000, 0, 750), FVector(0, 5.13, .75));
	RunCase(TEXT("Heavy retains existing faster ascent"), Action, FVector(100, 0, 900), FVector(0, 0, .75),
		FVector(1000, 0, 900), FVector(0, 5.13, .75));
	Action.EventSequence = 0;
	RunCase(TEXT("Rejected payload never applies"), Action, FVector(1, 2, 3), FVector(4, 5, 6), FVector(1, 2, 3), FVector(4, 5, 6));
	Movement->bShouldRunPhysics.Store(false);
	Action.EventSequence = 4;
	RunCase(TEXT("Inactive/proxy handler never applies"), Action, FVector(1, 2, 3), FVector(4, 5, 6), FVector(1, 2, 3), FVector(4, 5, 6));
	Movement->bShouldRunPhysics.Store(true);
	FVehicleBounceBackAction Bounce;
	Bounce.Value.Sequence=1; Bounce.Value.bActive=true; Bounce.Value.Direction=FVector(-1,0,0);
	Bounce.Value.Distance=100; Bounce.Value.Duration=.3f;
	const float Speed=100*(2/.3f-(1.f/60)/(.3f*.3f));
	RunCase(TEXT("Bounce cancels high horizontal speed and preserves Z/W"),Bounce,FVector(6000,400,-30),FVector(.2,.3,.7),FVector(-Speed,0,-30),FVector(.2,.3,.7));
	RunCase(TEXT("Bounce replay from restored state"),Bounce,FVector(6000,400,-30),FVector(.2,.3,.7),FVector(-Speed,0,-30),FVector(.2,.3,.7));
	RunCase(TEXT("Bounce distance and stop"),Bounce,FVector(6000,400,-30),FVector::ZeroVector,FVector(0,0,-30),FVector::ZeroVector,20,FVector(-100,0,0));
	TestTrue(TEXT("Solver exercised production handler on an actual worker thread"), bObservedWorker);
	Solver->UnregisterAndFreeSimCallbackObject_External(Probe);
	Step();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
