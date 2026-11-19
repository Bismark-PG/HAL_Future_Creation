#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "../ArcadeVehicleMovementComponent.h"
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

struct FHALHookSolverTestAccess
{
	static void Setup(UArcadeVehicleMovementComponent& M)
	{ M.SetAsyncPhysicsTickEnabled(false); M.bShouldRunPhysics.Store(true); M.RecoveryRules.bEnabled=false; }
};
namespace
{
	struct FHookSolverInput : Chaos::FSimCallbackInput
	{ UArcadeVehicleMovementComponent* Movement=nullptr; UPrimitiveComponent* Body=nullptr; void Reset() { Movement=nullptr; Body=nullptr; } };
	struct FHookSolverOutput : Chaos::FSimCallbackOutput
	{
		FVector Position=FVector::ZeroVector,Velocity=FVector::ZeroVector; int32 Frame=INDEX_NONE; bool Physics=false;
		void Reset() { Frame=INDEX_NONE; }
	};
	class FHookSolverProbe : public Chaos::TSimCallbackObject<FHookSolverInput,FHookSolverOutput,Chaos::ESimCallbackOptions::Presimulate|Chaos::ESimCallbackOptions::PostSolve>
	{
		UArcadeVehicleMovementComponent* Movement=nullptr; UPrimitiveComponent* Body=nullptr; FBodyInstanceAsyncPhysicsTickHandle Handle;
		void OnPreSimulate_Internal() override
		{
			if(const auto* I=GetConsumerInput_Internal();I&&I->Movement&&I->Body) { Movement=I->Movement; Body=I->Body; }
			if(!Movement||!Body) { return; } Handle=Body->GetBodyInstance()->GetBodyInstanceAsyncPhysicsTickHandle();
			Movement->AsyncPhysicsTickComponent(GetDeltaTime_Internal(),GetSimTime_Internal());
		}
		void OnPostSolve_Internal() override
		{
			if(!Handle.IsValid()) { return; } auto& O=GetProducerOutputData_Internal();
			// PostSolve precedes the P -> X commit. Read solved P for this frame,
			// otherwise the observation is one integration behind its frame label.
			const auto* Rigid=Body->GetBodyInstance()->GetPhysicsActor()->GetHandle_LowLevel()->CastToRigidParticle();
			O.Position=Rigid?Rigid->GetP():Handle->X(); O.Velocity=Handle->V(); O.Frame=Movement->GetLastPhysicsFrame(); O.Physics=Chaos::IsInPhysicsThreadContext();
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALHookSwingSolverTest,"HAL.FutureCreation.HookThrow.RealArcMomentumInterruptAndWall",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALHookSwingSolverTest::RunTest(const FString&)
{
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
		.ShouldSimulatePhysics(true).EnableTraceCollision(true).SetTransactional(false);
	auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init); GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	auto* Mat=NewObject<UPhysicalMaterial>(); Mat->Friction=0; Mat->Restitution=0;
	const auto Box=[&](FVector At,FVector Size,bool Sim)
	{
		auto* A=W->SpawnActor<AActor>(); auto* B=NewObject<UBoxComponent>(A); A->AddInstanceComponent(B); A->SetRootComponent(B);
		B->SetBoxExtent(Size); B->SetMobility(Sim?EComponentMobility::Movable:EComponentMobility::Static);
		B->SetCollisionProfileName(Sim?TEXT("PhysicsActor"):TEXT("BlockAll")); B->SetPhysMaterialOverride(Mat);
		B->SetSimulatePhysics(Sim); B->SetEnableGravity(Sim); B->SetWorldLocation(At); B->SetLinearDamping(0); B->SetAngularDamping(0); B->BodyInstance.bUseCCD=Sim; B->RegisterComponent(); return B;
	};
	Box(FVector(0,0,-50),FVector(5000,5000,50),false); auto* B=Box(FVector(750,0,50),FVector(50),true);
	auto* M=NewObject<UArcadeVehicleMovementComponent>(B->GetOwner()); B->GetOwner()->AddInstanceComponent(M); M->SetUpdatedPrimitive(B);
	FArcadeVehicleConfig Tuning; Tuning.Drive.CoastingDragRate=0; TestTrue(TEXT("Fixture tuning"),M->ApplyConfiguration(Tuning)); M->RegisterComponent(); FHALHookSolverTestAccess::Setup(*M);
	auto* Solver=W->GetPhysicsScene()->GetSolver(); Solver->SetThreadingMode_External(Chaos::EThreadingModeTemp::TaskGraph);
	const auto Step=[&]() { Solver->AdvanceAndDispatch_External(1.f/60); Solver->WaitOnPendingTasks_External(); };
	Step(); auto* Probe=Solver->CreateAndRegisterSimCallbackObject_External<FHookSolverProbe>(); auto* I=Probe->GetProducerInputData_External(); I->Movement=M; I->Body=B;
	const FGuid Epoch=FGuid::NewGuid(); M->ConfigureMovementEffects(Epoch,nullptr,true,7); uint32 Revision=0; TArray<FHookSolverOutput> Samples;
	const auto Observe=[&](int32 Count)
	{
		FHookSolverOutput Last;
		for(int32 N=0;N<Count;++N) { Step(); while(auto O=Probe->PopFutureOutputData_External()) { TestTrue(TEXT("Real physics thread"),O->Physics); Last.Position=O->Position; Last.Velocity=O->Velocity; Last.Frame=O->Frame; Samples.Add(Last); } }
		return Last;
	};
	const auto Queue=[&](const FHALHookMotion& Hook)
	{ FHALMovementSnapshot S; S.Epoch=Epoch; S.Revision=++Revision; S.bControlsBlocked=true; S.Hook=Hook; TestTrue(TEXT("Normal movement queue"),M->QueueMovementParameters(S)); };
	const auto Reset=[&]()
	{
		Queue({}); B->SetWorldLocationAndRotation(FVector(750,0,50),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
		B->SetPhysicsLinearVelocity(FVector::ZeroVector); B->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector); B->WakeAllRigidBodies(); Observe(2); Samples.Reset();
	};
	const auto Swing=[&](float Side)
	{
		FHALHookMotion H; H.Activation=FGuid::NewGuid(); H.bSwing=H.bCapture=true; H.Anchor=FVector(750,0,50); H.Pivot=FVector(0,0,50);
		H.DurationSeconds=.3f; H.SwingRadians=Side*250.f/750; H.MaximumAcceleration=20000; Queue(H); return H;
	};
	for(float Side:{-1.f,1.f})
	{
		Reset(); const auto H=Swing(Side); auto End=Observe(2);
		const int32 EndFrame=M->GetMovementEffectStatus().Definition.Hook.EndFrame;
		for(int32 N=0;End.Frame<EndFrame-1&&N<30;++N) { End=Observe(1); }
		TestTrue(TEXT("Observe the consumed last swing frame, not a render-step estimate"),EndFrame>0&&End.Frame==EndFrame-1);
		TestTrue(TEXT("Both sides follow a real curved path"),End.Position.Y*Side>220&&End.Position.X<725&&FMath::Abs(End.Position.Size2D()-750)<8);
		TestTrue(TEXT("Swing builds nonzero exit speed"),End.Velocity.Size2D()>1500);
		const FVector Tangent(-FMath::Sin(H.SwingRadians)*Side,FMath::Cos(H.SwingRadians)*Side,0);
		TestTrue(TEXT("Velocity points along the arc tangent"),FVector::DotProduct(End.Velocity.GetSafeNormal2D(),Tangent)>.98);
		FHALHookMotion Release; Release.Activation=H.Activation; Release.bThrow=Release.bPreserveHorizontal=true; Release.DurationSeconds=.1f; Release.ThrowVelocity=FVector(0,0,250); Queue(Release);
		const auto After=Observe(3);
		TestTrue(TEXT("Detach does not brake or replace horizontal velocity"),FVector::Dist2D(End.Velocity,After.Velocity)<1);
		TestTrue(TEXT("Minimum upward launch remains physical"),After.Velocity.Z>180);
		AddInfo(FString::Printf(TEXT("Hook solver side=%g endpoint=%s exit=%s afterDetach=%s"),Side,*End.Position.ToString(),*End.Velocity.ToString(),*After.Velocity.ToString()));
	}
	Reset(); Swing(-1); const auto Interrupted=Observe(9); Queue({}); const auto Continued=Observe(3);
	TestTrue(TEXT("Cancelling the swing preserves its current momentum"),Interrupted.Velocity.Size2D()>400&&FVector::Dist2D(Interrupted.Velocity,Continued.Velocity)<1);
	auto* Wall=Box(FVector(750,-170,150),FVector(500,20,150),false); Step(); Reset(); Swing(-1); const auto Blocked=Observe(22);
	TestTrue(TEXT("CCD wall blocks the swing; no authored pose teleport"),Blocked.Position.Y>-110);
	AddInfo(FString::Printf(TEXT("Hook wall endpoint=%s velocity=%s"),*Blocked.Position.ToString(),*Blocked.Velocity.ToString()));
	Wall->DestroyComponent(); Solver->UnregisterAndFreeSimCallbackObject_External(Probe); Step(); W->DestroyWorld(false); GEngine->DestroyWorldContext(W); return true;
}
#endif
