#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "HALVehicleStabilityTestRules.h"
#include "../HALBombActor.h"
#include "../HALSpecialBall.h"
#include "../HALSpecialBallCue.h"
#include "../HALElectricWallGimmick.h"
#include "../HALElectricSphereGimmick.h"
#include "Components/SphereComponent.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALGameplayTags.h"
#include "../HALImpactAttributeSet.h"
#include "../HALCombatAttributeSet.h"
#include "../PassiveTestVehicle.h"
#include "../VehicleDefinition.h"
#include "../VehicleHealthComponent.h"
#include "../VehicleKnockbackSettings.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace
{
	struct FBombWorld
	{
		FScopedHALStabilityTestRules Stability{false};
		UWorld* World;
		FBombWorld()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
				.ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		}
		~FBombWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
		APassiveTestVehicle* Target(FVector Location = FVector(100,0,100))
		{
			auto* Actor = World->SpawnActor<APassiveTestVehicle>();
			auto* Def = NewObject<UVehicleDefinition>();
			Def->bReadyForUse = true; Def->CollisionShape = EVehicleCollisionShape::Box;
			Def->Visual.Mesh = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
			Def->Combat.Version = EHALCombatConfigVersion::AttackSnapshotV1;
			Def->Impact.Version = EHALImpactVersion::TieredV1;
			auto* Source = FindFProperty<FEnumProperty>(Actor->GetClass(),TEXT("ConfigurationSource"));
			Source->GetUnderlyingProperty()->SetIntPropertyValue(Source->ContainerPtrToValuePtr<void>(Actor),uint64(EVehicleConfigurationSource::Definition));
			FindFProperty<FObjectProperty>(Actor->GetClass(),TEXT("Definition"))->SetObjectPropertyValue_InContainer(Actor,Def);
			Actor->PreInitializeComponents(); Actor->DispatchBeginPlay(); Actor->SetActorLocation(Location);
			return Actor;
		}
		AHALBombExplosion* Explosion(FHALBombConfig Config = FHALBombConfig(), FHALAttackSourceSnapshot Attribution = FHALAttackSourceSnapshot())
		{
			const FTransform Transform(FVector(0,0,100));
			auto* Actor = World->SpawnActorDeferred<AHALBombExplosion>(AHALBombExplosion::StaticClass(),Transform);
			Actor->Initialize(Config,Attribution); Actor->FinishSpawning(Transform);
			return Actor;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBombGroundTest,"HAL.FutureCreation.Bomb.GroundArmingAndDestruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBombGroundTest::RunTest(const FString&)
{
	FBombWorld F;
	auto* Bomb = F.World->SpawnActor<AHALBombActor>(FVector(0,0,1000),FRotator::ZeroRotator);
	Bomb->DispatchBeginPlay();
	auto* Ground = F.World->SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Ground); Ground->SetRootComponent(Box); Box->RegisterComponent();
	FHitResult Hit(Ground,Box,FVector::ZeroVector,FVector::RightVector); Hit.bBlockingHit = true;
	TestFalse(TEXT("Airborne starts unarmed"),Bomb->IsArmed());
	TestFalse(TEXT("Wall contact never arms"),Bomb->TryArmFromGround(Hit));
	Hit.ImpactNormal = FVector::UpVector;
	TestTrue(TEXT("First valid ground arms"),Bomb->TryArmFromGround(Hit));
	const float End = Bomb->GetFuseEndServerTime();
	TestFalse(TEXT("Repeated bounce cannot rearm"),Bomb->TryArmFromGround(Hit));
	TestEqual(TEXT("Bounce preserves end time"),Bomb->GetFuseEndServerTime(),End);
	Bomb->Detonate();
	TestTrue(TEXT("Explosion destroys physical prefab"),Bomb->IsActorBeingDestroyed());
	int32 Count = 0; for (TActorIterator<AHALBombExplosion> It(F.World);It;++It) { ++Count; }
	TestEqual(TEXT("Exactly one explosion range"),Count,1);
	Bomb->Detonate();
	Count=0; for (TActorIterator<AHALBombExplosion> It(F.World);It;++It) { ++Count; }
	TestEqual(TEXT("Repeated detonation cannot spawn another pulse"),Count,1);
	FHALBombConfig Bad; Bad.Damage=std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Nonfinite damage invalid"),Bad.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBombCombatTest,"HAL.FutureCreation.Bomb.DamageImpactImmunityAndDedup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBombCombatTest::RunTest(const FString&)
{
	FBombWorld F;
	auto* Target = F.Target();
	auto* ASC = CastChecked<UHALAbilitySystemComponent>(Target->GetAbilitySystemComponent());
	auto* Health = Target->FindComponentByClass<UVehicleHealthComponent>();
	auto* Explosion = F.Explosion();
	const auto First = Explosion->ResolveTarget(Target);
	TestTrue(TEXT("Configured explosion accepted"),First.bHitAccepted);
	TestEqual(TEXT("Damage uses shared GAS HP"),Health->GetCurrentHP(),75.f);
	TestEqual(TEXT("Authored heavy tier reaches receiver"),First.ImpactSummary.SourceTier,EHALImpactTier::Heavy);
	TestEqual(TEXT("Existing bomb remains Standard"),First.ImpactSummary.PushPreset,EHALImpactPushPreset::Standard);
	TestEqual(TEXT("Duplicate target rejected"),Explosion->ResolveTarget(Target).RejectReason,EHALHitRejectReason::Duplicate);
	ASC->AddLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
	const auto Immune = F.Explosion()->ResolveTarget(Target);
	TestEqual(TEXT("Impact immunity does not block damage"),Health->GetCurrentHP(),50.f);
	TestEqual(TEXT("Impact immunity suppresses rule impact"),Immune.ImpactSummary.EffectiveTier,EHALImpactTier::None);
	ASC->RemoveLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
	ASC->AddLooseGameplayTag(HALGameplayTags::State_Combat_DamageImmune);
	const auto DamageImmune = F.Explosion()->ResolveTarget(Target);
	TestEqual(TEXT("Damage immunity preserves HP"),Health->GetCurrentHP(),50.f);
	TestEqual(TEXT("Damage immunity preserves heavy impact"),DamageImmune.ImpactSummary.EffectiveTier,EHALImpactTier::Heavy);
	ASC->RemoveLooseGameplayTag(HALGameplayTags::State_Combat_DamageImmune);
	ASC->SetNumericAttributeBase(UHALImpactAttributeSet::GetTierReductionAttribute(),1);
	TestEqual(TEXT("GAS reduction lowers bomb impact"),F.Explosion()->ResolveTarget(Target).ImpactSummary.EffectiveTier,EHALImpactTier::Medium);
	auto* Far = F.Target(FVector(2000,0,100));
	TestFalse(TEXT("Outside radius rejected"),Explosion->ResolveTarget(Far).bHitAccepted);
	FHALBombConfig Config; Config.bCanHitInstigator=false;
	FHALAttackSourceSnapshot Attribution; Attribution.InstigatorActor=Far;
	Far->SetActorLocation(FVector(100,0,100));
	TestEqual(TEXT("Authored self exclusion"),F.Explosion(Config,Attribution)->ResolveTarget(Far).RejectReason,EHALHitRejectReason::ExcludedInstigator);
	Config.bCanHitInstigator=true; Config.ImpactPushPreset=EHALImpactPushPreset::Long;
	const auto Long = F.Explosion(Config)->ResolveTarget(Far);
	FVehicleKnockbackConfig Table; GetDefault<UVehicleKnockbackSettings>()->GetRuntimeRules(Table);
	FHALImpactPushRules Push; GetDefault<UVehicleKnockbackSettings>()->GetImpactPushRules(Push);
	TestEqual(TEXT("Explosion captures optional Long preset"),Long.ImpactSummary.PushPreset,EHALImpactPushPreset::Long);
	TestEqual(TEXT("Explosion uses shared Long multiplier"),Long.ImpactSummary.DesiredHorizontalDeltaSpeed,Table.HeavyTier.HorizontalDeltaSpeed*Push.LongMultiplier);
	return true;
}

namespace
{
	class FBombTimerAdvance : public IAutomationLatentCommand
	{
		TSharedRef<FBombWorld> Fixture;
		FAutomationTestBase* Test;
		TWeakObjectPtr<AHALBombActor> Bomb;
		float EndTime;
		uint64 LastFrame=MAX_uint64;
		double Deadline=FPlatformTime::Seconds()+15;
	public:
		FBombTimerAdvance(TSharedRef<FBombWorld> F, FAutomationTestBase* T, AHALBombActor* B)
			: Fixture(F), Test(T), Bomb(B), EndTime(F->World->GetTimeSeconds()+B->GetBombConfig().FuseSeconds+0.1f) {}
		bool Update() override
		{
			if (FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("Bomb fuse timer test timed out")); return true; }
			if (Fixture->World->GetTimeSeconds()>=EndTime)
			{
				Test->TestFalse(TEXT("Real timer destroys bomb without a manual detonation"),Bomb.IsValid());
				int32 Count=0; for (TActorIterator<AHALBombExplosion> It(Fixture->World);It;++It) { ++Count; }
				Test->TestEqual(TEXT("Timer creates exactly one independent explosion"),Count,1);
				return true;
			}
			if (LastFrame!=GFrameCounter) { LastFrame=GFrameCounter; Fixture->World->Tick(LEVELTICK_All,0.05f); }
			return false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBombTimerTest,"HAL.FutureCreation.Bomb.ExplicitCountdownAndRealTimer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBombTimerTest::RunTest(const FString&)
{
	auto Fixture=MakeShared<FBombWorld>();
	auto* Bomb=Fixture->World->SpawnActor<AHALBombActor>(FVector(0,0,1000),FRotator::ZeroRotator);
	Bomb->DispatchBeginPlay();
	TestFalse(TEXT("Placed airborne bomb waits for ground"),Bomb->IsArmed());
	TestTrue(TEXT("Explicit development trigger starts airborne fuse immediately"),Bomb->StartCountdown());
	const float End=Bomb->GetFuseEndServerTime();
	TestTrue(TEXT("Configured fuse duration"),FMath::IsNearlyEqual(End-Fixture->World->GetTimeSeconds(),3.f));
	TestFalse(TEXT("Repeated trigger cannot reset countdown"),Bomb->StartCountdown());
	TestEqual(TEXT("Fuse end remains stable"),Bomb->GetFuseEndServerTime(),End);
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FBombTimerAdvance>(Fixture,this,Bomb));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALExplosiveBallTest, "HAL.FutureCreation.SpecialBall.LifetimeContactsAndCover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALExplosiveBallTest::RunTest(const FString&)
{
	FBombWorld F;
	const auto* Type = GetDefault<UHALMatchBallSettings>()->ExplosiveBallType.LoadSynchronous();
	FString Why;
	if (!TestTrue(TEXT("Saved production ball DA and materials validate"), Type && Type->Validate(Why)))
	{ AddError(Why); return false; }
	auto SpawnBall = [&](FVector Position)
	{
		auto* Ball = F.World->SpawnActor<AHALExplosiveBall>(Position, FRotator::ZeroRotator);
		Ball->PreInitializeComponents();
		Ball->DispatchBeginPlay();
		return Ball;
	};
	auto CueCount = [&](EHALBallCue Kind)
	{
		int32 Count = 0;
		for (TActorIterator<AHALSpecialBallCue> It(F.World); It; ++It)
		{ if (It->GetCueState().Kind == Kind && It->GetCueState().Type == Type) { ++Count; } }
		return Count;
	};
	auto* Ball = SpawnBall(FVector(0, -300, 110));
	if (!TestTrue(TEXT("Native drop uses its configured ball type"), Ball->GetTypeDefinition() == Type && Ball->IsConfigurationValid())) { return false; }
	TestFalse(TEXT("Does not compete for primary designation or camera"), Ball->IsPrimaryMatchBall() || Ball->IsPrimaryCameraTarget());
	TestEqual(TEXT("One arrival cue"), CueCount(EHALBallCue::Arrival), 1);
	auto* Shooter = F.Target(FVector(-160, 0, 110));
	auto* Victim = F.Target(FVector(90, 5, 110));
	auto* Shielded = F.Target(FVector(360, 5, 110));
	auto* Far = F.Target(FVector(0, 0, 1100));
	auto HP = [](AActor* Vehicle) { return Vehicle->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(); };
	const float Full = HP(Victim);
	F.World->TimeSeconds += 5.;
	TestTrue(TEXT("Free lifetime counts down"), FMath::IsNearlyEqual(Ball->RemainingFreeSeconds(), Type->FreeLifeSeconds - 5.f, .01f));
	TestTrue(TEXT("First holder"), Ball->BeginControl(Victim));
	F.World->TimeSeconds += 8.;
	TestTrue(TEXT("Clock paused while held"), FMath::IsNearlyEqual(Ball->RemainingFreeSeconds(), Type->FreeLifeSeconds - 5.f, .01f));
	TestTrue(TEXT("Forced drop"), Ball->ReleaseFromControl(Victim, .15f));
	TestFalse(TEXT("Recent holder cannot recapture immediately"), Ball->CanBeControlledBy(Victim));
	F.World->TimeSeconds += 2.;
	TestTrue(TEXT("Another car can take the same special ball"), Ball->BeginControl(Shooter));
	TestTrue(TEXT("Formal shot"), Ball->LaunchFromControl(Shooter, Shooter, FVector(0, 2400, 0)));
	F.World->TimeSeconds += 6.;
	TestTrue(TEXT("Flight retains remaining budget and new attribution"), Ball->GetLaunchedBy() == Shooter
		&& FMath::IsNearlyEqual(Ball->RemainingFreeSeconds(), Type->FreeLifeSeconds - 7.f, .01f));
	auto* Wall = F.World->SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(10, 400, 180));
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->SetWorldLocation(FVector(240, 0, 110));
	Box->RegisterComponent();
	FHitResult FloorHit(Wall, Box, FVector(0, -250, 60), FVector::UpVector);
	Ball->ResolveLaunchedContact(Wall, Box, FVector::ZeroVector, FloorHit);
	TestFalse(TEXT("Floor normal does not detonate"), Ball->IsActorBeingDestroyed());
	const FVector Origin(0, 0, 110);
	TestTrue(TEXT("Unshielded ray"), FHALBlastArea::OpenFraction(*F.World, Origin, Victim->GetActorLocation()) == 1.f);
	TestTrue(TEXT("Solid wall blocks ray"), FHALBlastArea::OpenFraction(*F.World, Origin, Shielded->GetActorLocation()) < 1.f);
	auto* Body = CastChecked<UPrimitiveComponent>(Victim->GetRootComponent());
	FHitResult Direct(Victim, Body, Origin, -FVector::RightVector);
	Ball->ResolveLaunchedContact(Victim, Body, FVector::ZeroVector, Direct);
	Ball->ResolveLaunchedContact(Victim, Body, FVector::ZeroVector, Direct);
	TestTrue(TEXT("Shot consumed"), Ball->IsActorBeingDestroyed() || !IsValid(Ball));
	TestEqual(TEXT("No stacked direct and radial damage"), HP(Victim), Full - Type->Gameplay.Damage.VehicleHitDamage);
	TestEqual(TEXT("Cover protects the other car"), HP(Shielded), Full);
	TestEqual(TEXT("Launcher exempt even in radius"), HP(Shooter), Full);
	TestEqual(TEXT("Vertical distance counts"), HP(Far), Full);
	TestEqual(TEXT("Multiple contacts only create one impact"), CueCount(EHALBallCue::Impact), 1);
	for (TActorIterator<AHALSpecialBallCue> It(F.World); It; ++It)
	{
		if (It->GetCueState().Kind != EHALBallCue::Impact) { continue; }
		TestTrue(TEXT("Authoritative visual center and radius match hit query"),
			It->GetActorLocation().Equals(Origin, .1) && It->GetCueState().Radius == Type->ExplosionRadius);
	}
	auto* Field = F.World->SpawnActor<AHALElectricWallGimmick>(FVector(0, 2500, 0), FRotator::ZeroRotator);
	Field->DispatchBeginPlay();
	auto* Second = SpawnBall(FVector(0, 2100, 100));
	TestTrue(TEXT("Electric test launch"), Second->BeginControl(Shooter) && Second->LaunchFromControl(Shooter,Shooter,FVector(0, 15000, 0)));
	FHitResult Curtain;
	TestTrue(TEXT("Thin powered query-only curtain swept at high speed"), Second->FindFieldHit(FVector(0, 2900, 100), Curtain));
	TestTrue(TEXT("Contact on approach side of screen"), Curtain.ImpactPoint.Y > 2400 && Curtain.ImpactPoint.Y <= 2500);
	FHitResult Behind(Wall, Box, FVector(0, 2950, 100), -FVector::RightVector);
	Second->ResolveLaunchedContact(Wall, Box, FVector::ZeroVector, Behind);
	bool bCurtainFirst = false;
	for (TActorIterator<AHALSpecialBallCue> It(F.World); It; ++It)
	{ bCurtainFirst |= It->GetCueState().Kind == EHALBallCue::Impact && It->GetActorLocation().Equals(Curtain.ImpactPoint, 1.f); }
	TestTrue(TEXT("Powered curtain wins over later solid contact in the same step"), bCurtainFirst);
	Field->Destroy();
	auto* Inactive = F.World->SpawnActor<AHALElectricSphereGimmick>(FVector(0, 4500, 0), FRotator::ZeroRotator);
	Inactive->DispatchBeginPlay();
	auto* Third = SpawnBall(FVector(0, 4000, 100));
	TestTrue(TEXT("Dormant field test shot"), Third->BeginControl(Shooter) && Third->LaunchFromControl(Shooter,Shooter,FVector(0, 15000, 0)));
	TestFalse(TEXT("Warning-stage electric sphere does not trigger"), Third->FindFieldHit(FVector(0, 4900, 100), Curtain));
	// The actual Fred wall callback reported its contact 16 cm inside the blocking box.
	FHitResult WallHit(Wall, Box, FVector(246, 0, 110), -FVector::ForwardVector);
	Third->ResolveLaunchedContact(Wall, Box, FVector::ZeroVector, WallHit);
	TestEqual(TEXT("Wall hit itself consumes ball"), CueCount(EHALBallCue::Impact), 3);
	bool bWallCueOnFace = false;
	for (TActorIterator<AHALSpecialBallCue> It(F.World); It; ++It)
	{
		bWallCueOnFace |= It->GetCueState().Kind == EHALBallCue::Impact &&
			It->GetActorLocation().Equals(FVector(230,0,110), .1) &&
			FVector(It->GetCueState().SurfaceNormal).Equals(-FVector::ForwardVector, .01);
	}
	TestTrue(TEXT("Embedded physics contact places the effect on the incoming face"), bWallCueOnFace);
	TestEqual(TEXT("Far side of a directly hit wall is protected"), HP(Shielded), Full);
	TestEqual(TEXT("Wall hit can splash the approach side"), HP(Victim), Full - 2 * Type->Gameplay.Damage.VehicleHitDamage);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSpecialBallExpiryTest, "HAL.FutureCreation.SpecialBall.RealExpiryIsHarmless",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALSpecialBallExpiryTest::RunTest(const FString&)
{
	auto F = MakeShared<FBombWorld>();
	auto* Type = GetDefault<UHALMatchBallSettings>()->ExplosiveBallType.LoadSynchronous();
	if (!TestNotNull(TEXT("Authored type"), Type)) { return false; }
	auto* Short = DuplicateObject<UHALExplosiveBallDefinition>(Type, GetTransientPackage());
	Short->FreeLifeSeconds = .25f;
	auto* Ball = F->World->SpawnActor<AHALExplosiveBall>();
	FindFProperty<FObjectProperty>(Ball->GetClass(),TEXT("Definition"))->SetObjectPropertyValue_InContainer(Ball, Short);
	Ball->PreInitializeComponents();
	Ball->DispatchBeginPlay();
	auto* Target = F->Target(FVector(80,0,100));
	const float Initial = Target->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP();
	const TWeakObjectPtr<AHALExplosiveBall> Weak(Ball);
	const double Deadline = FPlatformTime::Seconds() + 8.;
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FFunctionLatentCommand>(
		[this, F, Weak, Target, Initial, Deadline]()
		{
			F->World->Tick(LEVELTICK_All, .05f);
			if (Weak.IsValid() && FPlatformTime::Seconds() < Deadline) { return false; }
			TestFalse(TEXT("Free lifetime timer destroys physical ball"), Weak.IsValid());
			int32 Expirations = 0, Impacts = 0;
			for (TActorIterator<AHALSpecialBallCue> It(F->World); It; ++It)
			{
				Expirations += It->GetCueState().Kind == EHALBallCue::Expiration;
				Impacts += It->GetCueState().Kind == EHALBallCue::Impact;
			}
			TestEqual(TEXT("Independent expiration effect"), Expirations, 1);
			TestEqual(TEXT("No explosion on expiration"), Impacts, 0);
			TestEqual(TEXT("Nearby car retains HP"), Target->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(), Initial);
			return true;
		}));
	return true;
}
#endif
