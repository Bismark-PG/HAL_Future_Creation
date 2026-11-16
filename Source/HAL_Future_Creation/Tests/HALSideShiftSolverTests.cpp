#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "HALSideShiftTestSupport.h"
#include "../VehicleConfigurationTypes.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PBDRigidsSolver.h"
#include "Chaos/SimCallbackObject.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

namespace
{
	struct FShiftSolverInput : Chaos::FSimCallbackInput
	{
		UArcadeVehicleMovementComponent* Movement=nullptr; UPrimitiveComponent* Body=nullptr;
		void Reset() { Movement=nullptr; Body=nullptr; }
	};
	struct FShiftSolverOutput : Chaos::FSimCallbackOutput
	{
		FVector Position=FVector::ZeroVector, Velocity=FVector::ZeroVector, Angular=FVector::ZeroVector;
		int32 Frame=INDEX_NONE; bool Physics=false;
		void Reset() { Frame=INDEX_NONE; }
	};
	class FShiftSolverProbe : public Chaos::TSimCallbackObject<FShiftSolverInput,FShiftSolverOutput,
		Chaos::ESimCallbackOptions::Presimulate|Chaos::ESimCallbackOptions::PostSolve>
	{
		FBodyInstanceAsyncPhysicsTickHandle Handle; int32 Frame=INDEX_NONE;
		UArcadeVehicleMovementComponent* Movement=nullptr; UPrimitiveComponent* Body=nullptr;
		void OnPreSimulate_Internal() override
		{
			const auto* I=GetConsumerInput_Internal(); if(I&&I->Movement&&I->Body) { Movement=I->Movement; Body=I->Body; }
			if(!Movement||!Body) { return; }
			Handle=Body->GetBodyInstance()->GetBodyInstanceAsyncPhysicsTickHandle(); if(!Handle.IsValid()) { return; }
			Movement->AsyncPhysicsTickComponent(GetDeltaTime_Internal(),GetSimTime_Internal()); Frame=Movement->GetLastPhysicsFrame();
		}
		void OnPostSolve_Internal() override
		{
			if(!Handle.IsValid()||Frame<0) { return; }
			auto& O=GetProducerOutputData_Internal(); O.Position=Handle->X(); O.Velocity=Handle->V(); O.Angular=Handle->W(); O.Frame=Frame; O.Physics=Chaos::IsInPhysicsThreadContext();
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSideShiftSolverTest,"HAL.FutureCreation.SideShift.RealGroundDistanceDriftAndWall",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALSideShiftSolverTest::RunTest(const FString&)
{
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
		.ShouldSimulatePhysics(true).EnableTraceCollision(true).SetTransactional(false);
	auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init); GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	auto* Mat=NewObject<UPhysicalMaterial>(); Mat->Friction=0; Mat->Restitution=0;
	const auto Box=[&](FVector At,FVector Extent,bool Sim)
	{
		auto* A=W->SpawnActor<AActor>(); auto* B=NewObject<UBoxComponent>(A); A->AddInstanceComponent(B); A->SetRootComponent(B);
		B->SetMobility(Sim?EComponentMobility::Movable:EComponentMobility::Static); B->SetBoxExtent(Extent); B->SetCollisionProfileName(Sim?TEXT("PhysicsActor"):TEXT("BlockAll"));
		B->SetPhysMaterialOverride(Mat); B->SetSimulatePhysics(Sim); B->SetEnableGravity(Sim); B->SetWorldLocation(At); B->SetLinearDamping(0); B->SetAngularDamping(0);
		B->RegisterComponent(); return B;
	};
	Box(FVector(0,0,-50),FVector(5000,5000,50),false);
	auto* Body=Box(FVector(0,0,50),FVector(50),true);
	auto* M=NewObject<UArcadeVehicleMovementComponent>(Body->GetOwner()); Body->GetOwner()->AddInstanceComponent(M); M->SetUpdatedPrimitive(Body);
	FArcadeVehicleConfig Tuning; Tuning.Drive.CoastingDragRate=0; TestTrue(TEXT("Neutral fixture drive config"),M->ApplyConfiguration(Tuning));
	M->RegisterComponent(); FHALSideShiftTestAccess::SetupMovement(*M);
	auto* Solver=W->GetPhysicsScene()->GetSolver(); Solver->SetThreadingMode_External(Chaos::EThreadingModeTemp::TaskGraph);
	const auto Step=[&]() { Solver->AdvanceAndDispatch_External(1.f/60); Solver->WaitOnPendingTasks_External(); };
	Step(); auto* Probe=Solver->CreateAndRegisterSimCallbackObject_External<FShiftSolverProbe>();
	auto* Input=Probe->GetProducerInputData_External(); Input->Movement=M; Input->Body=Body;
	const FGuid Epoch=FGuid::NewGuid(); M->ConfigureMovementEffects(Epoch,nullptr,true,7); uint32 Revision=0;
	const auto Observe=[&](int32 Steps)
	{
		FShiftSolverOutput Last;
		for(int32 I=0;I<Steps;++I)
		{
			Step(); while(auto Out=Probe->PopFutureOutputData_External())
			{ TestTrue(TEXT("Real Solver context"),Out->Physics); Last.Position=Out->Position; Last.Velocity=Out->Velocity; Last.Angular=Out->Angular; Last.Frame=Out->Frame; }
		}
		return Last;
	};
	const auto Reset=[&](FVector Velocity)
	{
		FHALMovementSnapshot Empty; Empty.Epoch=Epoch; Empty.Revision=++Revision; TestTrue(TEXT("Reset profile queued"),M->QueueMovementParameters(Empty));
		Body->SetWorldLocationAndRotation(FVector(0,0,50),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
		Body->SetPhysicsLinearVelocity(Velocity); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector); Body->WakeAllRigidBodies();
		FVehicleInputCmd Cmd; M->SetInputCommand(Cmd); return Observe(2);
	};
	const auto Publish=[&](float Side)
	{
		FHALMovementSnapshot S; S.Epoch=Epoch; S.Revision=++Revision; S.SideShift.Activation=FGuid::NewGuid(); S.SideShift.bActive=true; S.SideShift.Direction=FVector(0,Side,0);
		TestTrue(TEXT("Production side motion queue accepts profile"),M->QueueMovementParameters(S)); return S;
	};
	for(float Side:{-1.f,1.f})
	{
		const auto Start=Reset(FVector(600,0,0)); Publish(Side); const auto End=Observe(62);
		TestTrue(TEXT("Grounded motion travels configured 3m sideways"),FMath::Abs((End.Position.Y-Start.Position.Y)*Side-300)<8);
		TestTrue(TEXT("Existing forward velocity retained"),FMath::Abs(End.Velocity.X-600)<1);
		TestTrue(TEXT("Translation motor returns to initial lateral speed"),FMath::Abs(End.Velocity.Y)<3);
		AddInfo(FString::Printf(TEXT("Solver side=%g displacement=%.3f forward=%.3f finalLateral=%.3f"),Side,(End.Position.Y-Start.Position.Y)*Side,End.Velocity.X,End.Velocity.Y));
	}
	const auto Standing=Reset(FVector::ZeroVector); Publish(1); const auto StandingEnd=Observe(62);
	TestTrue(TEXT("Standing start also moves 3m"),FMath::Abs(StandingEnd.Position.Y-Standing.Position.Y-300)<8);
	Reset(FVector::ZeroVector); Publish(1); FVehicleInputCmd Drift; Drift.Steering=-1; Drift.bHandbrake=true; M->SetInputCommand(Drift);
	const auto Turn=Observe(50);
	TestTrue(TEXT("Drift turns the chassis while translation remains in original right direction"),Turn.Position.Y>260&&FMath::Abs(Turn.Position.X)<8&&Turn.Angular.Z<-2);
	auto* Wall=Box(FVector(0,200,150),FVector(1000,20,150),false); Step();
	const auto BeforeWall=Reset(FVector::ZeroVector); Publish(1); const auto Blocked=Observe(62);
	TestTrue(TEXT("Real blocking wall prevents authored distance; no teleport through it"),Blocked.Position.Y<150&&Blocked.Position.Y-BeforeWall.Position.Y<200);
	Wall->DestroyComponent();
	Solver->UnregisterAndFreeSimCallbackObject_External(Probe); Step(); W->DestroyWorld(false); GEngine->DestroyWorldContext(W); return true;
}
#endif
