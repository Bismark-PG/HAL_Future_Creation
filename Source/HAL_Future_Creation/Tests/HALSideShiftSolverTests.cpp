#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "HALSideShiftTestSupport.h"
#include "../VehicleConfigurationTypes.h"
#include "../VehicleDefinition.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
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
		float Yaw=0;
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
			auto& O=GetProducerOutputData_Internal(); O.Position=Handle->X(); O.Velocity=Handle->V(); O.Angular=Handle->W(); O.Yaw=Handle->R().Rotator().Yaw; O.Frame=Frame; O.Physics=Chaos::IsInPhysicsThreadContext();
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
		B->BodyInstance.bUseCCD=Sim; // Production vehicle constructor already requires CCD.
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
	TArray<FShiftSolverOutput> Samples;
	FShiftSolverOutput LastObservation;
	const auto Observe=[&](int32 Steps)
	{
		FShiftSolverOutput Last;
		for(int32 I=0;I<Steps;++I)
		{
			Step(); while(auto Out=Probe->PopFutureOutputData_External())
			{ TestTrue(TEXT("Real Solver context"),Out->Physics); Last.Position=Out->Position; Last.Velocity=Out->Velocity; Last.Angular=Out->Angular; Last.Yaw=Out->Yaw; Last.Frame=Out->Frame; Samples.Add(Last); }
		}
		LastObservation=Last;
		return Last;
	};
	const auto Reset=[&](FVector Velocity)
	{
		FHALMovementSnapshot Empty; Empty.Epoch=Epoch; Empty.Revision=++Revision; TestTrue(TEXT("Reset profile queued"),M->QueueMovementParameters(Empty));
		Body->SetWorldLocationAndRotation(FVector(0,0,50),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
		Body->SetPhysicsLinearVelocity(Velocity); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector); Body->WakeAllRigidBodies();
		FVehicleInputCmd Cmd; M->SetInputCommand(Cmd); return Observe(2);
	};
	const auto Publish=[&](float Side,float Bend=1.f,float Span=450.f)
	{
		FHALMovementSnapshot S; S.Epoch=Epoch; S.Revision=++Revision; S.SideShift.Activation=FGuid::NewGuid(); S.SideShift.bActive=true; S.SideShift.Direction=FVector(0,Side,0);
		S.SideShift.DriftArcBend=Bend; S.SideShift.DriftLateralSpan=Span;
		S.SideShift.Origin=LastObservation.Position; S.SideShift.InitialVelocity=LastObservation.Velocity;
		S.SideShift.InitialForward=FRotator(0,LastObservation.Yaw,0).Vector();
		S.SideShift.Direction=FVector::CrossProduct(FVector::UpVector,S.SideShift.InitialForward)*Side;
		TestTrue(TEXT("Production side motion queue accepts profile"),M->QueueMovementParameters(S)); return S;
	};
	for(float Side:{-1.f,1.f})
	{
		const auto Start=Reset(FVector(600,0,0)); Publish(Side); const auto End=Observe(22);
		TestTrue(TEXT("Grounded motion travels configured 3m sideways"),FMath::Abs((End.Position.Y-Start.Position.Y)*Side-300)<8);
		TestTrue(TEXT("Existing forward velocity retained"),FMath::Abs(End.Velocity.X-600)<1);
		TestTrue(TEXT("Translation motor returns to initial lateral speed"),FMath::Abs(End.Velocity.Y)<3);
		AddInfo(FString::Printf(TEXT("Solver side=%g displacement=%.3f forward=%.3f finalLateral=%.3f"),Side,(End.Position.Y-Start.Position.Y)*Side,End.Velocity.X,End.Velocity.Y));
	}
	const auto Standing=Reset(FVector::ZeroVector); Publish(1); const auto StandingEnd=Observe(22);
	TestTrue(TEXT("Standing start also moves 3m"),FMath::Abs(StandingEnd.Position.Y-Standing.Position.Y-300)<8);
	FVehicleInputCmd Drift; Drift.bHandbrake=true;
	for(float Side:{-1.f,1.f})
	{
		Reset(FVector::ZeroVector); Publish(Side); Drift.Steering=Side; M->SetInputCommand(Drift);
		const auto Early=Observe(5), Middle=Observe(5), AtWindow=Observe(10); const auto Held=Observe(70);
		for(const auto& Point:{Early,Middle,AtWindow})
		{ TestTrue(TEXT("Actual chassis center follows the same-side turning circle"),FMath::Abs(FVector2D(Point.Position.X,Point.Position.Y-Side*450).Size()-450)<8); }
		TestTrue(TEXT("Enhanced drift has its own real lateral span, not 3m curve length"),FMath::Abs(AtWindow.Position.Y-Side*450)<8&&FMath::Abs(AtWindow.Position.X-450)<8);
		TestTrue(TEXT("Real chassis turns in the input direction without changing the accepted arc"),FMath::Abs(AtWindow.Yaw-Side*90)<3);
		TestTrue(TEXT("Holding drift cannot accumulate a full spin"),FMath::Abs(Held.Yaw-Side*90)<1&&FMath::Abs(Held.Angular.Z)<.01);
		AddInfo(FString::Printf(TEXT("DriftArc side=%g early=(%.3f,%.3f) middle=(%.3f,%.3f) end=(%.3f,%.3f) yaw=%.3f heldRate=%.4f"),Side,Early.Position.X,Early.Position.Y,Middle.Position.X,Middle.Position.Y,AtWindow.Position.X,AtWindow.Position.Y,AtWindow.Yaw,Held.Angular.Z));
		FVehicleInputCmd Released; M->SetInputCommand(Released); const auto AfterRelease=Observe(8);
		TestTrue(TEXT("Releasing handbrake leaves no residual skill spin"),FMath::Abs(AfterRelease.Angular.Z)<.01);
	}
	// Actual Car07 reference wheel positions expose folds that center/end-yaw checks miss.
	TArray<FVector> WheelOffsets;
	if(const auto* Class=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_07.BP_VehiclePawn_Car_07_C")))
	if(const auto* Definition=Class->GetDefaultObject<ATestVehiclePawn>()->GetVehicleDefinition();Definition&&Definition->Presentation&&Definition->Presentation->Mesh)
	{
		const auto* Presentation=Definition->Presentation.Get(); const auto& Ref=Presentation->Mesh->GetRefSkeleton();
		for(const auto& Wheel:Presentation->Wheels)
		{
			int32 Bone=Ref.FindBoneIndex(Wheel.SpinBone); if(Bone<0) { continue; }
			FTransform Local=Ref.GetRefBonePose()[Bone];
			while((Bone=Ref.GetParentIndex(Bone))>=0) { Local*=Ref.GetRefBonePose()[Bone]; }
			WheelOffsets.Add(Presentation->RelativeTransform.TransformPosition(Local.GetLocation()));
		}
	}
	TestEqual(TEXT("Four actual Car07 reference wheel offsets available"),WheelOffsets.Num(),4);
	FString CSV=TEXT("case,frame,x,y,yaw,vx,vy,w0x,w0y,w1x,w1y,w2x,w2y,w3x,w3y\n");
	for(float Side:{-1.f,1.f}) for(float Speed:{0.f,1200.f})
	{
		auto Entry=Reset(FVector(Speed,0,0)); Drift.Steering=Side; M->SetInputCommand(Drift);
		if(Speed>0) { Entry=Observe(4); } // Already driving and steering with handbrake before the cast.
		Samples.Reset(); Publish(Side);
		const auto Exit=Observe(16); const auto After=Observe(8);
		if(Speed>0)
		{
			const FVector Expected=Entry.Velocity.RotateAngleAxis(Side*90,FVector::UpVector);
			TestTrue(TEXT("Already-drifting entry momentum turns with the full arc instead of reverting on exit"),Exit.Velocity.Equals(Expected,5));
			TestTrue(TEXT("Return to ordinary movement does not restore original forward velocity"),FVector::DotProduct(Exit.Velocity.GetSafeNormal2D(),After.Velocity.GetSafeNormal2D())>.999);
		}
		for(const auto& Point:Samples)
		{
			const auto Rotation=FRotator(0,Point.Yaw,0).Quaternion();
			if(Point.Velocity.Size2D()>20)
			{ TestTrue(TEXT("Center velocity and actual chassis heading turn together throughout the action"),FVector::DotProduct(Point.Velocity.GetSafeNormal2D(),Rotation.GetForwardVector())>.96); }
			FString Row=FString::Printf(TEXT("%g_%g,%d,%.5f,%.5f,%.5f,%.5f,%.5f"),Side,Speed,Point.Frame,Point.Position.X-Entry.Position.X,Point.Position.Y-Entry.Position.Y,Point.Yaw,Point.Velocity.X,Point.Velocity.Y);
			for(const auto& Offset:WheelOffsets)
			{
				const FVector Wheel=Point.Position-Entry.Position+Rotation.RotateVector(Offset);
				Row+=FString::Printf(TEXT(",%.5f,%.5f"),Wheel.X,Wheel.Y);
				if(Speed==0)
				{ TestTrue(TEXT("Actual Car07 wheel traces stay on complete concentric arcs rather than folding back"),FMath::Abs((Wheel-FVector(0,Side*450,Wheel.Z)).Size2D()-(Offset-FVector(0,Side*450,Offset.Z)).Size2D())<12); }
			}
			CSV+=Row+TEXT("\n");
		}
		AddInfo(FString::Printf(TEXT("CoupledArc side=%g initialSpeed=%g end=%s exitV=%s afterV=%s yaw=%.3f"),Side,Speed,*(Exit.Position-Entry.Position).ToString(),*Exit.Velocity.ToString(),*After.Velocity.ToString(),Exit.Yaw));
	}
	const FString PlotDirectory=FPaths::ProjectSavedDir()/TEXT("Car07SideShift/20261116_CoupledArc");
	IFileManager::Get().MakeDirectory(*PlotDirectory,true);
	TestTrue(TEXT("Actual solver center and four-wheel paths exported"),FFileHelper::SaveStringToFile(CSV,*(PlotDirectory/TEXT("Solver_WheelPaths.csv"))));
	Reset(FVector::ZeroVector); Publish(1,.5f,600); Drift.Steering=1; M->SetInputCommand(Drift); const auto Tuned=Observe(16);
	TestTrue(TEXT("Real motor independently applies configured 600cm lateral span and half-depth bend"),FMath::Abs(Tuned.Position.Y-600)<8&&FMath::Abs(Tuned.Position.X-300)<8);
	Reset(FVector::ZeroVector); Publish(1); M->SetInputCommand(Drift); Observe(5);
	M->QueueRecoil(FVector(-200,0,0)); const auto Recoil=Observe(11);
	TestTrue(TEXT("Arc motor retains a real gameplay recoil impulse in its base momentum"),FMath::Abs(Recoil.Velocity.X+200)<1&&FMath::Abs(Recoil.Velocity.Y)<1);
	Body->SetCenterOfMass(FVector(-15,-40,0));
	for(float Side:{-1.f,1.f})
	{
		Reset(FVector::ZeroVector); Publish(Side); Drift.Steering=Side; M->SetInputCommand(Drift); const auto Offset=Observe(20);
		TestTrue(TEXT("Asymmetric COM still follows the configured vehicle-root span"),FMath::Abs(Offset.Position.Y-Side*450)<8&&FMath::Abs(Offset.Position.X-450)<8);
		AddInfo(FString::Printf(TEXT("OffsetCOM side=%g root=(%.3f,%.3f) yaw=%.3f"),Side,Offset.Position.X,Offset.Position.Y,Offset.Yaw));
	}
	Body->SetCenterOfMass(FVector::ZeroVector);
	auto* Wall=Box(FVector(0,200,150),FVector(1000,20,150),false); Step();
	const auto BeforeWall=Reset(FVector::ZeroVector); Publish(1); const auto Blocked=Observe(62);
	TestTrue(TEXT("Real blocking wall prevents authored distance; no teleport through it"),Blocked.Position.Y<150&&Blocked.Position.Y-BeforeWall.Position.Y<200);
	Reset(FVector::ZeroVector); Publish(1); Drift.Steering=1; M->SetInputCommand(Drift); const auto ArcBlocked=Observe(22);
	AddInfo(FString::Printf(TEXT("Arc wall result position=%s velocity=%s CCD=%d"),*ArcBlocked.Position.ToString(),*ArcBlocked.Velocity.ToString(),Body->BodyInstance.bUseCCD));
	TestTrue(TEXT("Real wall also blocks the enhanced arc motor"),ArcBlocked.Position.Y<150);
	Wall->DestroyComponent();
	Solver->UnregisterAndFreeSimCallbackObject_External(Probe); Step(); W->DestroyWorld(false); GEngine->DestroyWorldContext(W); return true;
}
#endif
