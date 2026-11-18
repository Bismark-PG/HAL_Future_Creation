#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTLS.h"
#include "HAL/IConsoleManager.h"
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
		bool bObserveOnly = false;
		void Reset() { Movement = nullptr; Body = nullptr; Action.Reset(); CaseId = 0; bObserveOnly = false; }
	};
	struct FImpactProbeOutput : Chaos::FSimCallbackOutput
	{
		FVector Velocity = FVector::ZeroVector;
		FVector AngularVelocity = FVector::ZeroVector;
		FVector Travel = FVector::ZeroVector;
		FVector Position = FVector::ZeroVector;
		int32 CaseId = 0;
		bool bPhysicsContext = false;
		bool bSleepingBefore = false, bSleepingAfter = false;
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
		bool bSleepingBefore = false;
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
			if (!Input->bObserveOnly)
			{
				// Deliberately diverge PT state from GT snapshots. Never fake thread context on the GT.
				Handle->SetObjectState(Chaos::EObjectStateType::Dynamic, true);
				Handle->SetV(Input->InitialVelocity);
				Handle->SetW(Input->InitialAngularVelocity);
				Handle->SetLinearImpulse(FVector::ZeroVector, false);
				Handle->SetAngularImpulse(FVector::ZeroVector, false);
			}
			bSleepingBefore=Handle->ObjectState()==Chaos::EObjectStateType::Sleeping;
			Start=Handle->X(); StepsLeft=Input->Steps;
			if (!Input->bObserveOnly) { Input->Movement->ApplyAction_Internal(Input->Action); }
			BounceMovement=Input->bObserveOnly || Input->Action.GetPtr<FVehicleBounceBackAction>() ? Input->Movement : nullptr;
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
			Output.Position=Handle->X();
			Output.bPhysicsContext = bPrePhysicsContext;
			Output.bSleepingBefore = bSleepingBefore;
			Output.bSleepingAfter = Handle->ObjectState()==Chaos::EObjectStateType::Sleeping;
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
		const FVector& InitialW, const FVector& ExpectedV, const FVector& ExpectedW, int32 Steps=1, FVector Travel=FVector::ZeroVector, bool bObserveOnly=false)
	{
		auto* Input = Probe->GetProducerInputData_External();
		Input->Movement = Movement;
		Input->Body = Body;
		Input->CaseId = ++CaseId;
		using ActionType=std::decay_t<decltype(Action)>;
		Input->Action = TInstancedStruct<FNetworkPhysicsActionPayload>::Make<ActionType>(Action);
		Input->Steps=Steps;
		Input->bObserveOnly=bObserveOnly;
		Movement->BounceBackPhysics={};
		Input->InitialVelocity = InitialV;
		Input->InitialAngularVelocity = InitialW;
		bool bMeasured = false;
		FVector MeasuredTravel = FVector::ZeroVector;
		for (int32 Attempt = 0; Attempt < Steps+4 && !bMeasured; ++Attempt)
		{
			Step();
			while (auto Output = Probe->PopFutureOutputData_External())
			{
				if (Output->CaseId != CaseId) { continue; }
				bMeasured = true;
				MeasuredTravel = Output->Travel;
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
		return MeasuredTravel;
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
	Bounce.PredictionToken=1;
	Movement->ResolvedBouncePredictionToken.Store(1);
	RunCase(TEXT("Confirmed prediction replays before the later authority action"),Bounce,FVector(6000,400,-30),FVector::ZeroVector,
		FVector(-Speed,0,-30),FVector::ZeroVector);
	Movement->RejectedBouncePredictionTokens.Add(1);
	RunCase(TEXT("Rejected prediction stays retired after restoring old bounce state"),Bounce,FVector(6000,400,-30),FVector::ZeroVector,
		FVector(6000,400,-30),FVector::ZeroVector);
	Movement->RejectedBouncePredictionTokens.Add(2);
	RunCase(TEXT("Rejected older prediction cannot resurrect on later replay"),Bounce,FVector(100,0,0),FVector::ZeroVector,
		FVector(100,0,0),FVector::ZeroVector);
	Bounce.PredictionToken=0;
	RunCase(TEXT("Authority action still applies after speculation retires"),Bounce,FVector(6000,400,-30),FVector::ZeroVector,
		FVector(-Speed,0,-30),FVector::ZeroVector);
	// Exercise the actual non-History GT queue path. The worker only observes/ticks
	// afterward; it does not inject the desired state or apply the action a second time.
	auto* LogCVar=IConsoleManager::Get().FindConsoleVariable(TEXT("hal.ImpactPhysicsLog"));
	if (TestNotNull(TEXT("Impact diagnostic switch exists"),LogCVar))
	{
		const int32 OldLog=LogCVar->GetInt(); const FGuid AttackId=FGuid::NewGuid();
		Action={}; Action.EventSequence=100; Action.DeltaVelocity=FVector(900,0,0);
		Action.bMinimumUpwardSpeed=true; Action.MinimumUpwardSpeed=750;
		Action.bSetPlanarAngularVelocity=true; Action.PlanarAngularVelocity=FVector(0,5.13,0);
		const auto Direct=[&](const TCHAR* Label,int32 Log,int32 Steps)
		{
			LogCVar->Set(Log,ECVF_SetByCode);
			Body->SetPhysicsLinearVelocity(FVector::ZeroVector); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
			Step(); // Push the reset before submission; no fake PT-state injection.
			TestTrue(TEXT("Real non-History impact submission succeeds"),Movement->QueueImpact(Action,AttackId,7));
			if (Log)
			{
				TestEqual(TEXT("One bounded trace registered"),Movement->ImpactDiagnosticTraces.Num(),1);
				if (!Movement->ImpactDiagnosticTraces.IsEmpty())
				{
					const auto& Trace=Movement->ImpactDiagnosticTraces[0];
					TestTrue(TEXT("Combat identity reaches observer"),Trace.AttackId==AttackId&&Trace.PulseSequence==7&&Trace.EventSequence==Action.EventSequence);
				}
			}
			// The first observed frame still contains the PT-consumed pending impulse;
			// the following Steps-1 frames integrate the resulting constant velocity.
			return RunCase(Label,Action,FVector::ZeroVector,FVector::ZeroVector,FVector(900,0,750),FVector(0,5.13,0),Steps,FVector(900*(Steps-1)/60.f,0,0),true);
		};
		const FVector TravelOff=Direct(TEXT("Direct impulse with logging off"),0,4);
		TestTrue(TEXT("Disabled logging retains no observer state"),Movement->ImpactDiagnosticTraces.IsEmpty()&&!Movement->bHasImpactDiagnosticTraces.Load());
		const FVector TravelOn=Direct(TEXT("Direct impulse with logging on preserves V/W and travel"),1,4);
		TestTrue(TEXT("Enabling diagnostic observer preserves actual three-dimensional travel"),TravelOn.Equals(TravelOff,.1));
		if (TestEqual(TEXT("Short observation is still active"),Movement->ImpactDiagnosticTraces.Num(),1))
		{
			const auto& Trace=Movement->ImpactDiagnosticTraces[0];
			TestTrue(TEXT("Four actual physics frames were sampled"),Trace.FirstFrame>=0&&Trace.LastFrame==Trace.FirstFrame+3&&Trace.NextSample==4);
		}
		LogCVar->Set(0,ECVF_SetByCode);
		RunCase(TEXT("Disabling logs leaves motion untouched"),Action,FVector::ZeroVector,FVector::ZeroVector,FVector(900,0,750),FVector(0,5.13,0),1,FVector::ZeroVector,true);
		TestTrue(TEXT("Disabling clears a pending observer"),Movement->ImpactDiagnosticTraces.IsEmpty()&&!Movement->bHasImpactDiagnosticTraces.Load());
		++Action.EventSequence;
		Direct(TEXT("Bounded long observation leaves motion untouched"),1,96);
		TestTrue(TEXT("Observer retires after 96 physics frames"),Movement->ImpactDiagnosticTraces.IsEmpty()&&!Movement->bHasImpactDiagnosticTraces.Load());
		LogCVar->Set(OldLog,ECVF_SetByCode);
	}
	TestTrue(TEXT("Solver exercised production handler on an actual worker thread"), bObservedWorker);
	Solver->UnregisterAndFreeSimCallbackObject_External(Probe);
	Step();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALNonHistoryImpactWakeTest, "HAL.FutureCreation.Impact.NonHistoryGroundedSleepWakeAndBatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALNonHistoryImpactWakeTest::RunTest(const FString& Parameters)
{
	const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
		.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
		.ShouldSimulatePhysics(true).EnableTraceCollision(true).SetTransactional(false);
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
	if(!TestNotNull(TEXT("Grounded wake world"),World)) { return false; }
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* FloorActor=World->SpawnActor<AActor>(); auto* Floor=NewObject<UBoxComponent>(FloorActor);
	FloorActor->AddInstanceComponent(Floor); FloorActor->SetRootComponent(Floor);
	Floor->SetMobility(EComponentMobility::Static); Floor->SetBoxExtent(FVector(5000,5000,50));
	Floor->SetCollisionProfileName(TEXT("BlockAll"));
	// Isolate impact delivery from continuous arcade grip/steering. Zero force/torque
	// submissions also wake Chaos bodies; physical floor contacts/gravity remain real.
	Floor->SetCollisionResponseToChannel(ECC_Visibility,ECR_Ignore);
	Floor->SetWorldLocation(FVector(0,0,-50)); Floor->RegisterComponent();
	auto* Actor=World->SpawnActor<AActor>(); auto* Body=NewObject<UBoxComponent>(Actor);
	Actor->AddInstanceComponent(Body); Actor->SetRootComponent(Body); Body->SetBoxExtent(FVector(50));
	Body->SetCollisionProfileName(TEXT("PhysicsActor")); Body->SetSimulatePhysics(true);
	Body->SetEnableGravity(true); Body->SetLinearDamping(0); Body->SetAngularDamping(0);
	Body->SetWorldLocation(FVector(0,0,55)); Body->RegisterComponent();
	auto* Movement=NewObject<UArcadeVehicleMovementComponent>(Actor); Actor->AddInstanceComponent(Movement);
	Movement->SetUpdatedPrimitive(Body); Movement->RegisterComponent(); Movement->SetAsyncPhysicsTickEnabled(false);
	Movement->bShouldRunPhysics.Store(true);
	auto* Solver=World->GetPhysicsScene()->GetSolver(); Solver->SetThreadingMode_External(Chaos::EThreadingModeTemp::TaskGraph);
	const auto Step=[&]() { Solver->AdvanceAndDispatch_External(1.f/60); Solver->WaitOnPendingTasks_External(); };
	for(int32 I=0;I<120;++I) { Step(); }
	auto* Probe=Solver->CreateAndRegisterSimCallbackObject_External<FImpactProbeCallback>(); int32 CaseId=0;
	const auto Observe=[&](int32 Steps)
	{
		auto* In=Probe->GetProducerInputData_External(); In->Movement=Movement; In->Body=Body;
		In->CaseId=++CaseId; In->Steps=Steps; In->bObserveOnly=true;
		FImpactProbeOutput Measured; bool Found=false;
		for(int32 I=0;I<Steps+4&&!Found;++I)
		{
			Step(); while(auto Out=Probe->PopFutureOutputData_External()) if(Out->CaseId==CaseId)
			{
				Found=true; Measured.Velocity=Out->Velocity; Measured.AngularVelocity=Out->AngularVelocity;
				Measured.Travel=Out->Travel; Measured.Position=Out->Position; Measured.bSleepingBefore=Out->bSleepingBefore; Measured.bSleepingAfter=Out->bSleepingAfter;
				TestTrue(TEXT("Wake consumer runs in a real Solver physics context"),Out->bPhysicsContext);
			}
		}
		TestTrue(TEXT("Wake probe produced integrated state"),Found); return Measured;
	};
	const auto Reset=[&](bool Sleeping,bool Gravity,const FVector& V=FVector::ZeroVector,const FVector& W=FVector::ZeroVector)
	{
		Body->SetEnableGravity(Gravity);
		Body->SetWorldLocationAndRotation(FVector(0,0,Gravity?50:2000),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
		Body->SetPhysicsLinearVelocity(V); Body->SetPhysicsAngularVelocityInRadians(W);
		Step(); // Consume teleport/shape-contact updates before arranging rest.
		if(Gravity) { for(int32 I=0;I<120;++I) { Step(); } }
		if(Sleeping) { Body->PutAllRigidBodiesToSleep(); } else { Body->WakeAllRigidBodies(); }
		Step();
	};
	FVehicleImpactAction Heavy; Heavy.EventSequence=1; Heavy.DeltaVelocity=FVector(900,0,0);
	Heavy.bMinimumUpwardSpeed=true; Heavy.MinimumUpwardSpeed=750;
	Heavy.bSetPlanarAngularVelocity=true; Heavy.PlanarAngularVelocity=FVector(0,5.13,0);
	// Gravity/contact is real. Each reset passes through normal GT commands; the probe
	// never injects expected velocities, wakes, or applies a second copy of the action.
	int32 SleepingTrials=0,AwakeTrials=0;
	for(int32 I=0;I<12;++I)
	{
		const bool Sleeping=I%2==0; Reset(Sleeping,true);
		const auto Rest=Observe(1); TestEqual(FString::Printf(TEXT("Trial %d pre-hit PT sleep (V=%s)"),I,*Rest.Velocity.ToString()),Rest.bSleepingAfter,Sleeping);
		TestTrue(TEXT("Real gravity and floor support the reset target at Z=50"),FMath::Abs(Rest.Travel.Z)<1&&FMath::Abs(Rest.Position.Z-50)<1);
		++Heavy.EventSequence; TestTrue(TEXT("Grounded heavy is accepted"),Movement->QueueImpact(Heavy));
		const auto Hit=Observe(12);
		TestEqual(TEXT("Consumed from actual sleeping/awake body"),Hit.bSleepingBefore,Sleeping);
		TestFalse(TEXT("Heavy wakes and remains moving"),Hit.bSleepingAfter);
		TestTrue(TEXT("Grounded target actually rises and moves horizontally"),Hit.Travel.Z>70&&Hit.Travel.X>90&&Hit.Velocity.Z>400);
		TestTrue(TEXT("Accepted impact is consumed once"),Movement->PendingImpacts.IsEmpty()&&!Movement->bHasPendingImpacts.Load());
		if(Sleeping) { ++SleepingTrials; } else { ++AwakeTrials; }
	}
	Reset(true,true); Observe(1);
	FVehicleImpactAction NoOp; NoOp.EventSequence=200;
	TestTrue(TEXT("Suppressed zero-delta contact can be accepted"),Movement->QueueImpact(NoOp));
	const auto Suppressed=Observe(4);
	TestTrue(TEXT("Zero-delta support contact does not wake rested body"),Suppressed.bSleepingBefore&&Suppressed.bSleepingAfter&&Suppressed.Travel.Size()<.1);
	Reset(false,false,FVector(0,0,-20),FVector(1,2,.75));
	FVehicleImpactAction First=Heavy; First.EventSequence=201; First.DeltaVelocity=FVector(300,0,0); First.MinimumUpwardSpeed=500; First.PlanarAngularVelocity=FVector(0,6,0);
	Heavy.EventSequence=202; Heavy.DeltaVelocity=FVector(600,0,0);
	TestTrue(TEXT("First same-step action queued"),Movement->QueueImpact(First));
	TestTrue(TEXT("Second same-step action queued"),Movement->QueueImpact(Heavy));
	const auto Batch=Observe(4);
	TestTrue(TEXT("Batch adds horizontal impulses, uses highest ascent floor, and latest planar angular target"),
		Batch.Velocity.Equals(FVector(900,0,750),.1)&&Batch.AngularVelocity.Equals(FVector(0,5.13,.75),.01));
	const auto Later=Observe(4);
	TestTrue(TEXT("Queue never reapplies accepted actions on following steps"),Later.Velocity.Equals(Batch.Velocity,.1)&&Later.AngularVelocity.Equals(Batch.AngularVelocity,.01));
	Reset(false,false,FVector(0,0,900)); TestTrue(TEXT("Faster ascent impact accepted"),Movement->QueueImpact(Heavy));
	TestTrue(TEXT("Non-History hit retains faster existing ascent"),Observe(2).Velocity.Z>899);
	Reset(true,false); TestTrue(TEXT("Queued before deactivation"),Movement->QueueImpact(Heavy));
	Movement->Deactivate(); TestFalse(TEXT("No consumer means no false accepted impact"),Movement->QueueImpact(Heavy));
	Movement->bShouldRunPhysics.Store(true);
	const auto Retired=Observe(3); TestTrue(TEXT("Deactivation retires pending actions"),Retired.bSleepingAfter&&Retired.Velocity.IsNearlyZero());
	AddInfo(FString::Printf(TEXT("Actual grounded Chaos trials: sleeping=%d awake=%d; batch, ascent floor, zero-contact sleep, once-only and retirement checked"),SleepingTrials,AwakeTrials));
	Solver->UnregisterAndFreeSimCallbackObject_External(Probe); Step();
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return true;
}

#endif
