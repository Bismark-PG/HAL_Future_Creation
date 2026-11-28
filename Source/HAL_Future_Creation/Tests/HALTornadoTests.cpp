#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALVehicleAbilityTestSupport.h"
#include "HALRoomTestSupport.h"
#include "../HALTornado.h"
#include "../HALTornadoAbility.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALImpactReceiverComponent.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../VehicleHealthComponent.h"
#include "../BasicBallActor.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include <limits>

struct FHALTornadoTestAccess
{
	static void Pose(ATestVehiclePawn& V)
	{
		auto* M = V.FindComponentByClass<UArcadeVehicleMovementComponent>(); M->bShouldRunPhysics.Store(true);
		M->PublishedStability.bValid = M->PublishedStability.bGrounded = M->PublishedStability.bDrivingAllowed = true;
		M->PublishedStability.Up = M->PublishedStability.GroundNormal = FVector::UpVector;
		auto Snapshot = FHALRoomTestAccess::Flush(V.FindComponentByClass<UHALMovementEffectBridge>());
		Snapshot.Stamp(nullptr,10,1.f/60); M->PublishedMovementStatus.Definition = M->PublishedMovementStatus.Value = Snapshot;
		M->PublishedMovementStatus.ServerFrame = 10; M->PublishedMovementStatus.bReady = true;
	}
	static void Age(AHALTornado& T, double Age) { T.State.StartServerTime = T.ServerTime()-Age; T.LastAge = Age; }
	static void Contact(AHALTornado& T) { T.Contacts(T.State.Ground,T.State.Ground,0); }
	static void Step(AHALTornado& T, float Delta) { T.LastAge -= Delta; T.Simulate(Delta); }
	static void ReadyAgain(AHALTornado& T, AActor* Target) { T.NextHit.Remove(Target); }
	static float Upward(const AHALTornado& T) { return T.LastAcceptedUpwardSpeed; }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALTornadoClockTest,"HAL.FutureCreation.Tornado.PhasesValidationAndSweptCylinder",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALTornadoClockTest::RunTest(const FString&)
{
	FHALTornadoRules R; TestTrue(TEXT("Default rules valid"),R.IsValid());
	TestEqual(TEXT("Formation is harmless"),R.Phase(0),EHALTornadoPhase::Gathering);
	TestEqual(TEXT("Activation boundary"),R.Phase(R.GatherSeconds),EHALTornadoPhase::Active);
	TestEqual(TEXT("Retirement boundary"),R.Phase(R.GatherSeconds+R.ActiveSeconds),EHALTornadoPhase::Dissipating);
	TestEqual(TEXT("Finished boundary"),R.Phase(R.TotalSeconds()),EHALTornadoPhase::Finished);
	R.Speed = std::numeric_limits<float>::quiet_NaN(); TestFalse(TEXT("Nonfinite motion rejected"),R.IsValid());
	float T = 0;
	TestTrue(TEXT("High-speed crossing cannot skip a stationary tornado"),AHALTornado::SweptContact(FVector(-1000,0,50),FVector(1000,0,50),100,200,T));
	TestTrue(TEXT("Earliest contact determines outward direction"),FMath::IsNearlyEqual(T,.45f,.001f));
	TestFalse(TEXT("Another floor is not hit"),AHALTornado::SweptContact(FVector(-1000,0,600),FVector(1000,0,600),100,200,T));
	TestTrue(TEXT("Persistent overlap remains eligible for a later pulse"),AHALTornado::SweptContact(FVector(0,0,50),FVector(0,0,50),100,200,T));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALTornadoTravelClockTest,"HAL.FutureCreation.Tornado.ClientGroundBetweenSnapshots",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALTornadoTravelClockTest::RunTest(const FString&)
{
	FHALTornadoState State;
	State.Rules.GatherSeconds = .5f; State.Rules.ActiveSeconds = 2; State.Rules.Speed = 800;
	State.StartServerTime = State.SampleServerTime = 10;
	const FVector Start(500, -1200, 35);
	State.Ground = Start;
	State.Direction = FVector::RightVector;
	const auto At = [&](double Seconds) { return Start + State.Direction * (State.Rules.Speed * Seconds); };
	TestTrue(TEXT("Gathering is stationary"), State.PresentationGroundAt(10.48).Equals(Start,.001));
	TestTrue(TEXT("First active frame only travels its active fraction"), State.PresentationGroundAt(10.51).Equals(At(.01),.001));
	State.SampleServerTime = 11; State.Ground = At(.5);
	const FVector BeforePacket = State.PresentationGroundAt(11.18);
	TestTrue(TEXT("Travel continues across a 180 ms gap"), BeforePacket.Equals(At(.68),.001));
	State.SampleServerTime = 11.20; State.Ground = At(.70);
	TestTrue(TEXT("A newer sample ahead of the display clock causes no jump"), State.PresentationGroundAt(11.18).Equals(BeforePacket,.001));
	TestTrue(TEXT("A long outage bounds unconfirmed travel"), State.PresentationGroundAt(12.4).Equals(State.PresentationGroundAt(12.0),.001));
	State.SampleServerTime = 12.4; State.Ground = At(1.9);
	TestTrue(TEXT("Natural retirement adds no extra travel"), State.PresentationGroundAt(13).Equals(At(2),.001));
	State.Ground = At(1.98) + FVector(0,0,17); State.SampleServerTime = 12.52;
	TestTrue(TEXT("The final swept height and endpoint win"), State.PresentationGroundAt(13).Equals(State.Ground,.001));
	State.bStopped = true; State.SampleServerTime = 11; State.Ground = At(.45);
	TestTrue(TEXT("A first observation after a wall stop stays at the wall"), State.PresentationGroundAt(11.3).Equals(State.Ground,.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALTornadoAuthorityTest,"HAL.FutureCreation.Tornado.AuthorityGASImpactBallsWallAndCleanup",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALTornadoAuthorityTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
	auto* W = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	auto* Ability = GetMutableDefault<UHALTornadoAbility>(); auto* Old = Ability->SkillConfig.Get();
	ON_SCOPE_EXIT { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); Ability->SkillConfig = Old; };
	auto* Config = NewObject<UHALTornadoConfig>(Ability); Config->Rules.Speed = 0; Ability->SkillConfig = Config;
	auto* Match = W->SpawnActor<AHALMatchGameState>(); W->SetGameState(Match); Match->SetMatchPhase(EHALMatchPhase::Playing);
	const auto Box = [&](FVector Point, FVector Extent)
	{
		auto* A = W->SpawnActor<AActor>(); auto* C = NewObject<UBoxComponent>(A); A->SetRootComponent(C);
		C->SetBoxExtent(Extent); C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); C->SetCollisionObjectType(ECC_WorldStatic);
		C->SetCollisionResponseToAllChannels(ECR_Block); C->SetWorldLocation(Point); C->RegisterComponent(); return C;
	};
	Box(FVector(0,0,-50),FVector(10000,10000,50)); int32 Id = 0;
	const auto Car = [&]()
	{
		auto* Class = LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_05.BP_VehiclePawn_Car_05_C"));
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* V = W->SpawnActor<ATestVehiclePawn>(Class,FVector(0,++Id*1200,60),FRotator::ZeroRotator,P);
		auto* D = FHALVehicleAbilityTestAccess::Configure(V,UHALTornadoAbility::StaticClass()); D->Combat.Version = EHALCombatConfigVersion::AttackSnapshotV1;
		D->Impact.Version = EHALImpactVersion::TieredV1; FHALVehicleAbilityTestAccess::RuntimeConfig(V,D->Abilities);
		V->PreInitializeComponents(); V->InitializeComponents(); V->PostInitializeComponents();
		auto* PC = W->SpawnActor<AHALPlayerController>(); PC->SetAsLocalPlayerController(); auto* PS = W->SpawnActor<AHALPlayerState>();
		PS->SetOwner(PC); PC->SetPlayerState(PS); PS->SetPlayerId(Id);
		PS->PreInitializeComponents(); PS->InitializeComponents(); PS->PostInitializeComponents(); PS->DispatchBeginPlay();
		PS->ChangeVehicleAvatar(V,1); V->DispatchBeginPlay(); FHALTornadoTestAccess::Pose(*V); return V;
	};
	auto* A = Car(); auto* B = Car(); auto* C = Car();
	auto* ASC = CastChecked<UHALAbilitySystemComponent>(A->GetAbilitySystemComponent());
	if (!TestTrue(TEXT("Normal GAS input creates a tornado"),ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary))) { return false; }
	AHALTornado* Wind = nullptr; for (TActorIterator<AHALTornado> It(W); It; ++It) { Wind = *It; }
	if (!TestNotNull(TEXT("Authority skill actor"),Wind)) { return false; }
	TestFalse(TEXT("Duplicate activation cannot add another tornado"),ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary));
	const FVector Ground = Wind->GetState().Ground; B->SetActorLocation(Ground+FVector(70,0,60));
	const auto* Health = B->FindComponentByClass<UVehicleHealthComponent>(); const float HP = Health->GetCurrentHP();
	FHALTornadoTestAccess::Contact(*Wind); TestEqual(TEXT("Formation has no hits"),Wind->GetVehicleHitCount(),0u);
	FHALTornadoTestAccess::Age(*Wind,.7); FHALTornadoTestAccess::Contact(*Wind);
	TestEqual(TEXT("Active contact accepted once"),Wind->GetVehicleHitCount(),1u);
	TestEqual(TEXT("No direct damage"),Health->GetCurrentHP(),HP);
	TestEqual(TEXT("Canonical heavy tier"),B->FindComponentByClass<UHALImpactReceiverComponent>()->GetLastImpact().EffectiveTier,EHALImpactTier::Heavy);
	TestTrue(TEXT("Accepted heavy response queues an upward launch through canonical movement"),FHALTornadoTestAccess::Upward(*Wind) > 0);
	FHALTornadoTestAccess::Contact(*Wind); TestEqual(TEXT("Repeated overlap is throttled"),Wind->GetVehicleHitCount(),1u);
	FHALTornadoTestAccess::ReadyAgain(*Wind,B); FHALTornadoTestAccess::Contact(*Wind);
	TestEqual(TEXT("Still overlapping can be hit after its deadline expires"),Wind->GetVehicleHitCount(),2u);
	auto* OtherASC = CastChecked<UHALAbilitySystemComponent>(C->GetAbilitySystemComponent());
	OtherASC->AddLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune); C->SetActorLocation(Ground+FVector(0,-70,60));
	FHALTornadoTestAccess::Contact(*Wind);
	TestEqual(TEXT("Existing impact immunity is respected"),C->FindComponentByClass<UHALImpactReceiverComponent>()->GetLastImpact().EffectiveTier,EHALImpactTier::None);
	const auto Ball = [&](FVector Offset)
	{
		auto* Class = LoadClass<ABasicBallActor>(nullptr,TEXT("/Game/Blueprints/Balls/BP_BasicBall.BP_BasicBall_C"));
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* V = W->SpawnActor<ABasicBallActor>(Class,Ground+Offset,FRotator::ZeroRotator,P); V->PreInitializeComponents(); V->DispatchBeginPlay(); return V;
	};
	auto* Free = Ball(FVector(-50,0,60)); auto* Shot = Ball(FVector(0,50,60));
	TestTrue(TEXT("Prepare real attack ball"),Shot->BeginControl(B) && Shot->LaunchFromControl(B,B,FVector(2000,0,0)));
	TestFalse(TEXT("No unqualified caller can invoke a disarm"),Shot->DeflectByTornado(*Wind,FVector(1000,0,0)));
	FHALTornadoTestAccess::Contact(*Wind);
	TestEqual(TEXT("Free ball becomes a Car05 attack"),Free->GetBallState(),EBasicBallState::Launched);
	TestEqual(TEXT("New free-ball shot belongs to caster"),Free->GetLaunchedBy(),static_cast<AActor*>(A));
	TestEqual(TEXT("Enemy attack ball is neutralized"),Shot->GetBallState(),EBasicBallState::Free);
	const auto State = Shot->GetBallRepState();
	TestTrue(TEXT("Disarm clears all attack identity and payload"),!State.LastLauncherPawn && State.LastLauncherPlayerId == INDEX_NONE && !State.AttackSnapshot.AttackId.IsValid());
	TestTrue(TEXT("Disarmed ball physically flies but cannot be instantly recaptured"),Shot->GetPhysicsRoot()->GetPhysicsLinearVelocity().Size() > 1000 && State.GlobalPickupLockEndServerTime > W->GetTimeSeconds());
	FHALTornadoTestAccess::ReadyAgain(*Wind,Shot); FHALTornadoTestAccess::Contact(*Wind);
	TestEqual(TEXT("A continuously resident disarmed ball is not immediately converted back into an attack"),Shot->GetBallState(),EBasicBallState::Free);
	ASC->ApplyStun(.4f,B); TestFalse(TEXT("Stun after release does not destroy the independent tornado"),Wind->IsActorBeingDestroyed());
	// A second source is a scoped wall-motion fixture, not another accepted GAS activation.
	auto* Moving = W->SpawnActor<AHALTornado>(); Moving->SetOwner(A); auto* Motion = NewObject<UHALTornadoConfig>();
	const FVector Origin = Ground+FVector(0,700,0); TestTrue(TEXT("Wall fixture starts"),Moving->Start(*Motion,Origin));
	Box(Origin+FVector(Motion->Rules.Radius+20,0,150),FVector(10,400,150));
	FHALTornadoTestAccess::Age(*Moving,.7); FHALTornadoTestAccess::Step(*Moving,.05f);
	TestTrue(TEXT("Wall stops motion without retiring gameplay"),Moving->GetState().bStopped && !Moving->IsActorBeingDestroyed());
	const FVector Stopped = Moving->GetState().Ground; FHALTornadoTestAccess::Step(*Moving,.05f);
	TestTrue(TEXT("Stationary vortex stays at its wall stop"),Moving->GetState().Ground.Equals(Stopped,.01));
	C->SetActorLocation(Origin+FVector(Motion->Rules.Radius+70,0,60)); FHALTornadoTestAccess::Contact(*Moving);
	TestEqual(TEXT("No attacks through the blocking wall"),Moving->GetVehicleHitCount(),0u);
	FHALTornadoTestAccess::Age(*Wind,Config->Rules.GatherSeconds+Config->Rules.ActiveSeconds+.1);
	FHALTornadoTestAccess::Step(*Wind,.01f); const auto Hits = Wind->GetVehicleHitCount(); FHALTornadoTestAccess::Contact(*Wind);
	TestEqual(TEXT("Visual retirement cannot keep attacking"),Wind->GetVehicleHitCount(),Hits);
	TestFalse(TEXT("Normal retirement preserves a visual tail"),Wind->IsActorBeingDestroyed());
	A->GetController()->UnPossess(); FHALTornadoTestAccess::Step(*Wind,.01f);
	TestTrue(TEXT("Avatar exit clears even the visual tail"),Wind->IsActorBeingDestroyed());
	return true;
}
#endif
