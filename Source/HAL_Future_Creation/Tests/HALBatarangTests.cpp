#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../BasicBallActor.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALBatarang.h"
#include "../HALBatarangAbility.h"
#include "../HALGameplayTags.h"
#include "../HALImpactReceiverComponent.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../TestVehiclePawn.h"
#include "../VehicleDefinition.h"
#include "../VehicleHealthComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"

struct FHALBatarangTestAccess
{
	static void Advance(AHALBatarang& Dart,double Now) { Dart.Advance(Now); }
	static void Ground(ATestVehiclePawn& Car,bool bGround)
	{
		auto* Move = Car.FindComponentByClass<UArcadeVehicleMovementComponent>();
		Move->bShouldRunPhysics.Store(true);
		auto& Pose = Move->PublishedStability;
		Pose.bValid = true; Pose.bDrivingAllowed = Pose.bGrounded = bGround; Pose.Up = Pose.GroundNormal = FVector::UpVector;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALDartPathTest,"HAL.FutureCreation.Batarang.MirroredLoopAndFourLocalSides",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALDartPathTest::RunTest(const FString&)
{
	const auto Point = [](float T,int8 Sign) { return HALBatarang::Arc(T,1050,480,Sign); };
	TestTrue(TEXT("Leaves forward and ends behind the original muzzle"),Point(.01f,1).X>0 && Point(1,1).X < -400);
	TestTrue(TEXT("Wide lobes on both sides of the release axis"),Point(.2f,1).Y>320 && Point(.6f,1).Y < -400);
	for (int32 I=0; I<=32; ++I)
	{
		const FVector A=Point(I/32.f,1), B=Point(I/32.f,-1);
		TestTrue(TEXT("Opposite steering mirrors the whole arc"),A.Equals(FVector(B.X,-B.Y,0),.01));
	}
	const FVector Extent(170,78,30);
	for (const auto Side : {EHALDartSide::Front,EHALDartSide::Rear,EHALDartSide::Right,EHALDartSide::Left})
	{
		const FVector Out = -HALBatarang::PushDirection(Side,0);
		TestTrue(TEXT("Four physical contact faces match inward impulse"),HALBatarang::HitSide(Out*Extent,Extent,Out)==Side);
		TestTrue(TEXT("90 degree target turn rotates expected impulse"),HALBatarang::PushDirection(Side,90).Equals(FRotator(0,90,0).RotateVector(-Out),.001));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALDartCombatTest,"HAL.FutureCreation.Batarang.ProductionGrantFlightPinFuseAndCover",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALDartCombatTest::RunTest(const FString&)
{
	auto* Class = LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_12.BP_VehiclePawn_Car_12_C"));
	auto* BallClass = LoadClass<ABasicBallActor>(nullptr,TEXT("/Game/Blueprints/Balls/BP_BasicBall.BP_BasicBall_C"));
	if (!TestNotNull(TEXT("Production Car12"),Class) || !TestNotNull(TEXT("Physical ball"),BallClass)) { return false; }
	const auto* Def = Class->GetDefaultObject<ATestVehiclePawn>()->GetVehicleDefinition();
	if (!Def || !TestEqual(TEXT("Exactly one production grant"),Def->Abilities.Grants.Num(),1)) { return false; }
	auto* Config = Cast<UHALBatarangConfig>(Def->Abilities.Grants[0].SkillConfig);
	if (!TestNotNull(TEXT("Production dart DA"),Config)) { return false; }
	TestTrue(TEXT("Production tuning and capture signal"),Config->HasValidRules() && Config->PinSignal);
	if (!TestNotNull(TEXT("Production sculpted blade"),Config->BladeMesh.Get())) { return false; }
	TestTrue(TEXT("Authored blade has a finish and measurable wingspan"),Config->BladeMesh->GetMaterial(0)
		&& Config->BladeMesh->GetBounds().BoxExtent.Y > UE_SMALL_NUMBER);
	TestTrue(TEXT("Production wake and resonance are independently assigned"),Config->WakeMaterial && Config->ResonanceMaterial
		&& Config->WakeMaterial!=Config->ResonanceMaterial && Config->WakeSeconds>0);
	TestEqual(TEXT("Five seconds from actual pin"),Config->FuseSeconds,5.f);
	TestEqual(TEXT("Ten damage at this car's base attack"),Config->DamageMultiplier*Def->Combat.AttackPower,10.f,.001f);
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
	auto* W = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	ON_SCOPE_EXIT { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); };
	auto* Match = W->SpawnActor<AHALMatchGameState>(); W->SetGameState(Match); Match->SetMatchPhase(EHALMatchPhase::Playing);
	FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto SpawnCar = [&](int32 Id)
	{
		auto* V = W->SpawnActor<ATestVehiclePawn>(Class,FVector(0,Id*3500,140),FRotator::ZeroRotator,Params);
		V->PreInitializeComponents(); V->InitializeComponents(); V->PostInitializeComponents();
		auto* PC = W->SpawnActor<AHALPlayerController>(); PC->SetAsLocalPlayerController();
		auto* PS = W->SpawnActor<AHALPlayerState>(); PS->SetOwner(PC); PC->SetPlayerState(PS); PS->SetPlayerId(Id);
		PS->PreInitializeComponents(); PS->InitializeComponents(); PS->PostInitializeComponents(); PS->DispatchBeginPlay();
		PS->ChangeVehicleAvatar(V,1); V->DispatchBeginPlay(); FHALBatarangTestAccess::Ground(*V,true);
		return V;
	};
	auto* A = SpawnCar(1); auto* B = SpawnCar(2);
	auto* ASC = CastChecked<UHALAbilitySystemComponent>(A->GetAbilitySystemComponent());
	auto* HP = B->FindComponentByClass<UVehicleHealthComponent>();
	const float Full = HP->GetCurrentHP();
	auto Start = [&]() -> AHALBatarang*
	{
		ASC->GrantUltimateEnergy(100);
		if (!A->RequestPrimarySkill())
		{
			const auto* PS = A->GetPlayerState<AHALPlayerState>();
			AddError(FString::Printf(TEXT("Local primary route: local=%d configured=%d owner=%s ready=%d"),A->IsLocallyControlled(),
				A->IsConfigurationValid(),*GetNameSafe(PS),PS && PS->AreVehicleAbilitiesReady(A)));
		}
		auto* D = A->GetActiveBatarang();
		if (D && !D->HasActorBegunPlay())
		{
			if (!D->IsActorInitialized()) { D->PreInitializeComponents(); D->InitializeComponents(); D->PostInitializeComponents(); }
			D->DispatchBeginPlay();
		}
		return D;
	};
	auto Place = [](AActor& V,const FVector& Center)
	{
		auto* Body = CastChecked<UPrimitiveComponent>(V.GetRootComponent());
		V.SetActorLocation(V.GetActorLocation()+Center-Body->Bounds.Origin,false,nullptr,ETeleportType::TeleportPhysics);
	};
	auto Advance = [](AHALBatarang& D,double Seconds) { FHALBatarangTestAccess::Advance(D,D.GetState().PhaseStart+Seconds); };
	FHALBatarangTestAccess::Ground(*A,false); TestNull(TEXT("Airborne release denied"),Start());
	TestEqual(TEXT("Rejected release spends nothing"),ASC->GetUltimateEnergyView().Energy,100.f);
	FHALBatarangTestAccess::Ground(*A,true);
	auto* D = Start();
	if (!TestNotNull(TEXT("Primary starts a real dart"),D)) { return false; }
	TestFalse(TEXT("In-flight taps cannot detonate"),D->RequestDetonation());
	TestEqual(TEXT("One successful full charge"),ASC->GetUltimateEnergyView().Energy,0.f);
	auto* Ball = W->SpawnActor<ABasicBallActor>(BallClass,D->FlightPoint(.12f),FRotator::ZeroRotator,Params);
	Ball->PreInitializeComponents(); Ball->DispatchBeginPlay();
	auto* Obstacle = W->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Obstacle);
	Obstacle->SetRootComponent(Box); Box->SetBoxExtent(FVector(45)); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Overlap); Box->RegisterComponent(); Obstacle->SetActorLocation(D->FlightPoint(.28f));
	Place(*B,D->FlightPoint(.55f)); Advance(*D,Config->FlightSeconds);
	if (!TestTrue(TEXT("One long server step follows the curved path through ball and trigger"),D->GetState().Target==B
		&& D->GetState().Phase==EHALDartPhase::Pinned)) { return false; }
	TestEqual(TEXT("Pin does zero damage"),HP->GetCurrentHP(),Full);
	const FVector Before = D->PushDirection();
	B->SetActorRotation(FRotator(0,90,0),ETeleportType::TeleportPhysics);
	const FVector After = D->PushDirection();
	TestTrue(TEXT("Arrow rotates with target before detonation"),After.Equals(FRotator(0,90,0).RotateVector(Before),.01));
	FHALBatarangTestAccess::Ground(*A,false);
	A->RequestPrimarySkill();
	TestTrue(TEXT("Airborne second press explodes along the displayed direction"),D->GetState().Phase==EHALDartPhase::Burst && D->GetState().BurstDirection.Equals(After,.001));
	TestEqual(TEXT("Manual burst exactly 10 HP"),HP->GetCurrentHP(),Full-10);
	TestTrue(TEXT("Medium impact reached normal combat pipeline"),B->FindComponentByClass<UHALImpactReceiverComponent>()->GetLastImpact().EffectiveTier==EHALImpactTier::Medium);
	TestFalse(TEXT("Repeat and expiry cannot settle again"),D->RequestDetonation());
	Advance(*D,.55); TestEqual(TEXT("Only one damage event"),HP->GetCurrentHP(),Full-10);
	FHALBatarangTestAccess::Ground(*A,true);
	D=Start(); if (!TestNotNull(TEXT("New cast after old dart is released"),D)) { return false; }
	Box->SetCollisionResponseToAllChannels(ECR_Block); Place(*Obstacle,D->FlightPoint(.2f)); Place(*B,D->FlightPoint(.55f));
	Advance(*D,Config->FlightSeconds);
	TestTrue(TEXT("Blocking wall ends the cast before a vehicle"),!A->GetActiveBatarang());
	TestEqual(TEXT("Wall miss still costs the full charge"),ASC->GetUltimateEnergyView().Energy,0.f);
	Obstacle->Destroy();
	D=Start(); if (!TestNotNull(TEXT("Timed cast"),D)) { return false; }
	Place(*B,D->FlightPoint(.3f)); Advance(*D,Config->FlightSeconds);
	if (!TestTrue(TEXT("Second physical pin"),D->GetState().Phase==EHALDartPhase::Pinned)) { return false; }
	ASC->ApplyStun(6.f,B); TestFalse(TEXT("Stun blocks manual detonation"),D->RequestDetonation());
	Advance(*D,4.99); TestEqual(TEXT("No early expiry"),HP->GetCurrentHP(),Full-10);
	Advance(*D,5.01); TestEqual(TEXT("Automatic burst still happens during stun"),HP->GetCurrentHP(),Full-20);
	Advance(*D,.55);
	auto* C = SpawnCar(3);
	auto* E = SpawnCar(4);
	// Two different casters pin the same moving target. Cleaning up one cast must leave the other intact.
	for (auto* Caster : {C,E})
	{
		Caster->RequestPrimarySkill(); auto* Other = Caster->GetActiveBatarang();
		if (!TestNotNull(TEXT("A separate caster owns its own dart"),Other)) { return false; }
		if (!Other->IsActorInitialized()) { Other->PreInitializeComponents(); Other->InitializeComponents(); Other->PostInitializeComponents(); }
		if (!Other->HasActorBegunPlay()) { Other->DispatchBeginPlay(); }
		Place(*B,Other->FlightPoint(.29f)); Advance(*Other,Config->FlightSeconds);
		TestTrue(TEXT("Another existing mark does not exclude the target"),Other->GetState().Target==B);
	}
	const auto Remaining = TWeakObjectPtr<AHALBatarang>(E->GetActiveBatarang());
	C->GetController()->UnPossess();
	if (auto* Gone = C->GetActiveBatarang()) { Advance(*Gone,5.1); }
	if (!TestTrue(TEXT("Caster exit retires only that caster's dart"),!IsValid(C->GetActiveBatarang()) && Remaining.IsValid())) { return false; }
	Match->SetMatchPhase(EHALMatchPhase::WaitingForPlayers); Advance(*Remaining.Get(),5.1);
	TestFalse(TEXT("Round end cancels a pending automatic burst"),Remaining.IsValid());
	TestEqual(TEXT("Cleanup causes no late damage"),HP->GetCurrentHP(),Full-20);
	return true;
}
#endif
