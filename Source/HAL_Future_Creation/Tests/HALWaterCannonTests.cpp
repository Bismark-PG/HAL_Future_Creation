#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALRoomTestSupport.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../BallControlComponent.h"
#include "../BasicBallActor.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALGameplayTags.h"
#include "../HALImpactReceiverComponent.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../HALWaterCannon.h"
#include "../HALWaterCannonAbility.h"
#include "../VehicleDefinition.h"
#include "../VehicleHealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Misc/ScopeExit.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Sound/SoundWave.h"

struct FHALWaterCannonTestAccess
{
	static void Pose(ATestVehiclePawn& Car)
	{
		auto* Move = Car.FindComponentByClass<UArcadeVehicleMovementComponent>();
		Move->bShouldRunPhysics.Store(true);
		auto& Observation = Move->PublishedStability;
		Observation.bValid = Observation.bGrounded = Observation.bDrivingAllowed = true;
		Observation.Up = Observation.GroundNormal = FVector::UpVector;
		auto Snapshot = FHALRoomTestAccess::Flush(Car.FindComponentByClass<UHALMovementEffectBridge>());
		Snapshot.Stamp(nullptr,10,1.f/60);
		Move->PublishedMovementStatus.Definition = Move->PublishedMovementStatus.Value = Snapshot;
		Move->PublishedMovementStatus.ServerFrame = 10;
		Move->PublishedMovementStatus.bReady = true;
	}
	static FTransform Frame(const AHALWaterCannon& Jet) { return Jet.Frame(false); }
	static void HeavyPulse(AHALWaterCannon& Jet, double Age)
	{
		auto* Original = Jet.Config.Get();
		auto* Heavy = DuplicateObject<UHALWaterCannonConfig>(Original,&Jet);
		Heavy->ImpactTier = EHALImpactTier::Heavy;
		Jet.Config = Heavy;
		Sample(Jet,Age);
		Jet.Config = Original;
	}
	static void Sample(AHALWaterCannon& Jet, double Age) { Jet.SampleJet(Jet.StartTime + Age); }
	static const FHALWaterJetView& View(const AHALWaterCannon& Jet) { return Jet.View; }
	static bool Capture(ATestVehiclePawn& Car, ABasicBallActor& Ball)
	{
		return Car.FindComponentByClass<UBallControlComponent>()->AcquireBall(&Ball);
	}
	static int32 Draw(AHALWaterCannon& Jet)
	{
		Jet.BuildWaterMesh(.3f);
		return Jet.Mesh->GetMesh()->TriangleCount();
	}
	static void Expire(AHALWaterCannon& Jet) { Jet.StartTime -= Jet.Config->Duration; Jet.Tick(.01f); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALWaterDriveTest,"HAL.FutureCreation.WaterCannon.PhysicsHistory",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALWaterDriveTest::RunTest(const FString&)
{
	FHALMovementSnapshot Sample;
	Sample.Epoch = FGuid::NewGuid(); Sample.Revision = 1;
	auto& Drive = Sample.WaterJet;
	Drive.Activation = FGuid::NewGuid(); Drive.Seconds = 3; Drive.RecoilAcceleration = 500;
	Drive.ForwardLimitScale = .55f; Drive.ReverseLimit = 1400;
	Sample.Stamp(nullptr,20,.02f);
	TestTrue(TEXT("Complete finite window"),Sample.IsValid() && Drive.IsActive(20) && Drive.IsActive(169) && !Drive.IsActive(170));
	auto Stun = Sample; Stun.Revision++; Stun.bControlsBlocked = true; Stun.Stamp(&Sample,30,.02f);
	TestTrue(TEXT("Stun leaves original physics interval intact"),Stun.WaterJet == Drive);
	TestEqual(TEXT("Neutral and forward receive full recoil"),Drive.RecoilAt(0,.02f),500.f);
	TestEqual(TEXT("Maximum reverse contributes no extra recoil"),Drive.RecoilAt(-1450,.02f),0.f);
	TestEqual(TEXT("Last reverse step cannot overshoot"),Drive.RecoilAt(-1398,.02f),100.f);
	TArray<uint8> Bytes; FMemoryWriter Writer(Bytes); Stun.Serialize(Writer);
	FMemoryReader Reader(Bytes); FHALMovementSnapshot Copy; Copy.Serialize(Reader);
	TestTrue(TEXT("Physics history round trip preserves recoil and control block"),Copy.IsValid() && Stun.Equals(Copy));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALWaterGameplayTest,"HAL.FutureCreation.WaterCannon.ProductionGrantHitsBallsAndOcclusion",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALWaterGameplayTest::RunTest(const FString&)
{
	using Access = FHALWaterCannonTestAccess;
	auto* CarClass = LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_09.BP_VehiclePawn_Car_09_C"));
	auto* BallClass = LoadClass<ABasicBallActor>(nullptr,TEXT("/Game/Blueprints/Balls/BP_BasicBall.BP_BasicBall_C"));
	if (!TestNotNull(TEXT("Production fire engine"),CarClass) || !TestNotNull(TEXT("Production ball"),BallClass)) { return false; }
	const auto* D = CarClass->GetDefaultObject<ATestVehiclePawn>()->GetVehicleDefinition();
	if (!D || !TestEqual(TEXT("One real grant"),D->Abilities.Grants.Num(),1)) { return false; }
	const auto* Config = Cast<UHALWaterCannonConfig>(D->Abilities.Grants[0].SkillConfig);
	if (!TestNotNull(TEXT("Single skill DA is bound on the car"),Config)) { return false; }
	TestTrue(TEXT("Production rules, flowing water and looping siren are bound"),Config->HasValidRules() && Config->WaterMaterial
		&& Cast<USoundWave>(Config->Siren) && Config->Siren->IsLooping());
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
	auto* W = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	ON_SCOPE_EXIT { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); };
	auto* Match = W->SpawnActor<AHALMatchGameState>(); W->SetGameState(Match); Match->SetMatchPhase(EHALMatchPhase::Playing);
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto Car = [&](int32 Id)
	{
		auto* V = W->SpawnActor<ATestVehiclePawn>(CarClass,FVector(0,Id*1400,100),FRotator::ZeroRotator,Spawn);
		V->PreInitializeComponents(); V->InitializeComponents(); V->PostInitializeComponents();
		auto* PC = W->SpawnActor<AHALPlayerController>(); PC->SetAsLocalPlayerController();
		auto* PS = W->SpawnActor<AHALPlayerState>(); PS->SetOwner(PC); PC->SetPlayerState(PS); PS->SetPlayerId(Id);
		PS->PreInitializeComponents(); PS->InitializeComponents(); PS->PostInitializeComponents(); PS->DispatchBeginPlay();
		PS->ChangeVehicleAvatar(V,1); V->DispatchBeginPlay(); Access::Pose(*V);
		return V;
	};
	auto* A = Car(1); auto* B = Car(2); auto* C = Car(3);
	auto* ASC = CastChecked<UHALAbilitySystemComponent>(A->GetAbilitySystemComponent());
	if (!TestTrue(TEXT("Production primary input starts fire engine"),ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary))) { return false; }
	AHALWaterCannon* Jet = nullptr;
	for (TActorIterator<AHALWaterCannon> It(W); It; ++It) { Jet = *It; }
	if (!TestNotNull(TEXT("Replicated jet spawned"),Jet)) { return false; }
	// The fixture creates its world without a game mode. Run the normal actor lifecycle so Destroy routes EndPlay.
	if (!Jet->IsActorInitialized())
	{
		Jet->PreInitializeComponents();
		Jet->InitializeComponents();
		Jet->PostInitializeComponents();
	}
	if (!Jet->HasActorBegunPlay()) { Jet->DispatchBeginPlay(); }
	const auto Aim = Access::Frame(*Jet);
	const FVector At = Aim.TransformPosition(FVector(400,0,0));
	auto Place = [](AActor& Actor,const FVector& P)
	{
		auto* Body = CastChecked<UPrimitiveComponent>(Actor.GetRootComponent());
		Actor.SetActorLocation(Actor.GetActorLocation() + P - Body->Bounds.Origin,false,nullptr,ETeleportType::TeleportPhysics);
	};
	auto NewBall = [&](const FVector& P)
	{
		auto* Ball = W->SpawnActor<ABasicBallActor>(BallClass,P,FRotator::ZeroRotator,Spawn);
		Ball->PreInitializeComponents(); Ball->DispatchBeginPlay(); Place(*Ball,P); return Ball;
	};
	auto* Free = NewBall(At);
	auto* Shot = NewBall(At + FVector(80,0,0));
	TestTrue(TEXT("Prime an enemy attack ball"),Shot->BeginControl(B) && Shot->LaunchFromControl(B,B,FVector(-1800,0,0)));
	const auto OldAttack = Shot->GetBallRepState().AttackSnapshot.AttackId;
	Access::Sample(*Jet,0);
	TestTrue(TEXT("Free ball gains offensive ownership"),Free->GetBallState() == EBasicBallState::Launched && Free->GetLaunchedBy() == A);
	TestTrue(TEXT("Enemy launched ball gets a new real attack"),Shot->GetLaunchedBy() == A && Shot->GetBallRepState().AttackSnapshot.AttackId != OldAttack);
	const auto Seq = Free->GetBallStateSequence(); Access::Sample(*Jet,.1);
	TestEqual(TEXT("Continuously exposed ball cannot reset its attack"),Free->GetBallStateSequence(),Seq);
	auto* Crossing = NewBall(At + FVector(0,-260,0));
	TestTrue(TEXT("Prime a fast crossing shot"),Crossing->BeginControl(B) && Crossing->LaunchFromControl(B,B,FVector(0,5000,0)));
	Access::Sample(*Jet,.11); Place(*Crossing,At+FVector(0,260,0)); Access::Sample(*Jet,.12);
	TestTrue(TEXT("Enemy shot crossing between samples is relaunched"),Crossing->GetLaunchedBy() == A);
	TestFalse(TEXT("Unconfirmed contacts cannot launch balls"),Free->LaunchFromWaterCannon(*Jet,FVector(1300,0,0)));
	auto* Held = NewBall(At - FVector(100,0,0));
	TestTrue(TEXT("An opponent controls the ball"),Access::Capture(*B,*Held));
	auto* Own = NewBall(At+FVector(24,0,0));
	TestTrue(TEXT("Own ball constraint engaged"),Access::Capture(*A,*Own));
	Access::Sample(*Jet,.15);
	TestTrue(TEXT("Water cannot steal a held ball"),Held->GetControlledBy() == B);
	TestTrue(TEXT("A hit on the caster's own ball launches it and releases its actual constraint"),Own->GetLaunchedBy() == A && !A->FindComponentByClass<UBallControlComponent>()->GetHeldBall());
	Place(*B,At); Place(*C,At+FVector(300,0,0));
	const auto* HP = B->FindComponentByClass<UVehicleHealthComponent>();
	const auto* RearHP = C->FindComponentByClass<UVehicleHealthComponent>();
	const float Before = HP->GetCurrentHP(), RearBefore = RearHP->GetCurrentHP();
	Access::Sample(*Jet,.20);
	const float First = HP->GetCurrentHP();
	TestTrue(TEXT("First touch damages and applies configured impact"),First < Before
		&& B->FindComponentByClass<UHALImpactReceiverComponent>()->GetLastImpact().EffectiveTier == Config->ImpactTier);
	TestEqual(TEXT("Front car occludes the aligned rear car"),RearHP->GetCurrentHP(),RearBefore);
	TestEqual(TEXT("Configured impact/drop threshold governs held ball"),Held->GetBallState(),
		Config->ImpactTier >= D->Impact.MinimumDropTier && D->Impact.bDropOnImpact ? EBasicBallState::Free : EBasicBallState::Controlled);
	TestTrue(TEXT("Visible clipped jet and contact splash geometry"),Access::View(*Jet).ContactMask != 0 && Access::Draw(*Jet) > (AHALWaterCannon::LaneCount-1)*16);
	Place(*B,At + FVector(0,500,0)); Access::Sample(*Jet,.26);
	TestTrue(TEXT("Separately exposed target is immediately eligible"),RearHP->GetCurrentHP() < RearBefore);
	Place(*B,At); Access::Sample(*Jet,.30);
	TestEqual(TEXT("Re-entering does not reset vehicle cooldown"),HP->GetCurrentHP(),First);
	ASC->ApplyStun(1,B); Access::Sample(*Jet,.20 + Config->HitInterval + .01);
	TestTrue(TEXT("Stunned caster continues applying pulses"),ASC->IsStunned() && HP->GetCurrentHP() < First && Access::View(*Jet).bFlowing);
	Access::HeavyPulse(*Jet,.22 + 2*Config->HitInterval);
	TestTrue(TEXT("A strong jet hit uses the normal drop and preserves Free on the drop frame"),
		Held->GetBallState() == EBasicBallState::Free && !Held->GetLaunchedBy() && !B->FindComponentByClass<UBallControlComponent>()->GetHeldBall());
	auto* Wall = W->SpawnActor<AActor>(); auto* Box = NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(8,500,400)); Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->SetWorldLocation(Aim.TransformPosition(FVector(150,0,0))); Box->RegisterComponent();
	const float BeforeWall = HP->GetCurrentHP();
	Access::Sample(*Jet,1.5);
	TestEqual(TEXT("Wall clips all lanes and prevents damage behind it"),HP->GetCurrentHP(),BeforeWall);
	TestTrue(TEXT("Visible range agrees with occlusion"),Access::View(*Jet).Reach[8] < 100);
	A->SetActorRotation(FRotator(0,0,180),ETeleportType::TeleportPhysics);
	Access::Sample(*Jet,2);
	TestFalse(TEXT("Upside-down car pauses jet"),Access::View(*Jet).bFlowing);
	A->SetActorRotation(FRotator::ZeroRotator,ETeleportType::TeleportPhysics);
	Access::Sample(*Jet,2.1);
	TestTrue(TEXT("Righted car resumes remaining interval"),Access::View(*Jet).bFlowing);
	// This fixture does not advance Chaos. Acknowledge the preceding revision before verifying release.
	Access::Pose(*A);
	Access::Expire(*Jet);
	TestTrue(TEXT("Expiry removes gameplay actor"),Jet->IsActorBeingDestroyed() || !IsValid(Jet));
	TestFalse(TEXT("Expiry clears replayable recoil"),FHALRoomTestAccess::Flush(A->FindComponentByClass<UHALMovementEffectBridge>()).WaterJet.Activation.IsValid());
	return true;
}
#endif
