#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "HALVehicleStabilityTestRules.h"
#include "HALMovementAbilityTestSupport.h"
#include "../VehicleRecoveryComponent.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../BallControlComponent.h"
#include "../BasicBallActor.h"
#include "../CombatResolver.h"
#include "../HALImpactAttributeSet.h"
#include "../HALHealthAttributeSet.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../TestVehiclePawn.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "../HALImpactSourceComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include <limits>
#include "PBDRigidsSolver.h"
#include "Chaos/SimCallbackObject.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "../VehicleDefinition.h"
#include "HAL/PlatformTLS.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

struct FHALRecoveryTestAccess
{
	static bool Start(UVehicleRecoveryComponent* Recovery)
	{
		FVehicleStabilityObservation Sample; Sample.GroundNormal = FVector::UpVector; Sample.Forward = FVector::ForwardVector;
		return Recovery->Start(Sample);
	}
	static bool Acquire(UBallControlComponent* Control, ABasicBallActor* Ball) { return Control->AcquireBall(Ball); }
	static void Prepare(UArcadeVehicleMovementComponent* Movement, UPrimitiveComponent* Body)
	{
		Movement->bShouldRunPhysics.Store(true); Movement->RecoveryRules = FVehicleRecoveryConfig();
		const auto Bounds = Body->CalcBounds(FTransform(Body->GetComponentScale()));
		Movement->LocalBodyCenter = Bounds.Origin; Movement->LocalBodyExtent = Bounds.BoxExtent;
	}
	static void Step(UArcadeVehicleMovementComponent* Movement, int32 Frame, float Delta, bool bDrive)
	{
		FVehicleNetInputData Input; Input.InputSequence = Frame; Input.PhysicsFrame = Frame;
		if (bDrive) { Input.Cmd.Throttle = 1; Input.Cmd.Steering = 1; Input.Cmd.bHandbrake = true; }
		Movement->SimulateVehicleStep(Input,Delta);
	}
	static FVehicleRecoveryPhysicsState Physics(UArcadeVehicleMovementComponent* Movement) { return Movement->RecoveryPhysics; }
	static void SetRules(UArcadeVehicleMovementComponent* Movement, const FVehicleRecoveryConfig& Rules) { Movement->RecoveryRules = Rules; }
	static bool Support(UArcadeVehicleMovementComponent* Movement, const FTransform& Pose, FVector& Normal, bool bFallback)
	{
		Movement->GroundProbeRotation = Pose.GetRotation();
		return bFallback ? Movement->UpdateRecoverySupport(Pose.GetLocation(),Normal) : Movement->UpdateGroundContact(Pose.GetLocation(),Normal);
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRecoverySideSupportTest,"HAL.FutureCreation.Recovery.SideSupportAndThresholdBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALRecoverySideSupportTest::RunTest(const FString& Parameters)
{
	FVehicleRecoveryConfig Rules; FString Error;
	Rules.MinimumRecoveryTilt = Rules.DriveExitTilt;
	TestTrue(TEXT("Recovery can meet the drive-exit threshold without a dead band"),Rules.Validate(Error));
	const FVector Lean = FQuat(FVector::ForwardVector,FMath::DegreesToRadians(68.f)).GetUpVector();
	TestFalse(TEXT("68 degree car cannot drive"),Rules.CanDrive(true,Lean,FVector::UpVector));
	TestTrue(TEXT("65 degree recovery setting accepts stable 68 degree lean"),Rules.IsEligible(true,Lean,FVector::UpVector,0,0));
	Rules.MinimumRecoveryTilt = 70;
	TestEqual(TEXT("Existing 70 degree tuning is honored with explicit feedback"),Rules.EligibilityBlockReason(true,Lean,FVector::UpVector,0,0),FName(TEXT("Tilt")));
	TestEqual(TEXT("Air remains rejected"),Rules.EligibilityBlockReason(false,FVector::RightVector,FVector::UpVector,0,0),FName(TEXT("NoGround")));
	TestEqual(TEXT("Fast translation remains rejected"),Rules.EligibilityBlockReason(true,FVector::RightVector,FVector::UpVector,151,0),FName(TEXT("LinearSpeed")));
	TestEqual(TEXT("Fast tumbling remains rejected"),Rules.EligibilityBlockReason(true,FVector::RightVector,FVector::UpVector,0,2),FName(TEXT("AngularSpeed")));
	const auto Options=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Options);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	const auto Box=[World](const FVector& Extent,const FTransform& Pose)
	{
		auto* Actor=World->SpawnActor<AActor>(); auto* Body=NewObject<UBoxComponent>(Actor);
		Actor->AddInstanceComponent(Body); Actor->SetRootComponent(Body); Body->SetBoxExtent(Extent);
		Body->SetCollisionProfileName(TEXT("BlockAll")); Body->SetWorldTransform(Pose); Body->RegisterComponent(); return Body;
	};
	// A narrow ledge supports the front half of a side-resting proxy; its center ray is off the ledge.
	auto* Floor=Box(FVector(20,200,10),FTransform(FVector(60,0,-10)));
	const FTransform Pose(FQuat(FVector::ForwardVector,PI/2),FVector(0,0,80));
	auto* Body=Box(FVector(120,80,30),Pose);
	auto* Movement=NewObject<UArcadeVehicleMovementComponent>(Body->GetOwner()); Body->GetOwner()->AddInstanceComponent(Movement);
	Movement->SetUpdatedPrimitive(Body); Movement->RegisterComponent(); FHALRecoveryTestAccess::Prepare(Movement,Body);
	FVector Normal;
	TestFalse(TEXT("Center driving probe misses the ledge"),FHALRecoveryTestAccess::Support(Movement,Pose,Normal,false));
	TestTrue(TEXT("Projected recovery probe detects real nearby ledge support"),FHALRecoveryTestAccess::Support(Movement,Pose,Normal,true));
	TestTrue(TEXT("Recovery target is a valid floor normal"),Rules.GroundIsValid(true,Normal));
	FTransform Air=Pose; Air.AddToTranslation(FVector(0,0,150));
	TestFalse(TEXT("Fallback does not extend vertical reach for an airborne car"),FHALRecoveryTestAccess::Support(Movement,Air,Normal,true));
	Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box(FVector(10,200,200),FTransform(FVector(60,0,100)));
	TestFalse(TEXT("Wall-only contact never supplies recovery floor"),FHALRecoveryTestAccess::Support(Movement,Pose,Normal,true));
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	if (const auto* Saved=LoadObject<UVehicleStabilityRules>(nullptr,TEXT("/Game/Data/MVP_B/DA_Vehicle_Stability_Test_v1.DA_Vehicle_Stability_Test_v1")))
	{ AddInfo(FString::Printf(TEXT("Saved Recovery (read-only): %s %s"),*Saved->GetPathName(),*Saved->Recovery.SignatureKey())); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRecoveryPolicyTest,"HAL.FutureCreation.Recovery.PoseAndPermissionClock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALRecoveryPolicyTest::RunTest(const FString& Parameters)
{
	FVehicleRecoveryConfig Config; FString Error;
	TestTrue(TEXT("Defaults valid"),Config.Validate(Error));
	const auto UpAt = [](float Degrees) { return FQuat(FVector::ForwardVector,FMath::DegreesToRadians(Degrees)).GetUpVector(); };
	TestTrue(TEXT("Upright drive"),Config.CanDrive(false,FVector::UpVector,FVector::UpVector));
	TestFalse(TEXT("Roof cannot drive"),Config.CanDrive(true,-FVector::UpVector,FVector::UpVector));
	TestFalse(TEXT("Side cannot drive even with throttle/handbrake boost"),Config.CanDrive(true,FVector::RightVector,FVector::UpVector));
	TestTrue(TEXT("Active driving retains hysteresis band"),Config.CanDrive(true,UpAt(60),FVector::UpVector));
	TestFalse(TEXT("Inactive driving cannot reenter in hysteresis band"),Config.CanDrive(false,UpAt(60),FVector::UpVector));
	TestFalse(TEXT("Steep wall is not support"),Config.GroundIsValid(true,FVector::ForwardVector));
	TestFalse(TEXT("No air recovery"),Config.IsEligible(false,-FVector::UpVector,FVector::UpVector,0,0));
	TestFalse(TEXT("High speed ineligible"),Config.IsEligible(true,-FVector::UpVector,FVector::UpVector,151,0));
	TestFalse(TEXT("High angular speed ineligible"),Config.IsEligible(true,-FVector::UpVector,FVector::UpVector,0,1.1f));
	TestTrue(TEXT("Low-speed side qualifies"),Config.IsEligible(true,FVector::RightVector,FVector::UpVector,150,1));
	FVehicleRecoveryClock Clock;
	for (int32 I=0; I<49; ++I) { TestFalse(TEXT("Cannot start before 1+1.5 seconds"),Clock.Advance(Config,true,true,.05f)); }
	TestTrue(TEXT("Starts only after flip dwell plus full hold"),Clock.Advance(Config,true,true,.05f));
	Clock = {}; Clock.Advance(Config,true,true,1.25f);
	TestTrue(TEXT("A time slice only awards post-dwell hold"),FMath::IsNearlyEqual(Clock.Hold,.25f));
	Clock.Advance(Config,true,false,.05f); TestEqual(TEXT("Release resets progress"),Clock.Hold,0.f);
	Clock.Advance(Config,true,true,.5f); Clock.Advance(Config,false,true,.05f);
	TestTrue(TEXT("Condition loss resets dwell and hold"),Clock.Hold == 0 && Clock.Flipped == 0 && Clock.Cooldown == 0);
	Clock.Finish(Config); TestEqual(TEXT("Execution end starts configured cooldown"),Clock.Cooldown,3.f);
	TestFalse(TEXT("Hold during cooldown cannot start"),Clock.Advance(Config,true,true,2.9f));
	TestEqual(TEXT("No banked hold in cooldown"),Clock.Hold,0.f);
	Clock.Advance(Config,true,true,.2f); TestTrue(TEXT("Only post-cooldown time counts"),FMath::IsNearlyEqual(Clock.Hold,.1f,.001f));
	Config.DriveExitTilt = Config.DriveEnterTilt; TestFalse(TEXT("Invalid hysteresis rejected"),Config.Validate(Error));
	Config = {}; Config.HoldSeconds = std::numeric_limits<float>::quiet_NaN(); TestFalse(TEXT("NaN config rejected"),Config.Validate(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRecoveryHistoryTest,"HAL.FutureCreation.Recovery.HistoryAndBoundedResponse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALRecoveryHistoryTest::RunTest(const FString& Parameters)
{
	FVehicleRecoveryConfig Config;
	FVehicleRecoveryPhysicsState State; State.Sequence=12; State.bActive=true;
	const FVector Acc = HALVehicleRecovery::AngularAcceleration(Config,State,-FVector::UpVector,FVector(0,0,2),1.f/60);
	TestTrue(TEXT("Exactly inverted has deterministic finite torque"),!Acc.ContainsNaN() && Acc.Size()>1 && Acc.Size()<=30.001);
	TestEqual(TEXT("Yaw remains outside recovery torque"),Acc.Z,0.0);
	State.bActive=false; TestTrue(TEXT("Inactive phase applies no torque"),HALVehicleRecovery::AngularAcceleration(Config,State,FVector::RightVector,FVector::ZeroVector,.02f).IsZero());
	FVehiclePhysicsHistoryState Source; Source.ServerFrame=123; Source.State.Recovery=State;
	Source.State.Recovery.bActive=true; Source.State.Recovery.ElapsedSeconds=.75f;
	Source.State.bPoseDriveAllowed=true; Source.State.PoseStableSeconds=.1f;
	TArray<uint8> Bytes; bool Success=false;
	{ FMemoryWriter Writer(Bytes); Source.NetSerialize(Writer,nullptr,Success); }
	TestTrue(TEXT("Recovery state serializes"),Success);
	FVehiclePhysicsHistoryState Read;
	{ FMemoryReader Reader(Bytes); Read.NetSerialize(Reader,nullptr,Success); }
	TestTrue(TEXT("Pose memory and recovery restore"),Success && Read.State.Recovery.Equals(Source.State.Recovery)
		&& Read.State.bPoseDriveAllowed && Read.State.PoseStableSeconds == .1f);
	TestTrue(TEXT("Equal replay state matches"),Source.CompareData(Read));
	Read.State.Recovery.bActive=false; TestFalse(TEXT("Missed start forces correction"),Source.CompareData(Read));
	FVehiclePhysicsHistoryInput Input; Input.Input.Cmd.bSelfRight=true; Input.Input.InputSequence=1;
	Bytes.Reset(); { FMemoryWriter Writer(Bytes); Input.NetSerialize(Writer,nullptr,Success); }
	FVehiclePhysicsHistoryInput InputRead; { FMemoryReader Reader(Bytes); InputRead.NetSerialize(Reader,nullptr,Success); }
	TestTrue(TEXT("Held intent travels with physics input"),Success && InputRead.Input.Cmd.bSelfRight);
	InputRead.DecayData(1); TestFalse(TEXT("Expired input releases recovery"),InputRead.Input.Cmd.bSelfRight);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRecoveryGameplayTest,"HAL.FutureCreation.Recovery.BallSkillImpactAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALRecoveryGameplayTest::RunTest(const FString& Parameters)
{
	FScopedHALStabilityTestRules Shared(true);
	const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Options);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Match=World->SpawnActor<AHALMatchGameState>(); World->SetGameState(Match); Match->SetMatchPhase(EHALMatchPhase::Playing);
	auto* PC=World->SpawnActor<AHALPlayerController>(); PC->SetAsLocalPlayerController();
	auto* Player=World->SpawnActor<AHALPlayerState>(); Player->SetOwner(PC); PC->SetPlayerState(Player);
	Player->PreInitializeComponents(); Player->InitializeComponents(); Player->PostInitializeComponents(); Player->DispatchBeginPlay();
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Class=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
	auto* Pawn=World->SpawnActor<ATestVehiclePawn>(Class,FVector(0,0,1000),FRotator::ZeroRotator,Spawn);
	if (!TestNotNull(TEXT("Saved player spawns"),Pawn)) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false; }
	Pawn->PreInitializeComponents(); Pawn->InitializeComponents(); Pawn->PostInitializeComponents(); PC->Possess(Pawn); Pawn->DispatchBeginPlay();
	HALMovementAbilityTests::ConfigurePrimary(Player);
	auto* ASC=Player->GetHALAbilitySystemComponent(); auto* Recovery=Pawn->FindComponentByClass<UVehicleRecoveryComponent>();
	auto* Balls=Pawn->FindComponentByClass<UBallControlComponent>();
	auto* Ball=World->SpawnActor<ABasicBallActor>(); Ball->DispatchBeginPlay();
	TestTrue(TEXT("Original skill can activate before execution"),Pawn->RequestPrimarySkill());
	TestTrue(TEXT("Ball acquires before execution"),FHALRecoveryTestAccess::Acquire(Balls,Ball));
	const FVector BeforeRelease=Ball->GetPhysicsRoot()->GetPhysicsLinearVelocity();
	TestTrue(TEXT("Authority accepts execution action"),FHALRecoveryTestAccess::Start(Recovery));
	TestTrue(TEXT("Held ball becomes Free only at accepted start"),Ball->GetBallState()==EBasicBallState::Free && Balls->GetHeldBall()==nullptr);
	TestTrue(TEXT("Released ball has no launch attack"),Ball->GetBallState()!=EBasicBallState::Launched);
	TestTrue(TEXT("Recovery adds no release velocity"),Ball->GetPhysicsRoot()->GetPhysicsLinearVelocity().Equals(BeforeRelease));
	TestFalse(TEXT("Existing reacquire lock preserved"),Ball->CanBeControlledBy(Pawn));
	TestFalse(TEXT("Reacquire blocked throughout execution"),FHALRecoveryTestAccess::Acquire(Balls,Ball));
	TestFalse(TEXT("New skill rejected during execution"),Pawn->RequestPrimarySkill());
	for (const auto& Spec:ASC->GetActivatableAbilities())
	{ TestFalse(TEXT("Direct GAS activation also rejects execution state"),ASC->TryActivateAbility(Spec.Handle)); }
	TestTrue(TEXT("Existing GE remains active"),ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Boost));
	auto* Source=World->SpawnActor<AActor>();
	auto* SourceBody=NewObject<UBoxComponent>(Source); Source->AddInstanceComponent(SourceBody); Source->SetRootComponent(SourceBody);
	SourceBody->SetCollisionObjectType(ECC_WorldStatic); SourceBody->RegisterComponent();
	FHALCombatHitRequest Contact; Contact.Impact.Mode=EHALImpactMode::PhysicalContact;
	Contact.Hit.SourceActor=Source; Contact.Hit.SourcePhysicsBody=SourceBody; Contact.Hit.NormalImpulse=FVector::ForwardVector;
	Recovery->NotifyExternalImpact(Contact,EHALImpactTier::Heavy);
	TestTrue(TEXT("Stationary floor/wall contact does not interrupt"),Recovery->IsExecuting());
	auto* Profile=NewObject<UHALImpactSourceComponent>(Source); Source->AddInstanceComponent(Profile); Profile->RegisterComponent();
	Recovery->NotifyExternalImpact(Contact,EHALImpactTier::Heavy);
	TestTrue(TEXT("Self-righting into an idle authored source does not interrupt"),Recovery->IsExecuting());
	const auto Hit = [&](int32 Reduction,bool Immune)
	{
		ASC->SetNumericAttributeBase(UHALImpactAttributeSet::GetTierReductionAttribute(),Reduction);
		if (Immune) { ASC->AddLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune); }
		FHALAttackSourceSnapshot Snapshot; Snapshot.SourceActor=Source; Snapshot.FixedDamage=0;
		FHALCombatAction Action; Action.Begin(*Source,Snapshot);
		FHALCombatHitRequest Request; Request.AttackId=Action.GetSnapshot().AttackId; Request.bApplyDamage=false; Request.bApplyImpact=true;
		Request.Impact.Mode=EHALImpactMode::ExplicitTier; Request.Impact.Tier=EHALImpactTier::Heavy;
		Request.Hit.SourceActor=Source; Request.Hit.TargetActor=Pawn; Request.Hit.TargetPhysicsBody=Cast<UPrimitiveComponent>(Pawn->GetRootComponent());
		Request.Hit.ImpactNormal=-FVector::ForwardVector;
		const auto Result=FCombatResolver::ResolveCombatHit(Action,Request);
		if (Immune) { ASC->RemoveLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune); }
		return Result;
	};
	TestEqual(TEXT("Immunity produces None"),Hit(0,true).ImpactSummary.EffectiveTier,EHALImpactTier::None);
	TestTrue(TEXT("Immune attack does not interrupt"),Recovery->IsExecuting());
	TestEqual(TEXT("Resistance produces Light"),Hit(2,false).ImpactSummary.EffectiveTier,EHALImpactTier::Light);
	TestTrue(TEXT("Effective Light does not interrupt"),Recovery->IsExecuting());
	TestEqual(TEXT("Resistance produces Medium"),Hit(1,false).ImpactSummary.EffectiveTier,EHALImpactTier::Medium);
	TestTrue(TEXT("Effective external Medium interrupts and cools down"),!Recovery->IsExecuting()
		&& Recovery->GetStatus().LastOutcome==TEXT("Interrupted") && Recovery->GetStatus().CooldownRemaining==3);
	TestEqual(TEXT("Recovery never heals or damages"),ASC->GetNumericAttribute(UHALHealthAttributeSet::GetCurrentHPAttribute()),100.f);
	TestTrue(TEXT("Existing effect survives interruption"),ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Boost));
	TestTrue(TEXT("Second isolated execution boundary queues"),FHALRecoveryTestAccess::Start(Recovery));
	SourceBody->SetPhysicsLinearVelocity(FVector(20,0,0));
	Recovery->NotifyExternalImpact(Contact,EHALImpactTier::Medium);
	TestTrue(TEXT("Incoming physical source interrupts on effective Medium"),!Recovery->IsExecuting() && Recovery->GetStatus().LastOutcome==TEXT("Interrupted"));
	Recovery->CancelForOwnerExit(); TestFalse(TEXT("Owner exit cannot leave execution active"),Recovery->IsExecuting());
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return true;
}

namespace HALRecoverySolverTest
{
	struct FInput : Chaos::FSimCallbackInput
	{
		UArcadeVehicleMovementComponent* Movement=nullptr;
		UPrimitiveComponent* Body=nullptr;
		int32 Step=0;
		bool bRecover=false, bDrive=false;
		void Reset() { Step=0; Movement=nullptr; Body=nullptr; bRecover=bDrive=false; }
	};
	struct FOutput : Chaos::FSimCallbackOutput
	{
		FVector Position=FVector::ZeroVector, Velocity=FVector::ZeroVector;
		FQuat Rotation=FQuat::Identity;
		FVehicleStabilityObservation Observation;
		int32 Step=0; bool bWorker=false;
		void Reset() { Step=0; }
	};
	class FProbe : public Chaos::TSimCallbackObject<FInput,FOutput,
		Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::PostSolve>
	{
		int32 Pending=0;
		virtual void OnPreSimulate_Internal() override
		{
			const auto* In=GetConsumerInput_Internal(); if (!In || !In->Step) { return; }
			Pending=In->Step;
			if (In->bRecover && (In->Step==1 || In->Step==31))
			{
				FVehicleRecoveryAction Start; Start.Value.Sequence=1; Start.Value.bActive=true;
				In->Movement->ApplyAction_Internal(TInstancedStruct<FNetworkPhysicsActionPayload>::Make<FVehicleRecoveryAction>(Start));
			}
			FHALRecoveryTestAccess::Step(In->Movement,In->Step,GetDeltaTime_Internal(),In->bDrive);
		}
		virtual void OnPostSolve_Internal() override
		{
			if (!Pending) { return; }
			const auto* In=GetConsumerInput_Internal();
			auto Handle=In->Body->GetBodyInstance()->GetBodyInstanceAsyncPhysicsTickHandle();
			const auto* Rigid=Handle.Proxy->GetHandle_LowLevel()->CastToRigidParticle();
			auto& Out=GetProducerOutputData_Internal(); Out.Step=Pending;
			Out.Position=Rigid->GetP(); Out.Rotation=Rigid->GetQ(); Out.Velocity=Handle->V();
			Out.Observation=In->Movement->GetStabilityObservation();
			Out.bWorker=Chaos::IsInPhysicsThreadContext(); Pending=0;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRecoverySolverTest,"HAL.FutureCreation.Recovery.RealSolverPoseAndRighting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALRecoverySolverTest::RunTest(const FString& Parameters)
{
	using namespace HALRecoverySolverTest;
	auto* Def=LoadObject<UVehicleDefinition>(nullptr,TEXT("/Game/Data/MVP_B/DA_Vehicle_Player_TestBaseline_v1.DA_Vehicle_Player_TestBaseline_v1"));
	if (!TestNotNull(TEXT("Saved collision definition"),Def) || !TestNotNull(TEXT("Saved convex"),Def->CollisionMesh.Get())) { return false; }
	TArray<FString> Trace{TEXT("case,step,x,y,z,up_dot,vx,vy,vz,drive,active,elapsed,worker")};
	float DefaultSideSeconds=0, DefaultRoofSeconds=0;
	for (int32 Case=0; Case<8; ++Case)
	{
		const auto Options=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
			.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(true).EnableTraceCollision(true).SetTransactional(false);
		auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Options);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		const auto MakeBody=[World](UStaticMesh* Mesh,const FTransform& Pose,const FVector& Scale,bool Dynamic)
		{
			auto* Actor=World->SpawnActor<AActor>(); auto* Body=NewObject<UStaticMeshComponent>(Actor);
			Actor->AddInstanceComponent(Body); Actor->SetRootComponent(Body); Body->SetStaticMesh(Mesh);
			Body->SetMobility(EComponentMobility::Movable); Body->SetCollisionProfileName(Dynamic ? TEXT("PhysicsActor") : TEXT("BlockAll"));
			Body->SetWorldTransform(Pose); Body->SetWorldScale3D(Scale); Body->SetSimulatePhysics(Dynamic); Body->RegisterComponent(); return Body;
		};
		MakeBody(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")),FTransform(FVector(0,0,-50)),FVector(80,80,1),false);
		const float Tilt=Case==0 ? 0 : Case==1 || Case==3 || Case==5 ? 90 : Case==2 || Case==6 ? -90 : 180;
		const FQuat Rotation(FVector::ForwardVector,FMath::DegreesToRadians(Tilt));
		double MinimumZ=0;
		for (const auto& Convex : Def->CollisionMesh->GetBodySetup()->AggGeom.ConvexElems)
		{ for (const FVector& V : Convex.VertexData) { MinimumZ=FMath::Min(MinimumZ,Rotation.RotateVector(Convex.GetTransform().TransformPosition(V)*Def->Physics.Scale).Z); } }
		auto* Body=MakeBody(Def->CollisionMesh,FTransform(Rotation,FVector(0,0,.5-MinimumZ)),Def->Physics.Scale,true);
		Body->SetLinearDamping(Def->Physics.Body.LinearDamping); Body->SetAngularDamping(Def->Physics.Body.AngularDamping);
		Body->SetMassOverrideInKg(NAME_None,800);
		auto* Mat=DuplicateObject<UPhysicalMaterial>(Body->BodyInstance.GetSimplePhysicalMaterial(),Body);
		Mat->Restitution=0; Mat->bOverrideRestitutionCombineMode=true; Mat->RestitutionCombineMode=EFrictionCombineMode::Min; Body->SetPhysMaterialOverride(Mat);
		auto* Movement=NewObject<UArcadeVehicleMovementComponent>(Body->GetOwner()); Body->GetOwner()->AddInstanceComponent(Movement);
		Movement->SetUpdatedPrimitive(Body); Movement->RegisterComponent(); FHALRecoveryTestAccess::Prepare(Movement,Body);
		if (Case>=5)
		{
			FVehicleRecoveryConfig Fast; Fast.AngularPositionGain=60; Fast.AngularDampingGain=12;
			Fast.MaximumAngularAcceleration=80; Fast.MaximumRecoveryAngularSpeed=6;
			FHALRecoveryTestAccess::SetRules(Movement,Fast);
		}
		auto* Solver=World->GetPhysicsScene()->GetSolver(); Solver->SetThreadingMode_External(Chaos::EThreadingModeTemp::TaskGraph);
		Solver->AdvanceAndDispatch_External(1.f/60); Solver->WaitOnPendingTasks_External();
		auto* Probe=Solver->CreateAndRegisterSimCallbackObject_External<FProbe>();
		FOutput Last; float BestUp=-1, FirstSettledSeconds=0; int32 Outputs=0; bool InvalidDrive=false;
		for (int32 Step=1; Step<=180; ++Step)
		{
			auto* In=Probe->GetProducerInputData_External(); In->Movement=Movement; In->Body=Body; In->Step=Step;
			In->bRecover=Case>=3; In->bDrive=Case<3;
			Solver->AdvanceAndDispatch_External(1.f/60); Solver->WaitOnPendingTasks_External();
			while (auto Out=Probe->PopFutureOutputData_External())
			{
				Last=*Out; ++Outputs;
				const auto& O=Out->Observation;
				const float Up=Out->Rotation.GetUpVector().Z;
				if (O.Up.Z < .3 && O.bDrivingAllowed) { InvalidDrive=true; }
				if (Step<=120) { BestUp=FMath::Max(BestUp,Up); }
				if (Case>=3 && FirstSettledSeconds==0 && Up>=FMath::Cos(FMath::DegreesToRadians(15.f)) && O.AngularSpeed<=FMath::DegreesToRadians(60.f))
				{ FirstSettledSeconds=Step/60.f; }
				if (Step==31 && Case>=3) { TestTrue(TEXT("Duplicate start cannot reset execution timer"),O.Recovery.ElapsedSeconds>.5f); }
				Trace.Add(FString::Printf(TEXT("%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%d,%.6f,%d"),Case,Step,
					Out->Position.X,Out->Position.Y,Out->Position.Z,Up,Out->Velocity.X,Out->Velocity.Y,Out->Velocity.Z,
					O.bDrivingAllowed,O.Recovery.bActive,O.Recovery.ElapsedSeconds,Out->bWorker));
			}
		}
		TestEqual(TEXT("180 real Solver outputs"),Outputs,180); TestTrue(TEXT("Worker physics execution"),Last.bWorker);
		TestFalse(TEXT("Side/roof never receives ordinary driving"),InvalidDrive);
		if (Case==0) { TestTrue(TEXT("Upright throttle remains effective"),Last.Velocity.Size2D()>100); }
		if (Case>=3)
		{
			TestTrue(TEXT("Side/roof reaches upright region within bounded execution"),BestUp>.96f);
			TestFalse(TEXT("Execution cannot continue beyond two seconds"),Last.Observation.Recovery.bActive);
			TestTrue(TEXT("Recovery reaches low-angle and low-spin success region"),FirstSettledSeconds>0);
		}
		if (Case==3) { DefaultSideSeconds=FirstSettledSeconds; }
		if (Case==4) { DefaultRoofSeconds=FirstSettledSeconds; }
		if (Case>=5) { TestTrue(TEXT("Suggested DA response settles faster than original gains"),FirstSettledSeconds < (Case==7 ? DefaultRoofSeconds : DefaultSideSeconds)); }
		AddInfo(FString::Printf(TEXT("Recovery Solver Case=%d BestUpIn2s=%.6f FinalUp=%.6f FinalSpeed=%.3f Elapsed=%.3f SettledSeconds=%.3f"),Case,BestUp,Last.Rotation.GetUpVector().Z,Last.Velocity.Size(),Last.Observation.Recovery.ElapsedSeconds,FirstSettledSeconds));
		Solver->UnregisterAndFreeSimCallbackObject_External(Probe); Solver->AdvanceAndDispatch_External(1.f/60); Solver->WaitOnPendingTasks_External();
		World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	}
	const FString Path=FPaths::ProjectSavedDir()/TEXT("VehicleRecovery")/(FGuid::NewGuid().ToString()+TEXT("_solver.csv"));
	TestTrue(TEXT("Solver trace saved"),FFileHelper::SaveStringArrayToFile(Trace,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	AddInfo(TEXT("Recovery Solver trace: ")+Path);
	return true;
}
#endif
