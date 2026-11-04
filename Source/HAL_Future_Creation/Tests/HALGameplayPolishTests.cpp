#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALVehicleStabilityTestRules.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PBDRigidsSolver.h"
#include "Chaos/SimCallbackObject.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALWallDropPolicyTest,"HAL.FutureCreation.GameplayPolish.WallDropAndGravityValidation",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALWallDropPolicyTest::RunTest(const FString&)
{
	FVehicleContactStabilityRules R; R.WallDropNormalDeltaSpeed=600;
	TestFalse(TEXT("Ordinary wall below independent threshold retains ball"),R.AllowsWallDrop(true,FVector::ForwardVector,599));
	TestTrue(TEXT("Exact wall threshold qualifies"),R.AllowsWallDrop(true,FVector::ForwardVector,600));
	TestTrue(TEXT("Floor keeps existing drop rule"),R.AllowsWallDrop(true,FVector::UpVector,100));
	TestTrue(TEXT("Ceiling is not a wall"),R.AllowsWallDrop(true,-FVector::UpVector,100));
	TestTrue(TEXT("Ramp keeps existing drop rule"),R.AllowsWallDrop(true,FVector(1,0,1).GetSafeNormal(),100));
	TestTrue(TEXT("Vehicle/ball/moving/specialized source keeps existing rule"),R.AllowsWallDrop(false,FVector::ForwardVector,100));
	TestFalse(TEXT("Unknown normal is not classified as wall"),R.IsOrdinaryWall(true,FVector::ZeroVector));
	R.MaximumWallUpDot=FMath::Cos(FMath::DegreesToRadians(90.f));
	TestTrue(TEXT("90 degree wall setting tolerates cosine roundoff"),R.IsOrdinaryWall(true,FVector::ForwardVector));
	auto* C=MakeHALStabilityTestRules(); FString Error;
	C->GravityScale=0; TestFalse(TEXT("Zero gravity scale rejected"),C->Validate(Error));
	C->GravityScale=1.5; TestTrue(TEXT("Vehicle multiplier valid"),C->Validate(Error));
	C->WallDropNormalDeltaSpeed=-1; TestFalse(TEXT("Negative wall threshold rejected"),C->Validate(Error));
	C->WallDropNormalDeltaSpeed=600; C->GravityScale=std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN gravity rejected"),C->Validate(Error));
	TestEqual(TEXT("Default gravity predicts one turn over two seconds"),HALVehicleStability::FlipAngularSpeed(980,1,-980,100),PI,0.0001f);
	TestEqual(TEXT("Scaled gravity scales ballistic flip rate"),HALVehicleStability::FlipAngularSpeed(980,1,-1470,100),1.5f*PI,0.0001f);
	TestEqual(TEXT("Configured angular cap remains binding"),HALVehicleStability::FlipAngularSpeed(980,1,-1470,2),2.f);
	TestEqual(TEXT("Zero world gravity does not divide by zero"),HALVehicleStability::FlipAngularSpeed(980,1,0,100),0.f);
	return true;
}
namespace
{
	struct FGravityInput:Chaos::FSimCallbackInput
	{
		TArray<FPhysicsActorHandle> Bodies;
		void Reset() { Bodies.Reset(); }
	};
	struct FGravityOutput:Chaos::FSimCallbackOutput
	{
		TArray<FVector> Velocities;
		void Reset() { Velocities.Reset(); }
	};
	class FGravityProbe:public Chaos::TSimCallbackObject<FGravityInput,FGravityOutput,Chaos::ESimCallbackOptions::Presimulate|Chaos::ESimCallbackOptions::PostSolve>
	{
		TArray<FPhysicsActorHandle> Bodies;
		virtual void OnPreSimulate_Internal() override
		{
			if (const auto* I=GetConsumerInput_Internal(); I && I->Bodies.Num()) { Bodies=I->Bodies; }
		}
		virtual void OnPostSolve_Internal() override
		{
			auto& O=GetProducerOutputData_Internal();
			for (auto* B:Bodies) { if (auto* P=B->GetPhysicsThreadAPI()) { O.Velocities.Add(P->V()); } }
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALVehicleGravitySolverTest,"HAL.FutureCreation.GameplayPolish.NativeSolverVehicleGravity",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALVehicleGravitySolverTest::RunTest(const FString&)
{
	FScopedHALStabilityTestRules Scope(true);
	auto* Definition=Scope.Settings->SharedRules.Get(); Definition->GravityScale=1.5;
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(true).SetTransactional(false);
	auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	auto* Shared=W->GetSubsystem<UVehicleStabilitySubsystem>();
	TestTrue(TEXT("Immutable world gravity snapshot initializes"),Shared->InitializeRules());
	const float Gravity=W->GetGravityZ();
	TestEqual(TEXT("Effective gravity respects world gravity override"),Shared->GetVehicleGravityZ(),Gravity*1.5f);
	Definition->GravityScale=3; TestEqual(TEXT("Editing source DA cannot change live history"),Shared->GetVehicleGravityZ(),Gravity*1.5f);
	TArray<UBoxComponent*> Bodies;
	for (int32 I=0;I<3;++I)
	{
		auto* A=W->SpawnActor<AActor>(); auto* B=NewObject<UBoxComponent>(A); A->AddInstanceComponent(B); A->SetRootComponent(B);
		B->SetBoxExtent(FVector(20)); B->SetWorldLocation(FVector(I*1000,0,10000));
		B->SetCollisionProfileName(TEXT("PhysicsActor")); B->SetSimulatePhysics(true); B->SetEnableGravity(true);
		B->SetLinearDamping(0); B->SetAngularDamping(0); B->BodyInstance.SetMassOverride(400.f*(I+1),true);
		B->RegisterComponent(); if (I<2) { Shared->ConfigureVehicleGravity(B); } Bodies.Add(B);
	}
	TestEqual(TEXT("Unregistered/free-ball body keeps world group"),int32(Bodies[2]->BodyInstance.GravityGroupIndex),0);
	auto* Solver=W->GetPhysicsScene()->GetSolver(); Solver->SetThreadingMode_External(Chaos::EThreadingModeTemp::TaskGraph);
	const FVector WorldGravity(0,0,Gravity);
	W->GetPhysicsScene()->SetUpForFrame(&WorldGravity,1.f/60,0,1.f/60,1.f/60,1,false);
	auto* Probe=Solver->CreateAndRegisterSimCallbackObject_External<FGravityProbe>();
	for (auto* B:Bodies) { Probe->GetProducerInputData_External()->Bodies.Add(B->BodyInstance.GetPhysicsActor()); }
	TArray<FVector> Last;
	const auto Steps=[&]()
	{
		for (int32 I=0;I<60;++I)
		{
			Solver->AdvanceAndDispatch_External(1.f/60); Solver->WaitOnPendingTasks_External();
			while (auto O=Probe->PopFutureOutputData_External()) { Last=O->Velocities; }
		}
	};
	Steps();
	if (TestEqual(TEXT("Three real integrated bodies sampled"),Last.Num(),3))
	{
		TestEqual(TEXT("Vehicle A integrates 1.5x gravity"),Last[0].Z,double(Gravity*1.5f),0.5);
		TestEqual(TEXT("Vehicle B gravity is mass independent"),Last[1].Z,Last[0].Z,0.1);
		TestEqual(TEXT("Free-body gravity stays unchanged"),Last[2].Z,double(Gravity),0.5);
		const double Before=Last[1].Z; Bodies[1]->SetEnableGravity(false); Steps();
		TestEqual(TEXT("Gravity-disabled fixture remains respected"),Last[1].Z,Before,0.1);
	}
	Solver->UnregisterAndFreeSimCallbackObject_External(Probe);
	W->DestroyWorld(false); GEngine->DestroyWorldContext(W);
	return true;
}
#endif
