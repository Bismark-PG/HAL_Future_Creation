#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "HALVehicleStabilityTestRules.h"
#include "../HALBombActor.h"
#include "../HALSpecialBall.h"
#include "../HALBlackHoleBall.h"
#include "../HALMeteorBall.h"
#include "../HALTimedBombBall.h"
#include "../BallControlComponent.h"
#include "../HALSkillTestDummy.h"
#include "../HALHealthAttributeSet.h"
#include "../HALSpecialBallCue.h"
#include "../HALElectricWallGimmick.h"
#include "../HALElectricSphereGimmick.h"
#include "Components/SphereComponent.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALGameplayTags.h"
#include "../HALImpactAttributeSet.h"
#include "../HALImpactReceiverComponent.h"
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
	TestTrue(TEXT("Thin powered query-only curtain swept at high speed"), Second->FindFieldHit(Second->GetActorLocation(), FVector(0, 2900, 100), Curtain));
	TestTrue(TEXT("Contact on approach side of screen"), Curtain.ImpactPoint.Y > 2400 && Curtain.ImpactPoint.Y <= 2500);
	FHitResult Behind(Wall, Box, FVector(0, 2950, 100), -FVector::RightVector);
	Second->ResolveLaunchedContact(Wall, Box, FVector::ZeroVector, Behind);
	bool bCurtainFirst = false;
	for (TActorIterator<AHALSpecialBallCue> It(F.World); It; ++It)
	{ bCurtainFirst |= It->GetCueState().Kind == EHALBallCue::Impact && It->GetActorLocation().Equals(Curtain.ImpactPoint, 1.f); }
	TestTrue(TEXT("Powered curtain wins over later solid contact in the same step"), bCurtainFirst);
	Field->Destroy();
	auto* ElectricSphere = F.World->SpawnActor<AHALElectricSphereGimmick>(FVector(0, 4500, 0), FRotator::ZeroRotator);
	ElectricSphere->DispatchBeginPlay();
	auto* Third = SpawnBall(FVector(0, 4000, 100));
	TestTrue(TEXT("Electric sphere test shot"), Third->BeginControl(Shooter) && Third->LaunchFromControl(Shooter,Shooter,FVector(0, 15000, 0)));
	TestTrue(TEXT("Expanding energized sphere triggers a launched contact ball"), Third->FindFieldHit(Third->GetActorLocation(), FVector(0, 4900, 100), Curtain));
	const auto& SphereTiming = ElectricSphere->GetSphereConfig();
	F.World->TimeSeconds += SphereTiming.BeginSeconds + SphereTiming.LoopSeconds + .01f;
	TestFalse(TEXT("Spent electric sphere does not trigger"), Third->FindFieldHit(Third->GetActorLocation(), FVector(0, 4900, 100), Curtain));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBlackHoleBallTest, "HAL.FutureCreation.SpecialBall.BlackHoleDirectDamageAndGroundField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBlackHoleBallTest::RunTest(const FString&)
{
	FBombWorld F;
	const auto* Defaults = GetDefault<UHALMatchBallSettings>();
	const auto* Type = Defaults->BlackHoleBallType.LoadSynchronous();
	const auto* Solar = Defaults->ExplosiveBallType.LoadSynchronous();
	FString Error;
	FHALBlackHoleConfig Scaled;
	if (!TestTrue(TEXT("Both production balls load, including the original field and collapse"), Solar && Type
		&& Type->Validate(Error) && Type->ResolveField(Scaled) && Type->ExpirationVortex)) { AddError(Error); return false; }
	TestEqual(TEXT("Equal physical radius"), Type->Radius, Solar->Radius);
	TestTrue(TEXT("Equal visual scale"), Type->Visual.RelativeTransform.GetScale3D().Equals(Solar->Visual.RelativeTransform.GetScale3D()));
	const auto& Base = Type->FieldClass.GetDefaultObject()->GetBlackHoleConfig();
	TestTrue(TEXT("Ball scales the live authored field, including swirl, pulse, grip and both lifetime stages"),
		Scaled.MaximumRadius < Base.MaximumRadius && Scaled.ContinuousAcceleration < Base.ContinuousAcceleration
		&& Scaled.SwirlAcceleration < Base.SwirlAcceleration && Scaled.PulseDeltaSpeed < Base.PulseDeltaSpeed
		&& Scaled.DragGripMultiplier > Base.DragGripMultiplier && Scaled.SwirlDirection == Base.SwirlDirection
		&& Scaled.DurationSeconds < Base.DurationSeconds && Scaled.ExpansionSeconds < Base.ExpansionSeconds
		&& Scaled.PulseIndexAt(Scaled.ExpansionSeconds + Scaled.DurationSeconds - .01) >= 1);
	const auto MakeBox = [&](FVector Center, FVector Extent)
	{
		auto* Actor = F.World->SpawnActor<AActor>();
		auto* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box); Box->SetBoxExtent(Extent); Box->SetCollisionProfileName(TEXT("BlockAll"));
		Box->SetWorldLocation(Center); Box->RegisterComponent();
		return Box;
	};
	auto* Ground = MakeBox(FVector(0,0,-25), FVector(3000,3000,25));
	auto* Wall = MakeBox(FVector(600,0,150), FVector(20,250,150));
	auto* Shooter = F.Target(FVector(-220,0,100));
	auto* Victim = F.Target(FVector(0,0,100));
	auto* Bystander = F.Target(FVector(200,50,100));
	auto HP = [](AActor* Car) { return Car->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(); };
	const auto SpawnBall = [&]()
	{
		auto* Ball = F.World->SpawnActor<AHALBlackHoleBall>(FVector(-400,0,115), FRotator::ZeroRotator);
		Ball->PreInitializeComponents(); Ball->DispatchBeginPlay();
		return Ball;
	};
	const auto Fields = [&]()
	{
		TArray<AHALBlackHole*> Found;
		for (TActorIterator<AHALBlackHole> It(F.World); It; ++It) { Found.Add(*It); }
		return Found;
	};
	auto* Ball = SpawnBall();
	auto* Body = CastChecked<UPrimitiveComponent>(Victim->GetRootComponent());
	const FVector Contact(-67,0,98);
	FHitResult Direct(Victim, Body, Contact, -FVector::ForwardVector);
	Ball->ResolveLaunchedContact(Victim, Body, FVector::ZeroVector, Direct);
	TestEqual(TEXT("Free physical touches do not leave a black hole"), Fields().Num(), 0);
	TestTrue(TEXT("First holder can drop it, another can launch it"), Ball->BeginControl(Victim)
		&& Ball->ReleaseFromControl(Victim, .1f) && Ball->BeginControl(Shooter)
		&& Ball->LaunchFromControl(Shooter, Shooter, FVector(3200,0,0)));
	FHitResult Floor(Ground->GetOwner(), Ground, FVector(-190,0,0), FVector::UpVector);
	Ball->ResolveLaunchedContact(Ground->GetOwner(), Ground, FVector::ZeroVector, Floor);
	Ball->ResolveLaunchedContact(Shooter, CastChecked<UPrimitiveComponent>(Shooter->GetRootComponent()), FVector::ZeroVector, Direct);
	TestEqual(TEXT("Floor and last formal shooter pass through"), Fields().Num(), 0);
	const float Before = HP(Victim), NearHP = HP(Bystander);
	Ball->ResolveLaunchedContact(Victim, Body, FVector::ZeroVector, Direct);
	Ball->ResolveLaunchedContact(Victim, Body, FVector::ZeroVector, Direct);
	TestEqual(TEXT("One direct hit"), HP(Victim), Before - Type->Gameplay.Damage.VehicleHitDamage);
	TestEqual(TEXT("A nearby opponent receives no splash damage"), HP(Bystander), NearHP);
	auto Active = Fields();
	if (!TestEqual(TEXT("Repeated hit produces exactly one field"), Active.Num(), 1)) { return false; }
	TestTrue(TEXT("Real arena BP on the physical floor below the hit"), Active[0]->GetClass() == Type->FieldClass
		&& Active[0]->GetActorLocation().Equals(FVector(Contact.X, Contact.Y, 2), 1.));
	TestEqual(TEXT("World radius from ball DA"), Active[0]->GetBlackHoleConfig().MaximumRadius, Scaled.MaximumRadius);
	Active[0]->Destroy();
	auto* WallShot = SpawnBall();
	TestTrue(TEXT("Formal wall test shot"), WallShot->BeginControl(Shooter) && WallShot->LaunchFromControl(Shooter,Shooter,FVector(3200,0,0)));
	FHitResult Solid(Wall->GetOwner(), Wall, FVector(589,0,115), -FVector::ForwardVector);
	WallShot->ResolveLaunchedContact(Wall->GetOwner(), Wall, FVector::ZeroVector, Solid);
	Active = Fields();
	if (!TestEqual(TEXT("Solid obstacle also consumes the ball"), Active.Num(), 1)) { return false; }
	TestTrue(TEXT("Wall field sits on the incoming side and on the floor"), Active[0]->GetActorLocation().X < 580
		&& FMath::IsNearlyEqual(Active[0]->GetActorLocation().Z, 2., 1.));
	TestEqual(TEXT("Obstacle creates field without a damage event nearby"), HP(Bystander), NearHP);
	return true;
}

namespace
{
void AddMeteorTestFloor(UWorld& World, const FVector& Center, const FVector& Extent)
{
	auto* Floor = World.SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Floor);
	Floor->SetRootComponent(Box);
	Box->SetBoxExtent(Extent);
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->SetWorldLocation(Center);
	Box->RegisterComponent();
}

AHALSkillTestDummy* AddMeteorTestOpponent(UWorld& World, const FVector& At)
{
	auto* Class = LoadClass<AHALSkillTestDummy>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_SkillTestDummy.BP_SkillTestDummy_C"));
	if (!Class) { return nullptr; }
	auto* Dummy = World.SpawnActor<AHALSkillTestDummy>(Class, At, FRotator::ZeroRotator);
	Dummy->PreInitializeComponents(); Dummy->InitializeComponents(); Dummy->PostInitializeComponents(); Dummy->DispatchBeginPlay();
	return Dummy;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALTimedBombRulesTest, "HAL.FutureCreation.SpecialBall.TimedBomb.ContactClockAndVictims",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALTimedBombRulesTest::RunTest(const FString&)
{
	FBombWorld F;
	const auto* Type = GetDefault<UHALMatchBallSettings>()->TimedBombBallType.LoadSynchronous();
	FString Error;
	if (!TestTrue(TEXT("Production clasp, dial, and timer validate"),Type && Type->Validate(Error))) { AddError(Error); return false; }
	auto* A = AddMeteorTestOpponent(*F.World,FVector(0,0,120));
	auto* B = AddMeteorTestOpponent(*F.World,FVector(350,0,120));
	auto* C = AddMeteorTestOpponent(*F.World,FVector(350,340,120));
	auto* Far = F.Target(FVector(4500,0,120));
	if (!A || !B || !C) { return false; }
	auto HP = [](AActor* Car) { return Car->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(); };
	auto Spawn = [&]()
	{
		auto* Ball = F.World->SpawnActor<AHALTimedBombBall>(FVector(-750,0,120),FRotator::ZeroRotator);
		Ball->PreInitializeComponents(); Ball->DispatchBeginPlay(); return Ball;
	};
	auto* Ball = Spawn();
	auto* Other = Spawn();
	const float Full=HP(A), FarFull=HP(Far), Damage=Type->Gameplay.Damage.VehicleHitDamage;
	TestTrue(TEXT("No automatic pickup or launch, no fuse while patrolling"),!Ball->CanBeControlledBy(A) && !Ball->BeginControl(A)
		&& !Ball->AllowsNormalLaunch() && Ball->GetFuseDeadline()<0);
	auto* Root = CastChecked<UPrimitiveComponent>(A->GetRootComponent());
	Ball->OnContact(Ball->GetPhysicsRoot(),A,Root,FVector::ZeroVector,FHitResult(A,Root,A->GetActorLocation(),FVector::RightVector));
	if (!TestEqual(TEXT("Any-side chassis contact ignites and occupies slot"),Ball->GetControlledBy(),static_cast<AActor*>(A))) { return false; }
	const double Deadline=Ball->GetFuseDeadline();
	FVector Recoil(1,1,1);
	TestFalse(TEXT("Fire button rejected"),A->FindComponentByClass<UBallControlComponent>()->LaunchHeldBall(Recoil));
	TestTrue(TEXT("No shot or recoil"),Recoil.IsZero() && !Ball->LaunchFromControl(A,A,FVector(2000,0,0)));
	Ball->OnRam(*A,*B,FVector::ZeroVector,FVector::ZeroVector);
	TestEqual(TEXT("Standing contact cannot pass"),Ball->GetControlledBy(),static_cast<AActor*>(A));
	F.World->TimeSeconds += .35;
	if (!TestTrue(TEXT("First pickup also forcibly replaces an occupied slot"),Other->TryMount(*A,nullptr)
		&& Ball->GetBallState()==EBasicBallState::Free && Other->GetControlledBy()==A)) { return false; }
	TestEqual(TEXT("Ejected armed ball retains deadline"),Ball->GetFuseDeadline(),Deadline);
	TestFalse(TEXT("Cannot immediately grab the just-displaced bomb"),Ball->TryMount(*A,nullptr));
	if (!TestTrue(TEXT("Contact re-pickup then pass onto another armed carrier"),Ball->TryMount(*B,nullptr) && Ball->TryMount(*A,B))) { return false; }
	TestTrue(TEXT("Other fuse survives forced swap and credit follows active pass"),Other->GetBallState()==EBasicBallState::Free
		&& Other->GetFuseDeadline()>Deadline && Ball->Credit.InstigatorActor.Get()==B && Ball->ContactLocks.Contains(B));
	if (!TestTrue(TEXT("Third car can immediately accept"),Ball->TryMount(*C,A))) { return false; }
	TestEqual(TEXT("All handoffs preserve original clock"),Ball->GetFuseDeadline(),Deadline);
	TestEqual(TEXT("No damage for multiple tags"),HP(A)+HP(B)+HP(C),Full*3.f);
	// Exercise an authored non-default response and an AOE too small to reach the chassis from the clasp.
	auto* MountedType = DuplicateObject<UHALTimedBombBallDefinition>(Type, GetTransientPackage());
	MountedType->Gameplay.Impact.Tier = EHALImpactTier::Medium;
	MountedType->Gameplay.Impact.PushPreset = EHALImpactPushPreset::Long;
	MountedType->GroundBlastRadius = 1.f;
	Ball->Definition = MountedType;
	auto Impact = [](AActor* Car) { return Car->FindComponentByClass<UHALImpactReceiverComponent>()->GetLastImpact(); };
	const FGuid BeforeA = Impact(A).EventId, BeforeB = Impact(B).EventId, BeforeC = Impact(C).EventId, BeforeFar = Impact(Far).EventId;
	F.World->TimeSeconds = Deadline+.001;
	C->FindComponentByClass<UBallControlComponent>()->ReleaseForStun();
	TestTrue(TEXT("At deadline mounted detonation wins over same-tick drop"),Ball->IsActorBeingDestroyed() && !C->FindComponentByClass<UBallControlComponent>()->GetHeldBall());
	TestTrue(TEXT("Mounted damage is exclusive"),HP(C)==Full-Damage && HP(A)==Full && HP(B)==Full);
	const FHALImpactSummary MountedHit = Impact(C);
	TestTrue(TEXT("Mounted blast pushes its holder with the configured tier and distance"),
		MountedHit.EventId.IsValid() && MountedHit.EventId != BeforeC && MountedHit.EffectiveTier == MountedType->Gameplay.Impact.Tier
		&& MountedHit.PushPreset == MountedType->Gameplay.Impact.PushPreset && MountedHit.DesiredHorizontalDeltaSpeed > 0.f);
	TestTrue(TEXT("Nearby previous carriers receive no mounted blast impact"), Impact(A).EventId == BeforeA && Impact(B).EventId == BeforeB);
	Other->SetActorLocation(FVector(200,100,120),false,nullptr,ETeleportType::TeleportPhysics);
	F.World->TimeSeconds = Other->GetFuseDeadline()+.001;
	Other->Patrol();
	TestTrue(TEXT("Dropped bomb damages all in radius, including former holder"),HP(A)==Full-Damage && HP(B)==Full-Damage && HP(C)==Full-2*Damage && HP(Far)==FarFull);
	const FHALImpactSummary GroundHit = Impact(A);
	TestTrue(TEXT("Ground blast sends the ball's impact to every in-range chassis once"),
		GroundHit.EventId.IsValid() && GroundHit.EventId != MountedHit.EventId && GroundHit.EventId == Impact(B).EventId
		&& GroundHit.EventId == Impact(C).EventId && GroundHit.SourceTier == Type->Gameplay.Impact.Tier
		&& GroundHit.PushPreset == Type->Gameplay.Impact.PushPreset && Impact(Far).EventId == BeforeFar);
	Other->Detonate();
	int32 Ground=0, Mounted=0;
	for (TActorIterator<AHALSpecialBallCue> It(F.World); It; ++It)
	{
		const auto& Cue=It->GetCueState();
		if (Cue.Kind==EHALBallCue::Impact) { ++Ground; TestEqual(TEXT("Ground cue matches effective radius"),Cue.Radius,Type->GroundBlastRadius); }
		Mounted+=Cue.Kind==EHALBallCue::MountedImpact;
	}
	TestTrue(TEXT("Each event survives its ball exactly once"),Ground==1 && Mounted==1 && HP(A)==Full-Damage);
	auto* Idle = Spawn();
	Idle->Expire();
	TestTrue(TEXT("Unignited expiration is harmless"),Idle->IsActorBeingDestroyed() && HP(B)==Full-Damage);
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FHALMeteorStrikeTest, "HAL.FutureCreation.SpecialBall.Meteor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FHALMeteorStrikeTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	Names = {TEXT("DodgeAfterLockAndSelfDamage"),TEXT("EliminatedTargetKeepsLastGround")};
	Commands = Names;
}
bool FHALMeteorStrikeTest::RunTest(const FString& Scenario)
{
	auto F = MakeShared<FBombWorld>();
	auto* Type = GetDefault<UHALMatchBallSettings>()->MeteorBallType.LoadSynchronous();
	FString Error;
	if (!TestTrue(TEXT("Authored type and both marker layers load"), Type && Type->Validate(Error)))
	{ AddError(Error); return false; }
	AddMeteorTestFloor(*F->World, FVector(0,0,-25), FVector(6000,6000,25));
	auto* Shooter = F->Target(FVector(1050,0,100));
	auto* Bystander = F->Target(FVector(1500,0,100));
	auto* Distant = F->Target(FVector(-2300,0,100));
	auto* Ball = F->World->SpawnActor<AHALMeteorBall>(FVector(600,0,130), FRotator::ZeroRotator);
	Ball->PreInitializeComponents();
	Ball->DispatchBeginPlay();
	TestTrue(TEXT("Can hold without an eligible opponent"), Ball->BeginControl(Shooter));
	auto* Dummy = AddMeteorTestOpponent(*F->World, FVector(500,0,105));
	if (!TestNotNull(TEXT("Skill test dummy"), Dummy)) { return false; }
	if (!TestTrue(TEXT("Eligible opponent allows formal launch"), Dummy->CanReceiveSkillEffects()
		&& Ball->LaunchFromControl(Shooter,Shooter,FVector::ForwardVector*3600))) { return false; }
	TestTrue(TEXT("Consumed ball immediately loses its physics presence"), Ball->IsActorBeingDestroyed() && !Ball->GetActorEnableCollision());
	TActorIterator<AHALMeteorStrike> It(F->World);
	if (!TestTrue(TEXT("Single independent strike"), bool(It))) { return false; }
	auto* Strike = *It;
	++It;
	TestFalse(TEXT("Only one strike on launch"), bool(It));
	Strike->DispatchBeginPlay();
	TestEqual(TEXT("Lock identifies the opponent"), Strike->GetStrikeState().Target.Get(), static_cast<ATestVehiclePawn*>(Dummy));
	TestFalse(TEXT("A chosen opponent keeps the normal warning timing"), Strike->GetStrikeState().bGroundTargeted);

	const FVector Mark(1250,0,100);
	Dummy->SetActorLocation(Mark,false,nullptr,ETeleportType::TeleportPhysics);
	auto HP = [](AActor* Car) { return Car->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(); };
	const float OwnHP = HP(Shooter), OtherHP = HP(Bystander), FarHP = HP(Distant), TargetHP = HP(Dummy);
	const bool bEliminate = Scenario == TEXT("EliminatedTargetKeepsLastGround");
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FFunctionLatentCommand>(
		[this,F,Type,Dummy,Shooter,Bystander,Distant,Strike,HP,OwnHP,OtherHP,FarHP,TargetHP,bEliminate,
			bReleased=false,bLocked=false,Elapsed=0.f,ExpectedGround=Mark,FrozenGround=FVector::ZeroVector]() mutable
		{
			F->World->Tick(LEVELTICK_All,.05f);
			Elapsed += .05f;
			const auto& S = Strike->GetStrikeState();
			if (bEliminate && !bReleased && Elapsed > Type->TrackingSeconds*.4f)
			{
				ExpectedGround = Dummy->GetActorLocation();
				Dummy->GetAbilitySystemComponent()->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(),0.f);
				TestNull(TEXT("Elimination removes the tracking marker immediately"), S.Target.Get());
				Dummy->SetActorLocation(FVector(3200,800,100),false,nullptr,ETeleportType::TeleportPhysics);
				bReleased = true;
			}
			if (S.Phase == EHALMeteorPhase::Falling && !bLocked)
			{
				if (!bEliminate) { ExpectedGround = Dummy->GetActorLocation(); }
				ExpectedGround.Z = 0.;
				FrozenGround = S.Ground;
				TestTrue(TEXT("Locks the actual floor beneath the moving chassis"), FrozenGround.Equals(ExpectedGround,3.));
				TestNull(TEXT("No moving target during descent"), S.Target.Get());
				TestEqual(TEXT("Warning deals no early damage"), HP(Bystander),OtherHP);
				if (!bEliminate) { Dummy->SetActorLocation(FVector(3200,800,100),false,nullptr,ETeleportType::TeleportPhysics); }
				bLocked = true;
			}
			if (S.Phase != EHALMeteorPhase::Impact)
			{
				if (Elapsed < Type->TrackingSeconds+Type->FallSeconds+2.f) { return false; }
				AddError(TEXT("Meteor never reached impact")); return true;
			}
			TestTrue(TEXT("Descent precedes impact and final ground is immutable"), bLocked && FVector(S.Ground).Equals(FrozenGround,.01));
			const float Damage = Type->Gameplay.Damage.VehicleHitDamage;
			TestEqual(TEXT("Shooter inside radius receives the same attack once"),HP(Shooter),OwnHP-Damage);
			TestEqual(TEXT("Nearby bystander hit once"),HP(Bystander),OtherHP-Damage);
			TestEqual(TEXT("Outside target untouched"),HP(Distant),FarHP);
			TestEqual(TEXT("Selected target can escape or remain eliminated"),HP(Dummy),bEliminate ? 0.f : TargetHP);
			return true;
		}));
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FHALUntargetedMeteorTest, "HAL.FutureCreation.SpecialBall.Meteor.Untargeted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FHALUntargetedMeteorTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	Names = {TEXT("ClearDeck"), TEXT("ShortDeck"), TEXT("NoSafeGround")};
	Commands = Names;
}
bool FHALUntargetedMeteorTest::RunTest(const FString& Scenario)
{
	auto F = MakeShared<FBombWorld>();
	const auto* Type = GetDefault<UHALMatchBallSettings>()->MeteorBallType.LoadSynchronous();
	if (!TestNotNull(TEXT("Saved meteor ball type"), Type)) { return false; }
	const FVector Origin(-350,400,100);
	auto* Shooter = F->Target(Origin);
	Shooter->SetActorRotation(FRotator(0,90,0), ETeleportType::TeleportPhysics);
	const bool bNoGround = Scenario == TEXT("NoSafeGround");
	const bool bShort = Scenario == TEXT("ShortDeck");
	const float SafeDistance = Type->DamageRadius + Shooter->GetSimpleCollisionRadius() + 1.f;
	const float Preferred = FMath::Max(Type->UntargetedDistance, SafeDistance);
	const float DeckAhead = bNoGround ? SafeDistance * .65f : bShort ? (SafeDistance + Preferred) * .5f + 2.f : Preferred + 500.f;
	AddMeteorTestFloor(*F->World, FVector(Origin.X,Origin.Y,-25.), FVector(2200,DeckAhead,25));
	auto* Ball = F->World->SpawnActor<AHALMeteorBall>(Origin + FVector(0,250,20), FRotator::ZeroRotator);
	Ball->PreInitializeComponents(); Ball->DispatchBeginPlay();
	if (!TestTrue(TEXT("Solo holder can fire the meteor"), Ball->BeginControl(Shooter)
		&& Ball->LaunchFromControl(Shooter,Shooter,FVector(1100,3000,40)))) { return false; }
	TestTrue(TEXT("The shot spends the held ball"), Ball->IsActorBeingDestroyed());
	TestFalse(TEXT("A repeat input cannot spend it again"), Ball->LaunchFromControl(Shooter,Shooter,FVector(0,3000,0)));
	TActorIterator<AHALMeteorStrike> It(F->World);
	if (bNoGround)
	{
		TestFalse(TEXT("No strike without safe ground"), bool(It));
		int32 Dispersals = 0;
		for (TActorIterator<AHALSpecialBallCue> Cue(F->World); Cue; ++Cue)
		{ Dispersals += Cue->GetCueState().Kind == EHALBallCue::Expiration; }
		TestEqual(TEXT("One harmless dispersal frees the slot"), Dispersals, 1);
		return true;
	}
	if (!TestTrue(TEXT("One fixed strike exists"), bool(It))) { return false; }
	auto* Strike = *It;
	++It;
	TestFalse(TEXT("Repeated launch did not duplicate the strike"), bool(It));
	Strike->DispatchBeginPlay();
	const auto& State = Strike->GetStrikeState();
	const FVector Ground = State.Ground;
	TestTrue(TEXT("Ground warning starts on launch without a vehicle marker"), State.bGroundTargeted && !State.Target);
	TestTrue(TEXT("Ground follows chassis yaw on the drive surface"), FMath::IsNearlyEqual(Ground.X,Origin.X,1.)
		&& FMath::IsNearlyZero(Ground.Z,1.) && Ground.Y > Origin.Y);
	const double Distance = Ground.Y - Origin.Y;
	TestTrue(TEXT("Fits inside the deck outside initial self damage"), Distance >= SafeDistance - .2 && Distance <= DeckAhead);
	TestTrue(TEXT("Uses the DA distance unless the deck ends first"), bShort ? Distance < Preferred : FMath::IsNearlyEqual(Distance, double(Preferred), 1.));
	Shooter->SetActorRotation(FRotator(0,-90,0), ETeleportType::TeleportPhysics);
	auto* LateOpponent = AddMeteorTestOpponent(*F->World, Ground + FVector(80,0,100));
	if (!TestNotNull(TEXT("A vehicle enters the marked area after launch"), LateOpponent)) { return false; }
	auto* VictimHP = LateOpponent->FindComponentByClass<UVehicleHealthComponent>();
	auto* ShooterHP = Shooter->FindComponentByClass<UVehicleHealthComponent>();
	const float Before = VictimHP->GetCurrentHP(), OwnBefore = ShooterHP->GetCurrentHP();
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FFunctionLatentCommand>(
		[this,F,Type,Strike,Ground,VictimHP,ShooterHP,Before,OwnBefore,Elapsed=0.f,bSawFall=false]() mutable
		{
			F->World->Tick(LEVELTICK_All,.05f);
			Elapsed += .05f;
			const auto& S = Strike->GetStrikeState();
			if (S.Target || !FVector(S.Ground).Equals(Ground,.1))
			{ AddError(TEXT("Fixed aim changed after launch")); return true; }
			if (S.Phase != EHALMeteorPhase::Impact)
			{
				bSawFall |= S.Phase == EHALMeteorPhase::Falling;
				if (VictimHP->GetCurrentHP() != Before || Elapsed > Type->TrackingSeconds + Type->FallSeconds + 1.f)
				{ AddError(TEXT("Fixed strike damaged early or never impacted")); return true; }
				return false;
			}
			TestTrue(TEXT("Normal fall window precedes the impact"), bSawFall);
			TestEqual(TEXT("Late entrant takes one normal hit"), VictimHP->GetCurrentHP(), Before - Type->Gameplay.Damage.VehicleHitDamage);
			TestEqual(TEXT("Stationary shooter stays outside the blast"), ShooterHP->GetCurrentHP(), OwnBefore);
			return true;
		}));
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
