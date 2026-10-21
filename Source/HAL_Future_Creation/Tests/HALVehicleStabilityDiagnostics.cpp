#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../HALImpactTypes.h"
#include "../VehicleConfigurationApplication.h"
#include "../VehicleDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EventManager.h"
#include "EventsData.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTLS.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PBDRigidsSolver.h"
#include "Chaos/SimCallbackObject.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

/** Only the diagnostic callback executes forces; generic driving must stay disabled. */
struct FHALVehicleStabilityTestAccess
{
	static void Prepare(UArcadeVehicleMovementComponent* Movement)
	{
		Movement->SetAsyncPhysicsTickEnabled(false);
		Movement->bShouldRunPhysics.Store(true);
	}
};

namespace HALStabilityDiagnostic
{
	constexpr double Dt = 1.0 / 60.0;
	struct FInput : Chaos::FSimCallbackInput
	{
		UPrimitiveComponent* Body = nullptr;
		UArcadeVehicleMovementComponent* Movement = nullptr;
		FVector Velocity = FVector::ZeroVector, AngularVelocity = FVector::ZeroVector;
		FVector Supplement = FVector::ZeroVector;
		int32 Step = 0;
		bool bInitialize = false;
		void Reset() { Body = nullptr; Movement = nullptr; Step = 0; bInitialize = false; Supplement = FVector::ZeroVector; }
	};
	struct FOutput : Chaos::FSimCallbackOutput
	{
		FVector X = FVector::ZeroVector, V = FVector::ZeroVector, W = FVector::ZeroVector;
		FQuat R = FQuat::Identity;
		int32 Step = 0;
		bool bPhysicsThread = false;
		uint32 Thread = 0;
		void Reset() { Step = 0; }
	};
	class FProbe : public Chaos::TSimCallbackObject<FInput, FOutput,
		Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::PostSolve>
	{
		FBodyInstanceAsyncPhysicsTickHandle Handle;
		int32 Pending = 0;
		virtual void OnPreSimulate_Internal() override
		{
			const auto* In = GetConsumerInput_Internal();
			if (!In || !In->Body || !In->Step) { return; }
			Handle = In->Body->GetBodyInstance()->GetBodyInstanceAsyncPhysicsTickHandle();
			if (!Handle.IsValid()) { return; }
			Pending = In->Step;
			if (In->bInitialize)
			{
				Handle->SetObjectState(Chaos::EObjectStateType::Dynamic, true);
				Handle->SetV(In->Velocity); Handle->SetW(In->AngularVelocity);
			}
			if (!In->Supplement.IsNearlyZero())
			{
				FVehicleImpactAction Action; Action.EventSequence = In->Step; Action.DeltaVelocity = In->Supplement;
				In->Movement->ApplyAction_Internal(TInstancedStruct<FNetworkPhysicsActionPayload>::Make<FVehicleImpactAction>(Action));
			}
		}
		virtual void OnPostSolve_Internal() override
		{
			if (!Pending || !Handle.IsValid()) { return; }
			const auto* Rigid = Handle.Proxy->GetHandle_LowLevel()->CastToRigidParticle();
			if (!Rigid) { return; }
			auto& Out = GetProducerOutputData_Internal(); Out.Step = Pending;
			// Evolution commits P/Q into X/R after PostSolve; sample the solved pose.
			Out.X = Rigid->GetP(); Out.R = Rigid->GetQ(); Out.V = Handle->V(); Out.W = Handle->W();
			Out.bPhysicsThread = Chaos::IsInPhysicsThreadContext(); Out.Thread = FPlatformTLS::GetCurrentThreadId();
			Pending = 0;
		}
	};
	struct FContact
	{
		const IPhysicsProxyBase* Other = nullptr;
		FVector Impulse = FVector::ZeroVector, Location = FVector::ZeroVector, Normal = FVector::ZeroVector;
	};
	struct FContactCollector
	{
		const IPhysicsProxyBase* BodyProxy = nullptr;
		TArray<FContact> Contacts;
		void Receive(const Chaos::FCollisionEventData& Data)
		{
			for (const auto& C : Data.CollisionData.AllCollisionsArray)
			{
				if (C.Proxy1 != BodyProxy && C.Proxy2 != BodyProxy) { continue; }
				const bool bFirst = C.Proxy1 == BodyProxy;
				const auto* Other = bFirst ? C.Proxy2 : C.Proxy1;
				FContact* Entry = Contacts.FindByPredicate([Other](const FContact& V) { return V.Other == Other; });
				if (!Entry)
				{
					Entry = &Contacts.AddDefaulted_GetRef(); Entry->Other = Other;
					Entry->Location = FVector(C.Location); Entry->Normal = FVector(C.Normal) * (bFirst ? 1 : -1);
				}
				Entry->Impulse += FVector(C.Normal) * FVector::DotProduct(FVector(C.AccumulatedImpulse), FVector(C.Normal)) * (bFirst ? 1 : -1);
			}
		}
	};
	struct FResult
	{
		FString Name;
		double FirstContact = -1, FirstExitVz = 0, ReboundClearance = 0, StableAt = -1;
		double FinalSpeed = 0, FinalAngular = 0, MaxSupplement = 0, TotalSupplement = 0;
		int32 ContactFrames = 0, Episodes = 0, Outputs = 0;
		bool bWorker = false;
	};
	FString Vec(const FVector& V) { return FString::Printf(TEXT("%.9f,%.9f,%.9f"), V.X, V.Y, V.Z); }
	UStaticMeshComponent* MeshBody(UWorld* World, UStaticMesh* Mesh, const FTransform& Transform,
		const FBodyInstance& BodyConfig, const FVector& Scale, bool bDynamic)
	{
		auto* Owner = World->SpawnActor<AActor>();
		auto* Body = NewObject<UStaticMeshComponent>(Owner);
		Owner->AddInstanceComponent(Body); Owner->SetRootComponent(Body);
		Body->SetStaticMesh(Mesh); Body->SetMobility(bDynamic ? EComponentMobility::Movable : EComponentMobility::Static);
		Body->BodyInstance.CopyBodyInstancePropertiesFrom(&BodyConfig);
		Body->BodyInstance.bSimulatePhysics = bDynamic;
		Body->SetWorldTransform(Transform); Body->SetWorldScale3D(Scale);
		Body->SetVisibility(false); Body->RegisterComponent();
		return Body;
	}
	FResult Run(UVehicleDefinition* Definition, const FVehicleKnockbackConfig& Table, const FHALCollisionImpactRules& Rules,
		const FString& Name, const FQuat& Rotation, const FVector& InitialV, const FVector& InitialW,
		double Clearance, bool bWall, bool bTallWall, bool bLowMaterial, bool bRuleSupplement,
		TArray<FString>& Trace, TArray<FString>& ContactTrace, TArray<FString>& Native)
	{
		// Raw Chaos contacts + production budget helper + production action execution.
		// Contact episode timing is isolated here; this is not a full GAS/RPC/delegate test.
		FResult Result; Result.Name = Name;
		const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
			.ShouldSimulatePhysics(true).SetTransactional(false);
		auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		FBodyInstance StaticConfig; StaticConfig.SetCollisionProfileName(TEXT("BlockAll"));
		auto* Floor = MeshBody(World, LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/MapTemplates/SM_Template_Map_Floor.SM_Template_Map_Floor")),
			FTransform(FVector(0,0,0)), StaticConfig, FVector(8), false);
		// Use the same floor mesh/scale as both test maps; normalize its upper plane to Z=0.
		const double FloorTop = Floor->GetStaticMesh()->GetBoundingBox().Max.Z * 8;
		Floor->SetWorldLocation(FVector(0,0,-FloorTop));
		if (bWall)
		{
			MeshBody(World, LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")),
				FTransform(FVector(1000,0,bTallWall ? 200 : 0)), StaticConfig, FVector(1,80,bTallWall ? 5 : 1), false);
		}
		TArray<FVector> Vertices;
		for (const auto& Convex : Definition->CollisionMesh->GetBodySetup()->AggGeom.ConvexElems)
		{
			for (const FVector& V : Convex.VertexData) { Vertices.Add(Convex.GetTransform().TransformPosition(V) * Definition->Physics.Scale); }
		}
		double MinZ = 0;
		for (const FVector& V : Vertices) { MinZ = FMath::Min(MinZ, Rotation.RotateVector(V).Z); }
		auto* Body = MeshBody(World, Definition->CollisionMesh, FTransform(Rotation, FVector(0,0,Clearance-MinZ)),
			Definition->Physics.Body, Definition->Physics.Scale, true);
		if (bLowMaterial)
		{
			auto* Mat = DuplicateObject<UPhysicalMaterial>(Body->BodyInstance.GetSimplePhysicalMaterial(), Body);
			Mat->Restitution = 0; Mat->bOverrideRestitutionCombineMode = true; Mat->RestitutionCombineMode = EFrictionCombineMode::Min;
			Body->SetPhysMaterialOverride(Mat);
		}
		auto* Movement = NewObject<UArcadeVehicleMovementComponent>(Body->GetOwner());
		Body->GetOwner()->AddInstanceComponent(Movement); Movement->SetUpdatedPrimitive(Body);
		Movement->RegisterComponent(); FHALVehicleStabilityTestAccess::Prepare(Movement);
		const auto* PM = Body->BodyInstance.GetSimplePhysicalMaterial();
		const auto* FloorPM = Floor->BodyInstance.GetSimplePhysicalMaterial();
		Native.Add(FString::Printf(TEXT("%s,vehicle_material,%s,%.9f,%d,%d"), *Name, *PM->GetPathName(), PM->Restitution, PM->bOverrideRestitutionCombineMode, int32(PM->RestitutionCombineMode)));
		Native.Add(FString::Printf(TEXT("%s,floor_material,%s,%.9f,%d,%d"), *Name, *FloorPM->GetPathName(), FloorPM->Restitution, FloorPM->bOverrideRestitutionCombineMode, int32(FloorPM->RestitutionCombineMode)));
		Native.Add(FString::Printf(TEXT("%s,mass_kg,%.9f"), *Name, Body->GetMass()));
		Native.Add(Name+TEXT(",inertia_kg_cm2,")+Vec(Body->GetInertiaTensor()));
		Native.Add(Name+TEXT(",initial_com_cm,")+Vec(Body->GetCenterOfMass()));
		Native.Add(FString::Printf(TEXT("%s,collision_vertices,%d"), *Name, Vertices.Num()));
		auto* Solver = World->GetPhysicsScene()->GetSolver();
		Solver->SetThreadingMode_External(Chaos::EThreadingModeTemp::TaskGraph);
		Solver->SetGenerateCollisionData(true);
		Solver->AdvanceAndDispatch_External(Dt); Solver->WaitOnPendingTasks_External();
		Solver->GetEventManager()->FlipBuffersIfRequired();
		Solver->SyncEvents_GameThread(); // Complete registration before consuming the first input.
		FContactCollector Collector; Collector.BodyProxy = Body->BodyInstance.GetPhysicsActor();
		Solver->GetEventManager()->RegisterHandler<Chaos::FCollisionEventData>(Chaos::EEventType::Collision, &Collector, &FContactCollector::Receive);
		auto* Probe = Solver->CreateAndRegisterSimCallbackObject_External<FProbe>();
		TMap<const IPhysicsProxyBase*, double> LastSeen;
		TSet<const IPhysicsProxyBase*> Consumed;
		FVector PendingSupplement = FVector::ZeroVector;
		double StableDuration = 0;
		const uint32 GT = FPlatformTLS::GetCurrentThreadId();
		for (int32 Step = 1; Step <= 720; ++Step)
		{
			auto* In = Probe->GetProducerInputData_External(); In->Body = Body; In->Movement = Movement;
			In->Step = Step; In->bInitialize = Step == 1; In->Velocity = InitialV; In->AngularVelocity = InitialW;
			In->Supplement = PendingSupplement; PendingSupplement = FVector::ZeroVector;
			Collector.Contacts.Reset();
			Solver->AdvanceAndDispatch_External(Dt); Solver->WaitOnPendingTasks_External();
			FOutput State;
			bool bFound = false;
			while (auto Out = Probe->PopFutureOutputData_External())
			{
				if (Out->Step == Step) { State = *Out; bFound = true; }
			}
			// Manual Solver stepping must perform the scene's event-buffer handoff too.
			Solver->GetEventManager()->FlipBuffersIfRequired();
			Solver->SyncEvents_GameThread();
			if (!bFound) { continue; }
			++Result.Outputs; Result.bWorker |= State.bPhysicsThread && State.Thread != GT;
			const double Time = Step * Dt;
			double Bottom = State.X.Z;
			for (const FVector& V : Vertices) { Bottom = FMath::Min(Bottom, State.X.Z + State.R.RotateVector(V).Z); }
			if (!Collector.Contacts.IsEmpty())
			{
				++Result.ContactFrames;
				if (Result.FirstContact < 0) { Result.FirstContact = Time; Result.FirstExitVz = State.V.Z; }
			}
			for (const auto& Contact : Collector.Contacts)
			{
				const double* Seen = LastSeen.Find(Contact.Other);
				if (!Seen || Time - *Seen > Rules.ContactRearmSeconds) { Consumed.Remove(Contact.Other); }
				LastSeen.Add(Contact.Other, Time);
				const FVector Direction = Contact.Impulse.GetSafeNormal();
				const double Natural = Contact.Impulse.Size() / Body->GetMass();
				const FVector PointV = State.V + FVector::CrossProduct(State.W, Contact.Location-State.X);
				const double RelativeAfter = FVector::DotProduct(PointV, Direction);
				const EHALImpactTier Tier = Rules.SelectTier(Natural, Rules.EnvironmentTierCap);
				double Supplement = 0;
				if (Tier != EHALImpactTier::None && !Consumed.Contains(Contact.Other))
				{
					Consumed.Add(Contact.Other); ++Result.Episodes;
					const auto& Outcome = Table.GetTierDefinition(HALImpact::ToLegacy(Tier));
					Supplement = HALImpact::PhysicalSupplement(Outcome.HorizontalDeltaSpeed, Natural, RelativeAfter, 0, Rules.MaximumRuleRestitution);
					if (bRuleSupplement) { PendingSupplement += Direction * Supplement; }
				}
				Result.MaxSupplement = FMath::Max(Result.MaxSupplement, Supplement);
				if (bRuleSupplement) { Result.TotalSupplement += Supplement; }
				ContactTrace.Add(FString::Printf(TEXT("%s,%d,%.9f,%s,%s,%s,%.9f,%.9f,%d,%.9f,%d"), *Name, Step, Time,
					*Vec(Contact.Impulse), *Vec(Contact.Location), *Vec(Contact.Normal), Natural, RelativeAfter, int32(Tier), Supplement, bRuleSupplement));
			}
			if (Result.FirstContact >= 0)
			{
				Result.ReboundClearance = FMath::Max(Result.ReboundClearance, Bottom);
				const bool bStable = State.V.Size() < 5 && State.W.Size() < .1 && FMath::Abs(Bottom) < 5;
				StableDuration = bStable ? StableDuration + Dt : 0;
				if (!bStable) { Result.StableAt = -1; }
				if (StableDuration >= .5 && Result.StableAt < 0) { Result.StableAt = Time - .5; }
			}
			Result.FinalSpeed = State.V.Size(); Result.FinalAngular = State.W.Size();
			Trace.Add(FString::Printf(TEXT("%s,%d,%.9f,%s,%.9f,%.9f,%.9f,%.9f,%s,%s,%.9f,%d,%s"), *Name, Step, Time,
				*Vec(State.X), State.R.X, State.R.Y, State.R.Z, State.R.W, *Vec(State.V), *Vec(State.W), Bottom,
				Collector.Contacts.Num(), *Vec(PendingSupplement)));
		}
		Solver->GetEventManager()->UnregisterHandler(Chaos::EEventType::Collision, &Collector);
		Solver->UnregisterAndFreeSimCallbackObject_External(Probe);
		Solver->AdvanceAndDispatch_External(Dt); Solver->WaitOnPendingTasks_External();
		World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALVehicleStabilityDiagnostic, "HAL.Diagnostics.VehicleStability.SavedContactComparison",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALVehicleStabilityDiagnostic::RunTest(const FString& Parameters)
{
	using namespace HALStabilityDiagnostic;
	FString Prefix;
	if (!FParse::Value(FCommandLine::Get(), TEXT("HALStabilityReport="), Prefix))
	{
		Prefix = FPaths::ProjectSavedDir()/TEXT("VehicleStability")/FGuid::NewGuid().ToString();
	}
	Prefix = FPaths::ConvertRelativePathToFull(Prefix);
	if (!TestTrue(TEXT("Diagnostic output stays inside Saved"), FPaths::IsUnderDirectory(Prefix, FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir())))) { return false; }
	for (const TCHAR* Suffix : {TEXT("_summary.csv"), TEXT("_trace.csv"), TEXT("_contacts.csv"), TEXT("_native.csv")})
	{
		if (!TestFalse(TEXT("Existing evidence is never overwritten"), IFileManager::Get().FileExists(*(Prefix+Suffix)))) { return false; }
	}
	auto* Definition = LoadObject<UVehicleDefinition>(nullptr, TEXT("/Game/Data/MVP_B/DA_Vehicle_Player_TestBaseline_v1.DA_Vehicle_Player_TestBaseline_v1"));
	auto* Combat = LoadObject<UCombatRulesDefinition>(nullptr, TEXT("/Game/Data/MVP_B/DA_Combat_TestBaseline_v1.DA_Combat_TestBaseline_v1"));
	if (!TestNotNull(TEXT("Saved vehicle"), Definition) || !TestNotNull(TEXT("Saved combat rules"), Combat)
		|| !TestNotNull(TEXT("Saved convex collision"), Definition->CollisionMesh.Get())) { return false; }
	TArray<FString> Summary{TEXT("case,first_contact_s,first_contact_exit_vz_cm_s,max_clearance_after_contact_cm,stable_at_s,final_speed_cm_s,final_angular_rad_s,contact_frames,qualified_episodes,max_candidate_supplement_cm_s,total_applied_supplement_cm_s,outputs,worker")};
	TArray<FString> Trace{TEXT("case,step,time_s,x,y,z,qx,qy,qz,qw,vx,vy,vz,wx,wy,wz,bottom_z,contact_pairs,next_rule_dvx,next_rule_dvy,next_rule_dvz")};
	TArray<FString> Contacts{TEXT("case,step,time_s,impulse_x,impulse_y,impulse_z,point_x,point_y,point_z,normal_x,normal_y,normal_z,natural_dv,relative_after,tier,candidate_supplement,rule_enabled")};
	TArray<FString> Native{TEXT("case,property,value,optional_values")};
	for (int32 Scenario = 0; Scenario < 4; ++Scenario)
	{
		for (int32 Variant = 0; Variant < 4; ++Variant)
		{
			const bool bLow = (Variant & 2) != 0, bRules = (Variant & 1) != 0;
			const TCHAR* ScenarioName = Scenario == 0 ? TEXT("flat_drop") : Scenario == 1 ? TEXT("heavy_tumble") : Scenario == 2 ? TEXT("short_wall") : TEXT("tall_wall_control");
			const FString Name = FString::Printf(TEXT("%s_%s_%s"), ScenarioName, bLow ? TEXT("zero_min") : TEXT("saved"), bRules ? TEXT("rules") : TEXT("natural"));
			const auto R = Run(Definition, Combat->BallImpacts, Combat->CollisionImpacts, Name,
				Scenario == 1 ? FRotator(20,0,15).Quaternion() : FQuat::Identity,
				Scenario == 1 ? FVector(450,0,750) : Scenario >= 2 ? FVector(1400,0,0) : FVector::ZeroVector,
				Scenario == 1 ? FVector(0,5.13,.7) : FVector::ZeroVector,
				Scenario == 0 ? 500 : 1, Scenario >= 2, Scenario == 3, bLow, bRules, Trace, Contacts, Native);
			TestEqual(Name+TEXT(" 720 real Solver outputs"), R.Outputs, 720);
			TestTrue(Name+TEXT(" actual contact events captured"), R.ContactFrames > 0);
			TestTrue(Name+TEXT(" worker executed probe/action"), R.bWorker);
			TestTrue(Name+TEXT(" finite result"), FMath::IsFinite(R.FinalSpeed) && FMath::IsFinite(R.FinalAngular));
			Summary.Add(FString::Printf(TEXT("%s,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%d,%d,%.9f,%.9f,%d,%d"),
				*Name, R.FirstContact, R.FirstExitVz, R.ReboundClearance, R.StableAt, R.FinalSpeed, R.FinalAngular,
				R.ContactFrames, R.Episodes, R.MaxSupplement, R.TotalSupplement, R.Outputs, R.bWorker));
			AddInfo(FString::Printf(TEXT("%s contact=%.3f rebound=%.3f stable=%.3f supplement=%.3f"), *Name, R.FirstContact, R.ReboundClearance, R.StableAt, R.TotalSupplement));
		}
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Prefix), true);
	TestTrue(TEXT("Summary saved"), FFileHelper::SaveStringArrayToFile(Summary, *(Prefix+TEXT("_summary.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	TestTrue(TEXT("Trajectories saved"), FFileHelper::SaveStringArrayToFile(Trace, *(Prefix+TEXT("_trace.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	TestTrue(TEXT("Contacts saved"), FFileHelper::SaveStringArrayToFile(Contacts, *(Prefix+TEXT("_contacts.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	TestTrue(TEXT("Runtime materials saved"), FFileHelper::SaveStringArrayToFile(Native, *(Prefix+TEXT("_native.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	return true;
}

#endif
