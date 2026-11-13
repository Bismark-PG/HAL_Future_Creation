#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../VehicleConfigurationTypes.h"
#include "../HALMovementEffectBridge.h"
#include "../HALMovementAreaReceiver.h"
#include "../HALGameplayTags.h"
#include "../HALPlayerState.h"
#include "../HALAbilitySystemComponent.h"
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
			Handle->SetX(FVector(0,0,2000));
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
	FString MeasuredSamples;
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
			MeasuredSamples += FString::Printf(TEXT("%s\t%.9f\t%.9f\t%.9f\n"),Label,Output->Velocity.X,Output->Velocity.Y,Output->Velocity.Z);
			if (!FMath::IsNearlyEqual(Output->Velocity.X,ExpectedX,.02) || !FMath::IsNearlyEqual(Output->Velocity.Y,ExpectedY,.02))
			{ AddInfo(FString::Printf(TEXT("%s actual=%s expectedXY=(%.4f,%.4f) gravity=%s"),Label,*Output->Velocity.ToString(),ExpectedX,ExpectedY,*Status.Value.GravityDrive.Source.ToString())); }
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
	auto Drive = [Movement,Body,Solver](bool bReverse, float Initial, int32 ReplayFrame = INDEX_NONE, float Steering = 0)
	{
		auto Handle = Body->GetBodyInstance()->GetBodyInstanceAsyncPhysicsTickHandle(); Handle->SetV(FVector(Initial,0,0));
		Movement->PrepareMovementFrame(Solver, ReplayFrame == INDEX_NONE ? Solver->GetCurrentFrame() : ReplayFrame, ReplayFrame != INDEX_NONE);
		Movement->InputCommand.Reset(); if (bReverse) { Movement->InputCommand.Brake = 1; } else { Movement->InputCommand.Throttle = 1; }
		Movement->InputCommand.Steering = Steering;
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
	FHALMovementSnapshot Gravity; Gravity.Epoch = Epoch; Gravity.Revision = 2;
	Gravity.GravityDrive.Source = FGuid::NewGuid(); Gravity.GravityDrive.Center = FVector(0,1000,2000);
	Gravity.GravityDrive.Radius = 5000; Gravity.GravityDrive.HalfHeight = 200;
	Gravity.GravityDrive.Strength = 1; Gravity.GravityDrive.SteeringResistance = 1; Gravity.GravityDrive.RampSeconds = 0;
	TestTrue(TEXT("Gravity uses the normal authority parameter channel"), Movement->QueueMovementParameters(Gravity));
	const auto Bent = Run(TEXT("Gravity redirects actual forward propulsion"),[=](){ Drive(false,0); },0,30);
	Run(TEXT("Sideways entry momentum is retained while thrust bends"),[=](){ Drive(false,1000); },1000,30);
	Run(TEXT("Bent forward thrust is not mistaken for reverse driving"),[=](){ Drive(false,-1000); },-1000,30);
	Run(TEXT("Full steering resists gravity bending"),[=](){ Drive(false,0,INDEX_NONE,1); },30,0);
	Run(TEXT("Gravity preserves reverse propulsion"),[=](){ Drive(true,0); },-20,0);
	Run(TEXT("Gravity preserves braking direction"),[=](){ Drive(true,100); },100-Config.Drive.BrakeDeceleration/60,0);
	Publish(3,1,1); Run(TEXT("Gravity removal restores forward propulsion"),[=](){ Drive(false,0); },30,0);
	const auto GravityReplay = Run(TEXT("Old gravity force survives removal during replay"),[=](){ Drive(false,0,Bent.LocalFrame); },0,30);
	TestTrue(TEXT("Replay retains the exact gravity source"),GravityReplay.Value.GravityDrive.Source == Gravity.GravityDrive.Source);
	Run(TEXT("Unknown replay adds no drive"), [=](){ Drive(false,0,0); },0);
	Run(TEXT("Unknown replay does not invent a speed cap"), [=](){ Drive(false,2700,0); },2700);
	// Exercise source selection and cleanup through the real GT -> authority -> solver adapter.
	auto* State = World->SpawnActor<AHALPlayerState>(); auto* ASC = State->GetHALAbilitySystemComponent();
	auto* Bridge = NewObject<UHALMovementEffectBridge>(Actor); Actor->AddInstanceComponent(Bridge); Bridge->RegisterComponent();
	ASC->InitAbilityActorInfo(State,Actor); Bridge->Bind(ASC,7);
	auto* Receiver = NewObject<UHALMovementAreaReceiver>(Actor); Actor->AddInstanceComponent(Receiver); Receiver->RegisterComponent(); Receiver->Bind(ASC);
	auto* SourceA = World->SpawnActor<AActor>(); auto* SourceB = World->SpawnActor<AActor>();
	for (auto* Source : {SourceA,SourceB}) { Source->PreInitializeComponents(); Source->InitializeComponents(); Source->PostInitializeComponents(); Source->DispatchBeginPlay(); }
	auto FieldA = Gravity.GravityDrive, FieldB = FieldA; FieldB.Source = FGuid::NewGuid();
	FieldB.Strength = .5f; FieldB.Center = FVector(0,-1000,2000);
	const auto Flush = [&]() { FWorldDelegates::OnWorldPostActorTick.Broadcast(World,LEVELTICK_All,1.f/60); };
	Bridge->SetDragDrive(FieldA); Flush();
	Run(TEXT("Flow payload without Drag never bends propulsion"),[=](){ Drive(false,0); },30,0);
	FHALMovementAreaConfig DragA; DragA.Kind = EHALMovementAreaKind::Drag; DragA.RecoverySeconds = 0;
	Receiver->RegisterArea(SourceA,DragA); Flush();
	Run(TEXT("Grip-only Drag has no invented flow direction"),[=](){ Drive(false,0); },30,0);
	DragA.DragDrive = FieldA; auto DragB = DragA; DragB.DragDrive = FieldB;
	Receiver->RegisterArea(SourceA,DragA); Receiver->RegisterArea(SourceB,DragB); Flush();
	Run(TEXT("Overlapping fields select strongest bend"),[=](){ Drive(false,0); },0,30);
	Receiver->UnregisterArea(SourceA); Flush();
	Run(TEXT("Removing one field preserves the remaining source"),[=](){ Drive(false,0); },30*UE_SQRT_2/2,-30*UE_SQRT_2/2);
	SourceB->Destroy(); Flush();
	Run(TEXT("Destroyed field cannot leave stale propulsion bending"),[=](){ Drive(false,0); },30,0);
	Receiver->RegisterArea(SourceA,DragA); Flush();
	const auto DragFrame = Run(TEXT("Reenter Drag restores its flow"),[=](){ Drive(false,0); },0,30);
	TestEqual(TEXT("Remove actual Drag GE while still within its source"),ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(HALGameplayTags::State_Movement_Drag)),1);
	Flush(); Run(TEXT("Removing Drag state stops bending despite retained source geometry"),[=](){ Drive(false,0); },30,0);
	Run(TEXT("Past Drag frame still bends when replayed after removal"),[=](){ Drive(false,0,DragFrame.LocalFrame); },0,30);
	Receiver->UnregisterArea(SourceA); DragA.LateralGripMultiplier = 1; Receiver->RegisterArea(SourceA,DragA); Flush();
	Run(TEXT("Disabling Drag grip also disables propulsion bending"),[=](){ Drive(false,0); },30,0);
	Receiver->ClearForOwnerExit(); Receiver->Bind(ASC);
	FHALMovementAreaConfig Oil; Oil.Kind = EHALMovementAreaKind::Slippery;
	Receiver->RegisterArea(SourceA,Oil); Bridge->SetDragDrive(FieldA); Flush();
	TestTrue(TEXT("Slippery fixture grants only Slippery state"),ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_SlipperyArea)
		&& !ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Drag));
	Run(TEXT("Slippery state never enables Drag propulsion"),[=](){ Drive(false,0); },30,0);
	Receiver->ClearForOwnerExit();
	Bridge->Unbind();
	Epoch = FGuid::NewGuid(); Movement->ConfigureMovementEffects(Epoch,nullptr,true,7); Publish(1,1,1);
	const auto OrdinaryDrive = [=](float Throttle, float Brake, const FVector& Velocity, const FVector& Forward)
	{
		auto Handle = Body->GetBodyInstance()->GetBodyInstanceAsyncPhysicsTickHandle(); Handle->SetV(Velocity);
		Movement->PrepareMovementFrame(Solver,Solver->GetCurrentFrame(),false);
		Movement->InputCommand.Reset(); Movement->InputCommand.Throttle = Throttle; Movement->InputCommand.Brake = Brake;
		Movement->CoastingDragRate = .35f;
		Movement->ApplyLongitudinalForces(*Body->GetBodyInstance(),Forward,FVector::DotProduct(Velocity,Forward));
		Movement->CoastingDragRate = 0;
	};
	Run(TEXT("No Drag ordinary coast preserves lateral momentum"),[=](){ OrdinaryDrive(0,0,FVector(600,180,0),FVector::ForwardVector); },596.5,180);
	Run(TEXT("No Drag partial throttle and base drag"),[=](){ OrdinaryDrive(.5f,0,FVector(600,300,0),FVector::ForwardVector); },611.5,300);
	Run(TEXT("No Drag brake wins simultaneous throttle"),[=](){ OrdinaryDrive(1,.6f,FVector(600,0,0),FVector::ForwardVector); },600-(Config.Drive.BrakeDeceleration*.6+210)/60,0);
	Run(TEXT("No Drag reverse with base drag"),[=](){ OrdinaryDrive(0,1,FVector(-600,0,0),FVector::ForwardVector); },-600+(-Config.Drive.ReverseAcceleration+210)/60,0);
	Run(TEXT("No Drag inclined ground drive"),[=](){ OrdinaryDrive(.5f,0,FVector(480,200,360),FVector(.8,0,.6)); },489.2,200);
	if (FParse::Param(FCommandLine::Get(),TEXT("HALDragInterfaceSamples")))
	{
		const TCHAR* Name = FParse::Param(FCommandLine::Get(),TEXT("HALDragInterfaceBaseline")) ? TEXT("Before.tsv") : TEXT("After.tsv");
		TestTrue(TEXT("Write opt-in measured physics comparison"),FFileHelper::SaveStringToFile(MeasuredSamples,*(FPaths::ProjectSavedDir()/TEXT("BlackHole/20261113/DragInterface")/Name)));
	}
	TestTrue(TEXT("Actual worker thread exercised"), bWorker);
	Solver->UnregisterAndFreeSimCallbackObject_External(Probe); Step();
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}
#endif
