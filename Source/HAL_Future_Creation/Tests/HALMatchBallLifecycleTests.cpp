#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "../HALMatchBallLifecycle.h"
#include "../HALSpecialBallPad.h"
#include "../HALMatchFeverDefinition.h"
#include "../HALMatchGameState.h"
#include "../BasicBallActor.h"
#include "../HALSpecialBallCue.h"
#include "../HALBlackHoleBall.h"
#include "../BallControlComponent.h"
#include "../TestVehiclePawn.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "Components/BoxComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "AssetCompilingManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ScopeExit.h"

namespace
{
	const FHALMatchBallArena* ProductionBallArena()
	{
		return GetDefault<UHALMatchBallSettings>()->Arenas.FindByPredicate([](const FHALMatchBallArena& Arena)
		{ return Arena.MatchMap.ToSoftObjectPath().GetLongPackageName() == TEXT("/Game/Maps/MVPB_Match"); });
	}

	struct FMatchBallWorld
	{
		UWorld* World;
		FMatchBallWorld()
		{
			const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL(), false);
			World->SetBegunPlay(true);
		}
		~FMatchBallWorld()
		{
			World->EndPlay(EEndPlayReason::Quit);
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
		AActor* Block(FVector Center, FVector Extent)
		{
			auto* Actor = World->SpawnActor<AActor>();
			auto* Body = NewObject<UBoxComponent>(Actor);
			Actor->AddInstanceComponent(Body);
			Actor->SetRootComponent(Body);
			Body->SetBoxExtent(Extent);
			Body->SetCollisionProfileName(TEXT("BlockAll"));
			Body->SetWorldLocation(Center);
			Body->RegisterComponent();
			return Actor;
		}
		int32 PrimaryCount() const
		{
			int32 Count = 0;
			for (TActorIterator<ABasicBallActor> It(World); It; ++It) { Count += It->IsPrimaryMatchBall(); }
			return Count;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSpecialBallSupplyTest, "HAL.FutureCreation.SpecialBallSupply.SharedClockBlockingAndBag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALSpecialBallSupplyTest::RunTest(const FString&)
{
	FMatchBallWorld F;
	UWorld* World = F.World;
	auto* State = World->SpawnActor<AHALMatchGameState>();
	World->SetGameState(State);
	F.Block(FVector(0,0,-20), FVector(4000,4000,20));
	auto* First = World->SpawnActor<AHALSpecialBallPad>(FVector(700,0,0), FRotator::ZeroRotator);
	auto* Second = World->SpawnActor<AHALSpecialBallPad>(FVector(-700,0,0), FRotator::ZeroRotator);
	Second->RotationOrder = 1;
	auto* Supply = State->FindComponentByClass<UHALSpecialBallSupply>();
	auto* Stock = GetDefault<UHALMatchBallSettings>()->SpecialBallSupply.LoadSynchronous();
	if (!TestNotNull(TEXT("Formal supply DA loads"), Stock) || !TestNotNull(TEXT("Production mesh"), Stock->PadMesh.Get())) { return false; }
	auto Balls = [&]()
	{
		TArray<AHALSpecialBall*> Found;
		for (TActorIterator<AHALSpecialBall> It(World); It; ++It) { Found.Add(*It); }
		return Found;
	};
	auto Obstruct = [&](AHALSpecialBallPad* Pad) { return F.Block(Pad->GetActorLocation() + FVector(0,0,95), FVector(99,80,55)); };
	TestFalse(TEXT("No supply timer in Waiting"), World->GetTimerManager().TimerExists(Supply->Timer));
	State->SetMatchPhase(EHALMatchPhase::Playing);
	TestEqual(TEXT("One shared first warning delay"), World->GetTimerManager().GetTimerRemaining(Supply->Timer), Stock->FirstDropSeconds-Stock->WarningSeconds, .01f);
	auto* Fever = NewObject<UHALMatchFeverDefinition>();
	Fever->SpecialBallIntervalMultiplier = .5f;
	Supply->ScheduleCycle(20.f, 0.f, .5f);
	Supply->ApplyFever(Fever);
	TestEqual(TEXT("Changing pace halfway through an unannounced wait preserves its fraction"),
		World->GetTimerManager().GetTimerRemaining(Supply->Timer), (10.f - Stock->WarningSeconds) * .5f, .01f);
	Supply->ApplyFever(nullptr);
	// Exercise two complete bags even if a designer narrows the production pool.
	Supply->Settings = DuplicateObject<UHALSpecialBallSupplyDefinition>(Stock, Supply);
	Supply->Settings->BallPool = {EHALPadBall::Explosive, EHALPadBall::BlackHole, EHALPadBall::Meteor, EHALPadBall::TimedBomb};
	const int32 BagSize = Supply->Settings->BallPool.Num();
	bool bCarried = false;
	TSet<EHALPadBall> InBag;
	EHALPadBall Previous = EHALPadBall::None;
	for (int32 Drop = 0; Drop < 2*BagSize; ++Drop)
	{
		Supply->BeginCycle();
		auto* Pad = Drop % 2 ? Second : First;
		const EHALPadBall Kind = Pad->GetWarning().Ball;
		if (!TestTrue(TEXT("Only the next alternating pad announces"), Kind != EHALPadBall::None
			&& (Drop%2 ? First : Second)->GetWarning().Ball == EHALPadBall::None)) { return false; }
		if (!(Drop%BagSize)) { InBag.Reset(); }
		TestTrue(TEXT("No repeat inside bag or across boundary"), !InBag.Contains(Kind) && Kind != Previous);
		InBag.Add(Kind); Previous = Kind;
		if (Drop == 0)
		{
			const float AnnouncedTime = World->GetTimerManager().GetTimerRemaining(Supply->Timer);
			Supply->ApplyFever(Fever);
			TestEqual(TEXT("Fever keeps the entire warning after announcement"), World->GetTimerManager().GetTimerRemaining(Supply->Timer), AnnouncedTime);
			TestTrue(TEXT("Announced ball and location are stable"), Supply->AnnouncedPad.Get()==Pad && Pad->GetWarning().Ball==Kind);
			Supply->ApplyFever(nullptr);
			auto* CarSizedBlock = Obstruct(Pad);
			Supply->Deliver();
			TestTrue(TEXT("Arriving after warning blocks the announced site; no hidden handoff"),
				Balls().IsEmpty() && First->GetWarning().Ball == Kind && Second->GetWarning().Ball == EHALPadBall::None);
			CarSizedBlock->Destroy();
		}
		Supply->Deliver();
		const auto Current = Balls();
		if (!TestEqual(TEXT("Every delivery adds a ball even when several remain alive"), Current.Num(), Drop+1)) { return false; }
		TestEqual(TEXT("Shared next warning delay after success"), World->GetTimerManager().GetTimerRemaining(Supply->Timer),
			Stock->DropInterval-Stock->WarningSeconds, .01f);
		auto* const* NewBall = Current.FindByPredicate([Pad](auto* Ball) { return FVector::Dist2D(Ball->GetActorLocation(), Pad->GetActorLocation()) < 1.; });
		if (!TestNotNull(TEXT("Ball is at announced center"), NewBall)) { return false; }
		TestEqual(TEXT("Fresh sphere bottom clears traced floor by configured amount"), (*NewBall)->GetActorLocation().Z,
			static_cast<double>((*NewBall)->GetPhysicsRoot()->GetScaledSphereRadius() + Stock->SpawnClearance), .1);
		TestTrue(TEXT("Available Free without changing primary identity"), (*NewBall)->GetBallState() == EBasicBallState::Free && !(*NewBall)->IsPrimaryMatchBall());
		FVector Center;
		TestFalse(TEXT("A ball occupying the pad blocks a second spawn there"), Pad->FindSpawnCenter(Supply->SpawnRadius, Stock->SpawnClearance, Center));
		(*NewBall)->SetActorLocation(FVector(2000, 350.*Drop, 150), false, nullptr, ETeleportType::TeleportPhysics);
		if (!bCarried && (*NewBall)->AllowsAutomaticPickup())
		{ bCarried = (*NewBall)->BeginControl(World->SpawnActor<AActor>()); TestTrue(TEXT("Carrying an earlier delivery does not stop the next turns"), bCarried); }
	}
	auto* BlockA = Obstruct(First);
	auto* BlockB = Obstruct(Second);
	Supply->BeginCycle();
	TestTrue(TEXT("Two occupied pads stay quiet"), !Supply->AnnouncedPad.IsValid());
	const auto Reserved = Supply->Bag;
	World->TimeSeconds += Stock->BlockedGrace + 1.f;
	Supply->Announce();
	TestTrue(TEXT("Blocked turn consumes no ball or bag entry"), Balls().Num() == 2*BagSize && Supply->Bag == Reserved);
	BlockA->Destroy();
	Supply->BeginCycle();
	TestTrue(TEXT("Clear alternate selected before warning"), Supply->AnnouncedPad.Get() == First);
	BlockB->Destroy();
	Obstruct(First);
	World->TimeSeconds += Stock->BlockedGrace + Stock->WarningSeconds + 1.f;
	Supply->Deliver();
	TestTrue(TEXT("Persistent obstruction after warning skips without consuming type"), Balls().Num() == 2*BagSize && Supply->Bag == Reserved && !Supply->AnnouncedPad.IsValid());
	Supply->BeginCycle();
	State->SetMatchPhase(EHALMatchPhase::Finished);
	TestTrue(TEXT("Round end cancels pending warning, timers and special balls"), First->GetWarning().Ball == EHALPadBall::None
		&& Second->GetWarning().Ball == EHALPadBall::None && !World->GetTimerManager().TimerExists(Supply->Timer) && Balls().IsEmpty());
	State->SetMatchPhase(EHALMatchPhase::Playing);
	TestTrue(TEXT("Next round starts fresh"), Supply->Bag.IsEmpty() && Supply->NextPad == 0 && Supply->LastDelivered == EHALPadBall::None);
	Supply->Settings = DuplicateObject<UHALSpecialBallSupplyDefinition>(Stock, Supply);
	Supply->Settings->BallPool = {EHALPadBall::Meteor, EHALPadBall::Meteor};
	for (int32 Drop=0; Drop<2; ++Drop)
	{
		Supply->BeginCycle();
		if (!TestTrue(TEXT("A single distinct type can repeat, using the free pad"), Second->GetWarning().Ball == EHALPadBall::Meteor && Supply->Bag.Num()==1)) { return false; }
		Supply->Deliver();
		for (auto* Ball : Balls()) { Ball->Destroy(); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMatchBallGroundRollTest, "HAL.FutureCreation.MatchBall.GroundedLaunchRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALMatchBallGroundRollTest::RunTest(const FString&)
{
	FMatchBallWorld F;
	const auto* Arena = ProductionBallArena();
	UClass* Class = Arena ? Arena->PrimaryBallClass.LoadSynchronous() : nullptr;
	if (!TestNotNull(TEXT("Production ball class"), Class)) { return false; }
	auto* Ball = F.World->SpawnActorDeferred<ABasicBallActor>(Class, FTransform::Identity);
	auto* Type = DuplicateObject<UBallDefinition>(Class->GetDefaultObject<ABasicBallActor>()->Definition, Ball);
	Type->Gameplay.State.GroundRollReleaseSpeed = 1000;
	Type->Gameplay.State.GroundRollRequiredDuration = .15f;
	Type->Gameplay.Acquisition.GroundRollPickupLockDuration = .15f;
	Type->Gameplay.Damage.Version = EHALCombatConfigVersion::LegacyFixedDamage;
	Ball->Definition = Type;
	Ball->FinishSpawning(FTransform::Identity);
	AActor* Shooter = F.World->SpawnActor<AActor>();
	AActor* NextHolder = F.World->SpawnActor<AActor>();
	const float Radius = Ball->GetPhysicsRoot()->GetScaledSphereRadius();
	F.Block(FVector(0,0,-25), FVector(1000,1000,25));
	F.Block(FVector(500,0,500), FVector(20,200,200));
	auto* Trigger = CastChecked<UBoxComponent>(F.Block(FVector(-500,0,400), FVector(180,180,15))->GetRootComponent());
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	auto Sample = [&](FVector Point, float Speed, double Step = .05)
	{
		F.World->TimeSeconds += Step;
		Ball->SetActorLocation(Point, false, nullptr, ETeleportType::TeleportPhysics);
		Ball->PhysicsRoot->SetPhysicsLinearVelocity(FVector(Speed,0,0));
		Ball->CheckLaunchedLowSpeed();
	};
	const FVector Floor(0,0,Radius);
	const FVector Slow(650,0,0);
	if (!TestTrue(TEXT("Formal shot"), Ball->BeginControl(Shooter) && Ball->LaunchFromControl(Shooter,Shooter,Slow))) { return false; }
	const FGuid Attack = Ball->GetBallRepState().AttackSnapshot.AttackId;
	for (const FVector Point : {FVector(0,0,500), FVector(480-Radius,0,500), FVector(-500,0,415+Radius)})
	{
		Sample(Point, Slow.X); Sample(Point, Slow.X, .45);
		TestTrue(TEXT("Air, wall-only and query-only surfaces keep flight active"), Ball->GetBallRepState().AttackSnapshot.AttackId == Attack);
	}
	Sample(Floor, 1600); Sample(Floor, 1600, .30);
	TestTrue(TEXT("Fast ground shot keeps its original attack"), Ball->GetBallRepState().AttackSnapshot.AttackId == Attack);
	Sample(Floor, Slow.X); Sample(Floor, Slow.X, .09); Sample(Floor + FVector(0,0,200), Slow.X);
	Sample(Floor, Slow.X); Sample(Floor, Slow.X, .08);
	TestTrue(TEXT("A bounce resets continuous support time"), Ball->GetBallState() == EBasicBallState::Launched);
	Sample(Floor, Slow.X, .08);
	TestTrue(TEXT("Ground roll ends the attack and clears its attribution"), Ball->GetBallState() == EBasicBallState::Free
		&& !Ball->GetLaunchedBy() && !Ball->GetBallRepState().AttackSnapshot.AttackId.IsValid());
	TestFalse(TEXT("The brief pickup lock applies to every car"), Ball->CanBeControlledBy(NextHolder));
	F.World->TimeSeconds += .16;
	if (!TestTrue(TEXT("Another holder can pick up and relaunch"), Ball->BeginControl(NextHolder) && Ball->LaunchFromControl(NextHolder, NextHolder, Slow))) { return false; }
	const FQuat Tilt(FVector::RightVector, FMath::DegreesToRadians(30.f));
	const FVector RampCenter(2700,0,200);
	auto* Ramp = F.Block(RampCenter, FVector(400,250,20));
	Ramp->SetActorRotation(Tilt, ETeleportType::TeleportPhysics);
	const FVector OnRamp = RampCenter + Tilt.GetUpVector() * (20.f + Radius);
	Sample(OnRamp, Slow.X); Sample(OnRamp, Slow.X, .16);
	TestTrue(TEXT("A supported slow shot on a ramp also becomes Free"), Ball->GetBallState() == EBasicBallState::Free);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMatchBallMapTest, "HAL.FutureCreation.MatchBall.ProductionMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALMatchBallMapTest::RunTest(const FString&)
{
	const auto* Rules = ProductionBallArena();
	if (!TestNotNull(TEXT("Competition map is configured"), Rules) || !TestTrue(TEXT("Arena rules valid"), Rules->IsValid())) { return false; }
	auto* Map = Rules->MatchMap.LoadSynchronous();
	UClass* BallClass = Rules->PrimaryBallClass.LoadSynchronous();
	if (!TestNotNull(TEXT("Saved map loads"), Map) || !TestNotNull(TEXT("Production ball class loads"), BallClass)) { return false; }
	int32 Count = 0;
	for (AActor* Actor : Map->PersistentLevel->Actors)
	{
		auto* Ball = Cast<ABasicBallActor>(Actor);
		if (!IsValid(Ball) || !Ball->IsPrimaryMatchBall()) { continue; }
		++Count;
		TestTrue(TEXT("Saved primary matches respawn Blueprint"), Ball->GetClass() == BallClass);
		TestTrue(TEXT("Original kickoff point inside bounds"), Rules->Contains(Ball->GetActorLocation()));
		TestEqual(TEXT("Respawn matches authored primary radius"), Ball->GetConfiguredSphereRadius(), BallClass->GetDefaultObject<ABasicBallActor>()->GetConfiguredSphereRadius());
		const auto* Definition = FindFProperty<FObjectProperty>(BallClass, TEXT("Definition"));
		TestTrue(TEXT("Primary uses its Blueprint's ball DA"), Definition && Definition->Identical_InContainer(Ball, BallClass->GetDefaultObject()));
	}
	TestEqual(TEXT("Exactly one authored primary in the competition map"), Count, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMatchBallLifecycleTest, "HAL.FutureCreation.MatchBall.RecoveryAndSafeReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALMatchBallLifecycleTest::RunTest(const FString&)
{
	const auto* Production = ProductionBallArena();
	if (!TestNotNull(TEXT("Production defaults available"), Production)) { return false; }
	FHALMatchBallArena Rules = *Production;
	Rules.FloorCenter = FVector::ZeroVector;
	Rules.PlayableRadius = 2000;
	UClass* Class = Rules.PrimaryBallClass.LoadSynchronous();
	if (!TestNotNull(TEXT("Primary class"), Class)) { return false; }
	FMatchBallWorld F;
	auto* W = F.World;
	auto* State = W->SpawnActor<AHALMatchGameState>();
	W->SetGameState(State);
	auto* Life = State->FindComponentByClass<UHALMatchBallLifecycle>();
	auto* Floor = F.Block(FVector(0, 0, -20), FVector(2000, 2000, 20));
	const FVector Home(200, 300, 90);
	auto* Original = W->SpawnActor<ABasicBallActor>(Class, Home, FRotator::ZeroRotator);
	auto* Extra = W->SpawnActor<ABasicBallActor>(Class, FVector(1200, 0, 60), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Ordinary ball"), Original) || !TestNotNull(TEXT("Extra ball"), Extra)) { return false; }
	Original->SetPrimaryMatchBall(true);
	Life->PrepareArena(Rules);
	State->SetMatchPhase(EHALMatchPhase::Playing);
	auto Pump = [&](float Seconds) { W->TimeSeconds += Seconds; Life->CheckPrimaryBall(); };
	Pump(4.f);
	TestTrue(TEXT("Stationary ball remains the original"), Life->PrimaryBall.Get() == Original);
	auto* DriverClass = LoadClass<ATestVehiclePawn>(nullptr, TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
	if (!TestNotNull(TEXT("Saved vehicle class"), DriverClass)) { return false; }
	auto* PC = W->SpawnActor<AHALPlayerController>();
	PC->SetPlayerState(W->SpawnActor<AHALPlayerState>());
	auto* Driver = W->SpawnActor<ATestVehiclePawn>(DriverClass, FVector(-850, 0, 200), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Vehicle spawned"), Driver)) { return false; }
	PC->Possess(Driver);
	auto* Control = Driver->FindComponentByClass<UBallControlComponent>();
	if (!TestTrue(TEXT("Normal constraint acquires primary"), Control->AcquireBall(Original))) { return false; }
	Pump(2.f);
	TestTrue(TEXT("Holding preserves ball and constraint"), Original->GetControlledBy() == Driver && Control->GetHeldBall() == Original);
	FVector Recoil;
	if (!TestTrue(TEXT("Normal active launch"), Control->LaunchHeldBall(Recoil))) { return false; }
	Original->SetActorLocation(FVector(0, 0, 5000), false, nullptr, ETeleportType::TeleportPhysics);
	Pump(2.f);
	TestTrue(TEXT("In-bounds airborne launch keeps its attack"), Original->GetBallState() == EBasicBallState::Launched && Life->PrimaryBall.Get() == Original);
	Original->SetActorLocation(FVector(2300, 0, 100), false, nullptr, ETeleportType::TeleportPhysics);
	Pump(.1f);
	Original->SetActorLocation(Home, false, nullptr, ETeleportType::TeleportPhysics);
	Pump(.1f);
	TestTrue(TEXT("Brief excursion clears grace"), Life->PrimaryBall.Get() == Original && !Life->IsReturnAnnounced());
	Original->SetActorLocation(FVector(0, 0, -500), false, nullptr, ETeleportType::TeleportPhysics);
	Pump(.1f); Pump(Rules.OutsideGraceSeconds + .1f);
	if (!TestTrue(TEXT("Confirmed fall announces a return"), Life->IsReturnAnnounced() && F.PrimaryCount() == 0)) { return false; }
	TestTrue(TEXT("Visible ground ring has a visible owner and no collision"), Life->WarningRing && Life->WarningRing->IsVisible()
		&& !Life->WarningRing->GetOwner()->IsHidden() && Life->WarningRing->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	const FVector FirstGround = Life->GetReturnGround();
	TestTrue(TEXT("Uses original authored XY"), FVector::Dist2D(Home, FirstGround) < 1.);
	auto* Obstacle = F.Block(FirstGround + FVector(0, 0, 90), FVector(130, 130, 90));
	Pump(Rules.ReturnWarningSeconds + .1f);
	TestTrue(TEXT("Newly occupied landing site moves and repeats its warning"), Life->IsReturnAnnounced()
		&& FVector::Dist2D(FirstGround, Life->GetReturnGround()) > 100 && F.PrimaryCount() == 0);
	Pump(Rules.ReturnWarningSeconds * .5f);
	TestEqual(TEXT("No early substitute"), F.PrimaryCount(), 0);
	Pump(Rules.ReturnWarningSeconds);
	auto* Replacement = Life->PrimaryBall.Get();
	if (!TestNotNull(TEXT("Replacement created"), Replacement)) { return false; }
	TestTrue(TEXT("Fresh Free ball without stale attack, focus restored"), Replacement->GetClass() == Class
		&& Replacement->GetBallState() == EBasicBallState::Free && !Replacement->GetControlledBy() && !Replacement->GetLaunchedBy()
		&& !Replacement->GetBallRepState().AttackSnapshot.AttackId.IsValid() && Replacement->IsPrimaryCameraTarget());
	Pump(3.f);
	TestEqual(TEXT("Repeated polls keep exactly one primary"), F.PrimaryCount(), 1);
	TestTrue(TEXT("Extra ball stays independent"), IsValid(Extra) && !Extra->IsPrimaryMatchBall());
	if (!TestTrue(TEXT("Replacement capturable"), Control->AcquireBall(Replacement))) { return false; }
	Replacement->Destroy();
	TestTrue(TEXT("External destruction immediately releases holder and constraint"), !Control->GetHeldBall() && !Control->bConstraintActive);
	Obstacle->Destroy();
	Floor->Destroy();
	Pump(.1f); Pump(3.f);
	TestTrue(TEXT("Unavailable floor defers safely"), !Life->IsReturnAnnounced() && F.PrimaryCount() == 0);
	F.Block(FVector(0, 0, -20), FVector(2000, 2000, 20));
	Pump(.6f);
	TestTrue(TEXT("Recovery retries once ground is available"), Life->IsReturnAnnounced());
	State->SetMatchPhase(EHALMatchPhase::Finished);
	Pump(5.f);
	TestTrue(TEXT("Results cancel outstanding return"), !Life->IsReturnAnnounced() && F.PrimaryCount() == 0 && IsValid(Extra));
	// An arena missing its kickoff actor must recover even with another ball and a permanently blocked center.
	FMatchBallWorld Empty;
	auto* EmptyState = Empty.World->SpawnActor<AHALMatchGameState>();
	Empty.World->SetGameState(EmptyState);
	Empty.Block(FVector(0, 0, -20), FVector(2000, 2000, 20));
	Empty.Block(FVector(0, 0, 200), FVector(920, 920, 200));
	auto* Unmarked = Empty.World->SpawnActor<ABasicBallActor>(Class, FVector(1900, 0, 70), FRotator::ZeroRotator);
	auto* Fresh = EmptyState->FindComponentByClass<UHALMatchBallLifecycle>();
	Fresh->PrepareArena(Rules);
	TestFalse(TEXT("Waiting does not generate a primary"), Fresh->IsReturnAnnounced());
	EmptyState->SetMatchPhase(EHALMatchPhase::Playing);
	TestTrue(TEXT("Remote open floor gets a warning beyond the blocked center"), Fresh->IsReturnAnnounced()
		&& FVector::Dist2D(FVector::ZeroVector, Fresh->GetReturnGround()) > 920.);
	Empty.World->TimeSeconds += 2.;
	Fresh->CheckPrimaryBall();
	TestTrue(TEXT("Exactly one primary while the extra remains unmarked"), Empty.PrimaryCount() == 1
		&& IsValid(Unmarked) && !Unmarked->IsPrimaryMatchBall() && Fresh->PrimaryBall.Get() != Unmarked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMatchBallCueRenderTest, "HAL.RenderSmoke.MatchBall.GroundWarning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALMatchBallCueRenderTest::RunTest(const FString&)
{
	if (!FApp::CanEverRender()) { AddError(TEXT("Actual RHI required.")); return false; }
	FMatchBallWorld Fixture;
	auto* W = Fixture.World;
	auto* State = W->SpawnActor<AHALMatchGameState>();
	auto* Life = State->FindComponentByClass<UHALMatchBallLifecycle>();
	auto* GroundActor = W->SpawnActor<AActor>();
	auto* Deck = NewObject<UStaticMeshComponent>(GroundActor);
	GroundActor->SetRootComponent(Deck);
	Deck->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Deck->SetWorldScale3D(FVector(8, 8, .1));
	Deck->SetWorldLocation(FVector(0, 0, -5));
	Deck->RegisterComponent();
	auto* LightActor = W->SpawnActor<AActor>();
	auto* Light = NewObject<UDirectionalLightComponent>(LightActor);
	LightActor->SetRootComponent(Light);
	Light->SetIntensity(6.f);
	Light->SetWorldRotation(FRotator(-70, -25, 0));
	Light->RegisterComponent();
	auto* Camera = W->SpawnActor<ASceneCapture2D>()->GetCaptureComponent2D();
	Camera->bCaptureEveryFrame = false;
	Camera->ProjectionType = ECameraProjectionMode::Orthographic;
	Camera->OrthoWidth = 420.f;
	Camera->CaptureSource = SCS_FinalColorLDR;
	Camera->ShowFlags.SetTemporalAA(false);
	Camera->ShowFlags.SetBloom(false);
	Camera->ShowFlags.SetLensFlares(false);
	Camera->ShowFlags.SetMotionBlur(false);
	Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	Camera->SetWorldLocation(FVector(0, 0, 700));
	Camera->SetWorldRotation(FRotator(-90, 0, 0));
	auto* Target = NewObject<UTextureRenderTarget2D>();
	Target->InitCustomFormat(256, 256, PF_B8G8R8A8, false);
	Camera->TextureTarget = Target;
	auto Sample = [&]()
	{
		W->SendAllEndOfFrameUpdates();
		FlushRenderingCommands();
		Camera->CaptureScene();
		TArray<FColor> Pixels;
		Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
		return Pixels;
	};
	Life->SetReturnCue(true, FVector::ZeroVector);
	FAssetCompilingManager::Get().FinishAllCompilation();
	const auto Lit = Sample();
	// Check the silhouette against its actual material pixels, independently of the post-process chain.
	Camera->CaptureSource = SCS_BaseColor;
	const auto Visible = Sample();
	Life->SetReturnCue(false);
	const auto Hidden = Sample();
	if (!TestEqual(TEXT("Visible pixels read back"), Visible.Num(), 256 * 256) || !TestEqual(TEXT("Hidden readback"), Hidden.Num(), Visible.Num())) { return false; }
	const FString Proof = FPaths::ProjectSavedDir() / TEXT("MatchBall");
	if (Lit.Num() == Visible.Num()) { FFileHelper::CreateBitmap(*(Proof / TEXT("GroundWarning_Lit.bmp")), 256, 256, Lit.GetData()); }
	FFileHelper::CreateBitmap(*(Proof / TEXT("GroundWarning_BaseColor.bmp")), 256, 256, Visible.GetData());
	FFileHelper::CreateBitmap(*(Proof / TEXT("GroundWarning_Hidden.bmp")), 256, 256, Hidden.GetData());
	int32 RingPixels = 0, InteriorPixels = 0;
	for (int32 I = 0; I < Visible.Num(); ++I)
	{
		const int32 CyanContrast = int32(Visible[I].G) - Visible[I].R;
		const int32 GroundContrast = int32(Hidden[I].G) - Hidden[I].R;
		if (CyanContrast < 24 || CyanContrast - GroundContrast < 16 || Visible[I].B <= Visible[I].R) { continue; }
		++RingPixels;
		if (FVector2D(I % 256 - 128, I / 256 - 128).SizeSquared() < 55 * 55) { ++InteriorPixels; }
	}
	TestTrue(TEXT("Cyan annulus visible from the top; center stays clear"), RingPixels > 750 && InteriorPixels < 40);
	AddInfo(FString::Printf(TEXT("Visible cyan pixels=%d inside center=%d"), RingPixels, InteriorPixels));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSpecialBallArenaTest, "HAL.FutureCreation.SpecialBall.RoundAndOutOfBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALSpecialBallArenaTest::RunTest(const FString&)
{
	FMatchBallWorld F;
	auto* Settings = GetMutableDefault<UHALMatchBallSettings>();
	const auto* Production = ProductionBallArena();
	if (!TestNotNull(TEXT("Configured match arena"), Production)) { return false; }
	auto Rules = *Production;
	Rules.MatchMap = F.World;
	Rules.FloorCenter = FVector::ZeroVector;
	const int32 Index = Settings->Arenas.Add(Rules);
	ON_SCOPE_EXIT { Settings->Arenas.RemoveAt(Index); };
	auto* Match = F.World->SpawnActor<AHALMatchGameState>();
	F.World->SetGameState(Match);
	auto* Ball = F.World->SpawnActor<AHALExplosiveBall>(FVector(0,0,100), FRotator::ZeroRotator);
	TestEqual(TEXT("Preplaced ball waits for kickoff"), Ball->RemainingFreeSeconds(), 0.f);
	TestTrue(TEXT("Waiting ball cannot fall or block cars before it appears"),
		!Ball->GetPhysicsRoot()->IsGravityEnabled() && Ball->GetPhysicsRoot()->GetCollisionResponseToChannel(ECC_PhysicsBody) == ECR_Ignore);
	Match->SetMatchPhase(EHALMatchPhase::Playing);
	TestTrue(TEXT("Playing starts its own free budget"), Ball->RemainingFreeSeconds() > 0);
	TestTrue(TEXT("Kickoff restores the authored physical body"),
		Ball->GetPhysicsRoot()->IsGravityEnabled() && Ball->GetPhysicsRoot()->GetCollisionResponseToChannel(ECC_PhysicsBody) == ECR_Block);
	const FVector Outside(2*Rules.PlayableRadius,0,100);
	Ball->SetActorLocation(Outside, false, nullptr, ETeleportType::TeleportPhysics);
	Ball->CheckArena();
	F.World->TimeSeconds += Rules.OutsideGraceSeconds * .5f;
	Ball->CheckArena();
	TestTrue(TEXT("Brief excursion survives"), IsValid(Ball));
	Ball->SetActorLocation(FVector(0,0,100), false, nullptr, ETeleportType::TeleportPhysics);
	Ball->CheckArena();
	Ball->SetActorLocation(Outside, false, nullptr, ETeleportType::TeleportPhysics);
	Ball->CheckArena();
	F.World->TimeSeconds += Rules.OutsideGraceSeconds + .1;
	Ball->CheckArena();
	TestFalse(TEXT("Sustained exit retires the special ball"), IsValid(Ball));
	auto* Next = F.World->SpawnActor<AHALExplosiveBall>(FVector(0,0,100), FRotator::ZeroRotator);
	const auto* HoleType = Settings->BlackHoleBallType.LoadSynchronous();
	FHALBlackHoleConfig Reduced;
	if (!TestTrue(TEXT("Production field settings load"), HoleType && HoleType->ResolveField(Reduced))) { return false; }
	const FTransform Placement(FVector(200, 0, 2));
	auto* ActiveField = F.World->SpawnActorDeferred<AHALBlackHole>(HoleType->FieldClass, Placement);
	if (!TestTrue(TEXT("Ball can configure its own field"), ActiveField && ActiveField->ConfigureMatchBallField(Reduced))) { return false; }
	ActiveField->FinishSpawning(Placement);
	TestTrue(TEXT("Match was present when the field began play"), ActiveField->HasActorBegunPlay() && F.World->GetGameState() == Match);
	Match->SetMatchPhase(EHALMatchPhase::Finished);
	TestFalse(TEXT("Results clean up a surviving drop"), IsValid(Next));
	TestFalse(TEXT("Results also clean up the active ball field immediately"), IsValid(ActiveField));
	for (TActorIterator<AHALSpecialBallCue> It(F.World); It; ++It)
	{ TestFalse(TEXT("Round or arena cleanup never detonates"), It->GetCueState().Kind == EHALBallCue::Impact); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSpecialBallRenderTest, "HAL.RenderSmoke.SpecialBall.DarkSunAndThreeCues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALSpecialBallRenderTest::RunTest(const FString&)
{
	if (!FApp::CanEverRender()) { AddError(TEXT("Actual RHI required.")); return false; }
	FMatchBallWorld F;
	auto* World = F.World;
	auto* Type = GetDefault<UHALMatchBallSettings>()->ExplosiveBallType.LoadSynchronous();
	if (!TestNotNull(TEXT("Production explosive type"), Type)) { return false; }
	if (!TestNotNull(TEXT("Production dark sun has a persistent corona material"), Type->BodyCoronaMaterial.Get())) { return false; }
	if (!TestNotNull(TEXT("Production impact has a solar eruption material"), Type->EruptionMaterial.Get())) { return false; }
	for (UMaterialInterface* Material : {Type->CueMaterial.Get(), Type->Visual.Materials[0].Get(), Type->BodyCoronaMaterial.Get(), Type->EruptionMaterial.Get()})
	{
		Material->GetMaterial()->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous);
	}
	const FVector Origin(0, 0, 150);
	auto* Ball = World->SpawnActor<AHALExplosiveBall>(Origin, FRotator::ZeroRotator);
	auto* Camera = World->SpawnActor<ASceneCapture2D>()->GetCaptureComponent2D();
	Camera->bCaptureEveryFrame = false;
	Camera->ProjectionType = ECameraProjectionMode::Orthographic;
	Camera->CaptureSource = SCS_FinalColorLDR;
	Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	Camera->SetWorldLocation(Origin + FVector(0,0,900));
	Camera->SetWorldRotation(FRotator(-90,0,0));
	Camera->ShowFlags.SetTemporalAA(false);
	Camera->ShowFlags.SetBloom(false);
	Camera->ShowFlags.SetFog(false);
	Camera->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	auto* Target = NewObject<UTextureRenderTarget2D>();
	Target->InitCustomFormat(320,320,PF_B8G8R8A8,false);
	Camera->TextureTarget = Target;
	FAssetCompilingManager::Get().FinishAllCompilation();
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("ExplosiveBall/Visual");
	IFileManager::Get().MakeDirectory(*Directory,true);
	auto ReadCapture = [&]()
	{
		World->SendAllEndOfFrameUpdates();
		FlushRenderingCommands();
		Camera->CaptureScene();
		TArray<FColor> Pixels;
		Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
		return Pixels;
	};
	auto Capture = [&](AActor* Actor, float Width, const TCHAR* Name)
	{
		Camera->OrthoWidth = Width;
		Camera->ShowOnlyActors.Reset();
		Camera->ShowOnlyActors.Add(Actor);
		const auto* Root = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
		AddInfo(FString::Printf(TEXT("%s origin=%s bounds=%s width=%.1f"), *Actor->GetName(),
			*Actor->GetActorLocation().ToCompactString(), Root ? *Root->Bounds.BoxExtent.ToCompactString() : TEXT("?"), Width));
		TArray<FColor> Pixels = ReadCapture();
		int32 Warm = 0;
		for (const FColor& P : Pixels) { Warm += P.R > 18 && int32(P.R) > int32(P.B) + 8; }
		if (Pixels.Num() == 320*320) { FFileHelper::CreateBitmap(*(Directory / Name), 320, 320, Pixels.GetData()); }
		AddInfo(FString::Printf(TEXT("%s: %d warm pixels"), Name, Warm));
		TestTrue(Name, Warm > 120);
		return Pixels;
	};
	const float BodyRadius = Ball->GetPhysicsRoot()->GetScaledSphereRadius();
	const auto* Shell = Ball->GetVisualMesh();
	TestTrue(TEXT("Visible core matches the physical radius"),
		FMath::IsNearlyEqual(Shell->GetStaticMesh()->GetBounds().BoxExtent.X * Shell->GetComponentScale().X, double(BodyRadius), .5));
	const auto BodyPixels = Capture(Ball, BodyRadius*3.f, TEXT("DarkSun.bmp"));
	int32 HaloPixels = 0;
	for (int32 I = 0; I < BodyPixels.Num(); ++I)
	{
		const double PixelRadius = FVector2D(I%320-159.5, I/320-159.5).Size() * 3./320;
		const FColor& Pixel = BodyPixels[I];
		HaloPixels += PixelRadius > 1.04 && PixelRadius < 1. + Type->BodyCoronaWidth &&
			Pixel.G > 22 && int32(Pixel.R) - Pixel.B > 20;
	}
	AddInfo(FString::Printf(TEXT("%d warm corona pixels outside the solid sphere with bloom disabled"), HaloPixels));
	TestTrue(TEXT("A persistent warm corona is visible outside the core"), HaloPixels > 250);
	bool bOpenImpactCaptured = false;
	for (const auto Kind : {EHALBallCue::Arrival, EHALBallCue::Expiration, EHALBallCue::Impact, EHALBallCue::Impact})
	{
		const float Radius = Kind == EHALBallCue::Impact ? Type->ExplosionRadius : Ball->GetConfiguredSphereRadius();
		const bool bImpact = Kind == EHALBallCue::Impact;
		const bool bCovered = bImpact && bOpenImpactCaptured;
		if (bCovered) { F.Block(Origin + FVector(110,0,0), FVector(10,1000,700)); }
		AHALSpecialBallCue::Spawn(*World, Kind, Type, Origin, Radius, FVector::ZeroVector);
		World->TimeSeconds += (bImpact ? Type->ImpactSeconds : Type->CueSeconds(Kind))*.22f;
		AHALSpecialBallCue* Latest = nullptr;
		for (TActorIterator<AHALSpecialBallCue> It(World); It; ++It)
		{ if (It->GetCueState().Kind == Kind && (!Latest || It->GetCueState().Started >= Latest->GetCueState().Started)) { Latest = *It; } }
		if (!TestNotNull(TEXT("Independent world effect"), Latest)) { return false; }
		Latest->Tick(0);
		TInlineComponentArray<UDynamicMeshComponent*> Meshes(Latest);
		TestEqual(TEXT("Only impacts add the prominence and smoke mesh"), Meshes.Num(), bImpact ? 2 : 1);
		const float Width = Radius * (bImpact ? 2.25f : 2.3f*Type->CoronaSize);
		const TCHAR* Name = bCovered ? TEXT("ImpactWithCover.bmp") : bImpact ? TEXT("Impact.bmp") :
			Kind == EHALBallCue::Arrival ? TEXT("Arrival.bmp") : TEXT("Expiration.bmp");
		const auto Pixels = Capture(Latest, Width, Name);
		if (bImpact && Pixels.Num() == 320*320)
		{
			int32 BehindWall = 0, BeyondRadius = 0, Edge = 0;
			const FVector CameraUp = Camera->GetUpVector();
			const FVector CameraRight = Camera->GetRightVector();
			for (int32 I=0; I<Pixels.Num(); ++I)
			{
				if (int32(Pixels[I].R) - Pixels[I].B <= 8) { continue; }
				const FVector Offset = Width/320 * ((I%320-159.5f)*CameraRight + (159.5f-I/320)*CameraUp);
				BehindWall += Offset.X > 114.f;
				BeyondRadius += Offset.Size2D() > Radius + 8;
				Edge += Offset.Size2D() > Radius*.96f;
			}
			TestTrue(TEXT("The impact edge is visible at the authored radius"), Edge > 50);
			if (bCovered) { TestEqual(TEXT("No ribbons beyond solid cover"), BehindWall, 0); }
			else { TestTrue(TEXT("Same sector lit without cover"), BehindWall > 100); }
			TestEqual(TEXT("No ribbon beyond gameplay radius"), BeyondRadius, 0);
			// Isolate the opaque eruption at full bloom so the pressure ring cannot pass this check alone.
			World->TimeSeconds += Type->ImpactSeconds*.22f;
			Latest->Tick(0);
			Latest->GetRootComponent()->SetVisibility(false, false);
			const auto Peak = Capture(Latest, Width, bCovered ? TEXT("SolarPeakWithCover.bmp") : TEXT("SolarPeak.bmp"));
			int32 Core = 0, Arches = 0, ThroughCover = 0, Outside = 0;
			for (int32 I = 0; I < Peak.Num(); ++I)
			{
				if (int32(Peak[I].R) - Peak[I].B <= 8) { continue; }
				const FVector Offset = Width/320 * ((I%320-159.5f)*CameraRight + (159.5f-I/320)*CameraUp);
				const double Distance = Offset.Size2D();
				Core += Distance < Radius*.22;
				Arches += Distance > Radius*.38;
				ThroughCover += Offset.X > 114.f;
				Outside += Distance > Radius + 8;
			}
			TestTrue(TEXT("Compact solar core is visible at peak"), Core > 120);
			TestTrue(TEXT("Prominences extend past the core without the range ring"), Arches > 150);
			TestEqual(TEXT("Prominences stay inside the damage radius"), Outside, 0);
			if (bCovered) { TestEqual(TEXT("No prominence or smoke behind the wall at peak"), ThroughCover, 0); }
			else if (Type->CoreSmokeSize > .1f)
			{
				auto* BareType = DuplicateObject<UHALExplosiveBallDefinition>(Type, GetTransientPackage());
				BareType->CoreSmokeSize = 0.f;
				AHALSpecialBallCue::Spawn(*World, Kind, BareType, Origin, Radius, FVector::ZeroVector);
				AHALSpecialBallCue* Bare = nullptr;
				for (TActorIterator<AHALSpecialBallCue> It(World); It; ++It)
				{ if (It->GetCueState().Type == BareType) { Bare = *It; break; } }
				if (!TestNotNull(TEXT("Smoke-free impact"), Bare)) { return false; }
				World->TimeSeconds += Type->ImpactSeconds*.44f;
				Bare->Tick(0);
				Bare->GetRootComponent()->SetVisibility(false, false);
				const auto BarePixels = Capture(Bare, Width, TEXT("SolarPeakWithoutSmoke.bmp"));
				if (BarePixels.Num() != Peak.Num()) { AddError(TEXT("Incomplete smoke comparison")); return false; }
				int32 ChangedCore = 0, ChangedOuterSilhouette = 0;
				for (int32 I = 0; I < Peak.Num(); ++I)
				{
					const FColor& A = Peak[I];
					const FColor& B = BarePixels[I];
					const double Distance = Width/320 * FVector2D(I%320-159.5, I/320-159.5).Size();
					const int32 ColorDifference = FMath::Abs(int(A.R)-B.R) + FMath::Abs(int(A.G)-B.G) + FMath::Abs(int(A.B)-B.B);
					ChangedCore += Distance < Radius*Type->CoreSmokeSize && ColorDifference >= 36;
					// Subpixel coverage can change a few edge colours between captures. Compare their silhouettes.
					const bool bHasFlame = int(A.R)-A.B > 8;
					const bool bBareHasFlame = int(B.R)-B.B > 8;
					ChangedOuterSilhouette += Distance > Radius*.50 && bHasFlame != bBareHasFlame;
				}
				AddInfo(FString::Printf(TEXT("Switching off core smoke changed %d central pixels and %d outer silhouette pixels"), ChangedCore, ChangedOuterSilhouette));
				TestTrue(TEXT("Core Smoke Size adds visible cloud faces in the centre"), ChangedCore > 50);
				TestEqual(TEXT("Switching off the smoke preserves the outer silhouette"), ChangedOuterSilhouette, 0);
			}
			if (!bCovered)
			{
				const auto* Child = Meshes.FindByPredicate([Latest](UDynamicMeshComponent* Mesh) { return Mesh != Latest->GetRootComponent(); });
				auto* Paint = Child ? Cast<UMaterialInstanceDynamic>((*Child)->GetMaterial(0)) : nullptr;
				if (!TestNotNull(TEXT("Fire arches have their own material instance"), Paint)) { return false; }
				TestTrue(TEXT("Flow speed is measured in ribbon lengths per second"),
					FMath::IsNearlyEqual(Paint->K2_GetScalarParameterValue(TEXT("ProminenceFlowTravel")), Type->ProminenceFlowSpeed * Type->ImpactSeconds));
				TestTrue(TEXT("Flow contrast comes from the ball type"),
					FMath::IsNearlyEqual(Paint->K2_GetScalarParameterValue(TEXT("ProminenceFlowStrength")), Type->ProminenceFlowStrength));

				// Freeze the pose and mask at peak. Only the UV phase of the interior paint changes.
				auto CaptureDepth = [&]()
				{
					Camera->CaptureSource = SCS_DeviceDepth;
					auto Depth = ReadCapture();
					Camera->CaptureSource = SCS_FinalColorLDR;
					return Depth;
				};
				Paint->SetScalarParameterValue(TEXT("ProminenceFlowStrength"), .8f);
				Paint->SetScalarParameterValue(TEXT("ProminenceFlowTravel"), 0.f);
				const auto Still = Capture(Latest, Width, TEXT("SolarFlowStill.bmp"));
				const auto StillDepth = CaptureDepth();
				Paint->SetScalarParameterValue(TEXT("ProminenceFlowTravel"), .68f);
				const auto Moved = Capture(Latest, Width, TEXT("SolarFlowMoved.bmp"));
				const auto MovedDepth = CaptureDepth();
				if (Still.Num() != 320*320 || Moved.Num() != Still.Num() || StillDepth.Num() != Still.Num() || MovedDepth.Num() != Still.Num())
				{ AddError(TEXT("Incomplete flow comparison")); return false; }
				// Only the eruption is rendered. The top-left pixel is outside its radius and gives the clear depth.
				const FColor BackgroundDepth = StillDepth[0];
				TestTrue(TEXT("Both depth captures use the same background"), MovedDepth[0] == BackgroundDepth);
				int32 ChangedRibbon = 0, CoveredPixels = 0, ChangedCoverage = 0;
				for (int32 I = 0; I < Still.Num(); ++I)
				{
					const FColor A = Still[I], B = Moved[I];
					const double Distance = Width/320 * FVector2D(I%320-159.5, I/320-159.5).Size();
					const int32 Difference = FMath::Abs(int(A.R)-B.R) + FMath::Abs(int(A.G)-B.G) + FMath::Abs(int(A.B)-B.B);
					ChangedRibbon += Distance > Radius*.38 && Difference >= 36;
					const bool bWasCovered = StillDepth[I] != BackgroundDepth;
					CoveredPixels += bWasCovered;
					ChangedCoverage += bWasCovered != (MovedDepth[I] != BackgroundDepth);
				}
				AddInfo(FString::Printf(TEXT("Advancing only the flow changed %d outer ribbon colours and %d coverage pixels (%d foreground depth pixels)"), ChangedRibbon, ChangedCoverage, CoveredPixels));
				TestTrue(TEXT("The depth pass contains the eruption"), CoveredPixels > 150 && CoveredPixels < Still.Num());
				TestTrue(TEXT("Surface paint travels visibly outside the central smoke"), ChangedRibbon > 80);
				TestEqual(TEXT("Flow alone preserves the eruption silhouette"), ChangedCoverage, 0);
				if (Type->ProminenceTailSeconds > 0.f)
				{
					Paint->SetScalarParameterValue(TEXT("ProminenceFlowTravel"), Type->ProminenceFlowSpeed * Type->ImpactSeconds);
					Paint->SetScalarParameterValue(TEXT("ProminenceFlowStrength"), Type->ProminenceFlowStrength);
					Latest->GetRootComponent()->SetVisibility(true, false);
					Camera->ShowOnlyActors.Reset();
					Camera->ShowOnlyComponent(*Child);
					const float Duration = Type->CueSeconds(Kind);
					int64 Energy[11] = {};
					for (int32 Sample = 0; Sample < UE_ARRAY_COUNT(Energy); ++Sample)
					{
						// Sample the real cue starting just after its flash has ended, through actor-side hiding.
						const float Elapsed = FMath::Lerp(Type->ImpactSeconds, Duration, Sample/10.f) + .0001f;
						World->TimeSeconds = Latest->GetCueState().Started + Elapsed;
						Latest->Tick(0);
						const auto Frame = ReadCapture();
						if (Frame.Num() != Still.Num()) { AddError(TEXT("Incomplete afterglow capture")); return false; }
						for (int32 I = 0; I < Frame.Num(); ++I)
						{
							const double Distance = Width/320 * FVector2D(I%320-159.5, I/320-159.5).Size();
							if (Distance > Radius*.38) { Energy[Sample] += FMath::Max(0, int32(Frame[I].R) - Frame[I].B - 8); }
						}
						if (Sample == 0)
						{
							TestFalse(TEXT("The range ring has finished"), Latest->GetRootComponent()->IsVisible());
							TestTrue(TEXT("The arches outlive the range ring"), (*Child)->IsVisible());
						}
					}
					Camera->ClearShowOnlyComponents();
					int64 LargestLoss = 0;
					for (int32 I = 1; I < UE_ARRAY_COUNT(Energy); ++I) { LargestLoss = FMath::Max(LargestLoss, Energy[I-1]-Energy[I]); }
					AddInfo(FString::Printf(TEXT("Prominence tail warm signal at 0/50/90/100%%: %lld/%lld/%lld/%lld; largest step %lld"), Energy[0], Energy[5], Energy[9], Energy[10], LargestLoss));
					TestTrue(TEXT("Visible embers cool across the tail"), Energy[0] > 1000 && Energy[5] > 0 && Energy[5] < Energy[0]);
					TestTrue(TEXT("No single tenth cuts away most of the remaining display"), LargestLoss < Energy[0]*.40);
					TestTrue(TEXT("The last rendered embers are already faint before the cue is hidden"), Energy[9] < Energy[0]*.04);
					TestEqual(TEXT("No eruption after its visual lifetime"), Energy[10], int64(0));
				}
			}
			bOpenImpactCaptured = true;
		}
	}
	return true;
}
#endif
