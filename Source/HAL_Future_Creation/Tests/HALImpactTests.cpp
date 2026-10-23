#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Curves/CurveFloat.h"
#include "HALVehicleStabilityTestRules.h"
#include "../HALImpactTypes.h"
#include "../HALImpactAttributeSet.h"
#include "../HALImpactReceiverComponent.h"
#include "../HALImpactSourceComponent.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALGameplayTags.h"
#include "../HALHealthAttributeSet.h"
#include "../CombatResolver.h"
#include "../SpringPunchGimmick.h"
#include "../PassiveTestVehicle.h"
#include "../BasicBallActor.h"
#include "../TestVehiclePawn.h"
#include "../HALPlayerState.h"
#include "../BallControlComponent.h"
#include "../VehicleKnockbackSettings.h"
#include "Components/PrimitiveComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "GameplayEffect.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace
{
	struct FImpactWorld
	{
		FScopedHALStabilityTestRules Stability;
		UVehicleKnockbackSettings* Settings = GetMutableDefault<UVehicleKnockbackSettings>();
		EVehicleConfigurationSource OldSource = Settings->ConfigurationSource;
		TSoftObjectPtr<UCombatRulesDefinition> OldAsset = Settings->CombatRules;
		UCombatRulesDefinition* Rules;
		UWorld* World;
		void Reload()
		{
			FPropertyChangedEvent Event(nullptr);
			Settings->PostEditChangeProperty(Event);
			Settings->InitializeRules();
		}
		FImpactWorld(bool bSharedStability = false) : Stability(bSharedStability)
		{
			Settings->InitializeRules();
			Rules = NewObject<UCombatRulesDefinition>();
			Rules->bReadyForUse = true;
			Settings->GetRuntimeRules(Rules->BallImpacts);
			Rules->CollisionImpacts.Version = EHALImpactVersion::TieredV1;
			Settings->ConfigurationSource = EVehicleConfigurationSource::Definition;
			Settings->CombatRules = Rules;
			Reload();
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
				.ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		}
		~FImpactWorld()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
			Settings->ConfigurationSource = OldSource;
			Settings->CombatRules = OldAsset;
			Reload();
		}
		APassiveTestVehicle* Target(EHALImpactTier Cap = EHALImpactTier::Heavy)
		{
			auto* Actor = World->SpawnActor<APassiveTestVehicle>();
			auto* Def = NewObject<UVehicleDefinition>();
			Def->bReadyForUse = true;
			Def->CollisionShape = EVehicleCollisionShape::Box;
			Def->Visual.Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			Def->Combat.Version = EHALCombatConfigVersion::AttackSnapshotV1;
			Def->Impact.Version = EHALImpactVersion::TieredV1;
			Def->Impact.CollisionTierCap = Cap;
			const FEnumProperty* Source = FindFProperty<FEnumProperty>(Actor->GetClass(), TEXT("ConfigurationSource"));
			Source->GetUnderlyingProperty()->SetIntPropertyValue(Source->ContainerPtrToValuePtr<void>(Actor), uint64(EVehicleConfigurationSource::Definition));
			FindFProperty<FObjectProperty>(Actor->GetClass(), TEXT("Definition"))->SetObjectPropertyValue_InContainer(Actor, Def);
			Actor->PreInitializeComponents();
			Actor->DispatchBeginPlay();
			return Actor;
		}
	};
	UHALAbilitySystemComponent* ASC(APassiveTestVehicle* Target) { return CastChecked<UHALAbilitySystemComponent>(Target->GetAbilitySystemComponent()); }
	UPrimitiveComponent* Body(AActor* Actor) { return CastChecked<UPrimitiveComponent>(Actor->GetRootComponent()); }
	FHALCombatHitRequest Request(FHALCombatAction& Action, AActor* Target, EHALImpactTier Tier, bool Damage = true)
	{
		FHALCombatHitRequest Out;
		Out.AttackId = Action.GetSnapshot().AttackId;
		Out.Hit.TargetActor = Target;
		Out.Hit.TargetPhysicsBody = Body(Target);
		Out.Hit.ImpactNormal = FVector(-1, 0, 0);
		Out.bApplyDamage = Damage;
		Out.bApplyImpact = true;
		Out.Impact.Mode = EHALImpactMode::ExplicitTier;
		Out.Impact.Tier = Tier;
		return Out;
	}
	void Begin(FHALCombatAction& Action, AActor* Source, float Damage = 25)
	{
		FHALAttackSourceSnapshot Snapshot;
		Snapshot.SourceActor = Source;
		Snapshot.FixedDamage = Damage;
		Action.Begin(*Source, Snapshot);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALImpactTierTest, "HAL.FutureCreation.Impact.TiersCalibrationAndEnergyBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALImpactTierTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Saved legacy Light index unchanged"), uint8(EVehicleKnockbackTier::Light), uint8(0));
	FHALCollisionImpactRules Rules;
	TestEqual(TEXT("New C++ defaults cannot activate migration"), Rules.Version, EHALImpactVersion::Legacy);
	TestEqual(TEXT("Support is None"), Rules.SelectTier(49, EHALImpactTier::Heavy), EHALImpactTier::None);
	TestEqual(TEXT("Inclusive Medium threshold"), Rules.SelectTier(125, EHALImpactTier::Heavy), EHALImpactTier::Medium);
	TestEqual(TEXT("Source capability caps automatic classification"), Rules.SelectTier(500, EHALImpactTier::Light), EHALImpactTier::Light);
	TestEqual(TEXT("One tier reduction"), HALImpact::Reduce(EHALImpactTier::Heavy, 1, false), EHALImpactTier::Medium);
	TestEqual(TEXT("Reduction clamps to None"), HALImpact::Reduce(EHALImpactTier::Light, 3, false), EHALImpactTier::None);
	TestEqual(TEXT("Immunity blocks rule reaction"), HALImpact::Reduce(EHALImpactTier::Heavy, 0, true), EHALImpactTier::None);
	FHALVehicleImpactConfig Vehicle;
	Vehicle.MinimumDropTier = EHALImpactTier::None;
	FString Error;
	TestFalse(TEXT("None cannot accidentally mean drop on every impact"), Vehicle.Validate(Error));
	Vehicle.bDropOnImpact = false;
	TestTrue(TEXT("Disabling impact drop has its own explicit switch"), Vehicle.Validate(Error));
	// Wall: incoming -100, solver gives +100, resulting speed 0. Supplement <=15, never target 900.
	TestTrue(TEXT("Wall bounce cannot inject full tier energy"), FMath::IsNearlyEqual(HALImpact::PhysicalSupplement(900, 100, 0, 0, 0.15f), 15.0f, 0.001f));
	TestEqual(TEXT("Already separating fast: no additional wall impulse"), HALImpact::PhysicalSupplement(900, 100, 50, 0, 0.15f), 0.0f);
	TestEqual(TEXT("Solver already exceeds authored response"), HALImpact::PhysicalSupplement(250, 300, 0, 0, 0.15f), 0.0f);
	const float A = HALImpact::PhysicalSupplement(900, 100, 0, 1, 0.15f);
	TestTrue(TEXT("Two equal vehicles share relative budget instead of doubling"), FMath::IsNearlyEqual(A * 2, 30.0f, 0.001f));
	TestEqual(TEXT("Invalid measurement fails closed"), HALImpact::PhysicalSupplement(900, std::numeric_limits<float>::infinity(), 0, 0, 0.15f), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALImpactResistanceTest, "HAL.FutureCreation.Impact.GASResistanceAndIndependentDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALImpactResistanceTest::RunTest(const FString& Parameters)
{
	FImpactWorld F;
	auto* Target = F.Target();
	AActor* Source = F.World->SpawnActor<AActor>(); // A hazard does not need an ASC.
	auto* Gas = ASC(Target);
	UGameplayEffect* Buff = NewObject<UGameplayEffect>();
	Buff->DurationPolicy = EGameplayEffectDurationType::Infinite;
	auto& Modifier = Buff->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UHALImpactAttributeSet::GetTierReductionAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FScalableFloat(1);
	const auto Handle = Gas->ApplyGameplayEffectToSelf(Buff, 1, Gas->MakeEffectContext());
	FHALCombatAction Reduced;
	Begin(Reduced, Source);
	const auto Hit = Request(Reduced, Target, EHALImpactTier::Heavy);
	const auto Result = FCombatResolver::ResolveCombatHit(Reduced, Hit);
	TestEqual(TEXT("GE lowers Heavy to Medium"), Result.ImpactSummary.EffectiveTier, EHALImpactTier::Medium);
	TestEqual(TEXT("Impact resistance does not reduce damage"), Result.ActualHPRemoved, 25.0f);
	TestTrue(TEXT("Independent impact applied"), Result.bImpactApplied);
	TestEqual(TEXT("Repeated pulse never applies twice"), FCombatResolver::ResolveCombatHit(Reduced, Hit).RejectReason, EHALHitRejectReason::Duplicate);
	Gas->RemoveActiveGameplayEffect(Handle);
	TestEqual(TEXT("GE removal restores resistance"), Gas->GetSet<UHALImpactAttributeSet>()->GetTierReduction(), 0.0f);
	Gas->AddLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
	Body(Target)->SetPhysicsLinearVelocity(FVector(123, 0, -10));
	FHALCombatAction Immune;
	Begin(Immune, Source);
	const auto ImmuneResult = FCombatResolver::ResolveCombatHit(Immune, Request(Immune, Target, EHALImpactTier::Heavy));
	TestEqual(TEXT("Impact immunity keeps full damage"), ImmuneResult.ActualHPRemoved, 25.0f);
	TestEqual(TEXT("Impact immune outcome explicit"), ImmuneResult.ImpactOutcome, EHALImpactOutcome::Suppressed);
	TestTrue(TEXT("No cancellation of natural velocity"), Body(Target)->GetPhysicsLinearVelocity().Equals(FVector(123, 0, -10)));
	Gas->RemoveLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
	Gas->AddLooseGameplayTag(HALGameplayTags::State_Combat_DamageImmune);
	FHALCombatAction NoDamage;
	Begin(NoDamage, Source);
	const auto NoDamageResult = FCombatResolver::ResolveCombatHit(NoDamage, Request(NoDamage, Target, EHALImpactTier::Heavy));
	TestTrue(TEXT("Damage immunity does not stop Heavy"), NoDamageResult.bImpactApplied && NoDamageResult.ActualHPRemoved == 0);
	Gas->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(), 0);
	FHALCombatAction ZeroHP;
	Begin(ZeroHP, Source, 0);
	TestTrue(TEXT("Zero HP/zero damage still valid impact"), FCombatResolver::ResolveCombatHit(ZeroHP, Request(ZeroHP, Target, EHALImpactTier::Medium)).bImpactApplied);
	FHALCombatAction None;
	Begin(None, Source);
	Gas->RemoveLooseGameplayTag(HALGameplayTags::State_Combat_DamageImmune);
	const auto NoneResult = FCombatResolver::ResolveCombatHit(None, Request(None, Target, EHALImpactTier::None));
	TestFalse(TEXT("None does not move or drop"), NoneResult.bImpactApplied || NoneResult.ImpactSummary.bBallDropped);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALImpactContactTest, "HAL.FutureCreation.Impact.AllSourcesAndContactEpisodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALImpactContactTest::RunTest(const FString& Parameters)
{
	FImpactWorld F;
	auto* Target = F.Target();
	auto* Receiver = Target->FindComponentByClass<UHALImpactReceiverComponent>();
	AActor* Wall = F.World->SpawnActor<AActor>();
	UBoxComponent* WallBody = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(WallBody);
	WallBody->RegisterComponent();
	FHitResult Hit;
	Hit.ImpactNormal = FVector(1, 0, 0);
	Receiver->HandleContact(Body(Target), Wall, WallBody, FVector(10, 0, 0), Hit);
	TestFalse(TEXT("Support cannot create an impact event"), Receiver->GetLastImpact().EventId.IsValid());
	Receiver->HandleContact(Body(Target), Wall, WallBody, FVector(120000, 0, 0), Hit);
	const auto First = Receiver->GetLastImpact();
	TestEqual(TEXT("Ordinary wall needs no special Actor or ASC"), First.SourceTier, EHALImpactTier::Medium);
	Receiver->HandleContact(Body(Target), Wall, WallBody, FVector(800000, 0, 0), Hit);
	TestEqual(TEXT("Continuous contact is one episode"), Receiver->GetLastImpact().EventId, First.EventId);
	F.World->Tick(LEVELTICK_TimeOnly, 0.3f);
	Receiver->HandleContact(Body(Target), Wall, WallBody, FVector(800000, 0, 0), Hit);
	TestNotEqual(TEXT("Separated later collision rearms"), Receiver->GetLastImpact().EventId, First.EventId);
	auto* Vehicle = F.Target(EHALImpactTier::Light);
	// Source content is initialized before play in production; use a separate component for this test source.
	AActor* Special = F.World->SpawnActor<AActor>();
	UBoxComponent* SpecialBody = NewObject<UBoxComponent>(Special);
	Special->SetRootComponent(SpecialBody);
	SpecialBody->RegisterComponent();
	auto* Profile = NewObject<UHALImpactSourceComponent>(Special);
	Special->AddInstanceComponent(Profile);
	Profile->Mode = EHALCollisionSourceMode::AutomaticCap;
	Profile->Tier = EHALImpactTier::Light;
	Profile->RegisterComponent();
	Receiver->HandleContact(Body(Target), Special, SpecialBody, FVector(800000, 0, 0), Hit);
	TestEqual(TEXT("Optional environment profile caps source strength"), Receiver->GetLastImpact().SourceTier, EHALImpactTier::Light);
	Receiver->HandleContact(Body(Target), Vehicle, Body(Vehicle), FVector(800000, 0, 0), Hit);
	TestEqual(TEXT("Vehicle contact uses source capability"), Receiver->GetLastImpact().SourceTier, EHALImpactTier::Light);
	auto* Ball = F.World->SpawnActor<ABasicBallActor>();
	Receiver->HandleContact(Body(Target), Ball, Ball->GetPhysicsRoot(), FVector(800000, 0, 0), Hit);
	TestEqual(TEXT("Free ball has shared Light cap"), Receiver->GetLastImpact().SourceTier, EHALImpactTier::Light);
	TestEqual(TEXT("Natural contacts never deal vehicle damage"), ASC(Target)->GetSet<UHALHealthAttributeSet>()->GetCurrentHP(), 100.0f);
	F.World->Tick(LEVELTICK_TimeOnly, 0.3f);
	Profile->Mode = EHALCollisionSourceMode::FixedTier;
	Profile->Tier = EHALImpactTier::Heavy;
	Receiver->HandleContact(Body(Target), Special, SpecialBody, FVector(50000, 0, 0), Hit);
	TestEqual(TEXT("Authored collision source can choose fixed tier after meaningful-contact gate"), Receiver->GetLastImpact().SourceTier, EHALImpactTier::Heavy);
	for (AActor* NaturalSource : {Wall, static_cast<AActor*>(Vehicle), static_cast<AActor*>(Ball)})
	{
		auto* Fresh = F.Target();
		ASC(Fresh)->AddLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
		auto* FreshReceiver = Fresh->FindComponentByClass<UHALImpactReceiverComponent>();
		FreshReceiver->HandleContact(Body(Fresh), NaturalSource, Body(NaturalSource), FVector(800000, 0, 0), Hit);
		TestTrue(TEXT("Wall, vehicle and free-ball immunity share one result"), FreshReceiver->GetLastImpact().bImmune
			&& FreshReceiver->GetLastImpact().EffectiveTier == EHALImpactTier::None);
	}
	const auto BeforeClient = Receiver->GetLastImpact().EventId;
	Target->SetRole(ROLE_SimulatedProxy);
	Receiver->HandleContact(Body(Target), F.World->SpawnActor<AActor>(), WallBody, FVector(800000, 0, 0), Hit);
	TestEqual(TEXT("Client callback never resolves gameplay"), Receiver->GetLastImpact().EventId, BeforeClient);
	Target->SetRole(ROLE_Authority);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALImpactActionTest, "HAL.FutureCreation.Impact.AuthorityPhysicsReplayContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALImpactActionTest::RunTest(const FString& Parameters)
{
	FVehicleImpactAction Action;
	Action.EventSequence = 9;
	Action.DeltaVelocity = FVector(600, 0, 0);
	Action.bMinimumUpwardSpeed = true;
	Action.MinimumUpwardSpeed = 500;
	Action.bSetPlanarAngularVelocity = true;
	Action.PlanarAngularVelocity = FVector(0, 8, 0);
	TestEqual(TEXT("Impact action is authored on authority"), uint8(Action.GetAuthorStyle()), uint8(FNetworkPhysicsActionPayload::EActionAuthorStyle::Authority));
	TestTrue(TEXT("Same physics state produces identical replay delta"), Action.ResolveLinearDelta(FVector(0, 0, 100)).Equals(FVector(600, 0, 400)));
	TestTrue(TEXT("Existing higher vertical speed preserved"), Action.ResolveLinearDelta(FVector(0, 0, 700)).Equals(FVector(600, 0, 0)));
	TestTrue(TEXT("Yaw preserved while authored flip replays"), Action.ResolveAngularVelocity(FVector(1, 2, 3)).Equals(FVector(0, 8, 3)));
	Action.bMinimumUpwardSpeed = false;
	TestTrue(TEXT("Ordinary physical contact never cancels falling"), Action.ResolveLinearDelta(FVector(0, 0, -100)).Equals(FVector(600, 0, 0)));
	FVehicleImpactAction Copy = Action;
	TestTrue(TEXT("Stable payload equality"), Action.IsNearlyEqual(Copy));
	Copy.EventSequence++;
	TestFalse(TEXT("Distinct event stays distinct"), Action.IsNearlyEqual(Copy));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALImpactBallPipelineTest, "HAL.FutureCreation.Impact.BallSnapshotDropAndCallbackMerge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALImpactBallPipelineTest::RunTest(const FString& Parameters)
{
	FImpactWorld F(true);
	auto* Launcher = F.Target();
	auto* Target = F.Target();
	auto* Ball = F.World->SpawnActor<ABasicBallActor>();
	Ball->RuntimeImpactConfig.Version = EHALImpactVersion::TieredV1;
	Ball->RuntimeImpactConfig.Tier = EHALImpactTier::Heavy;
	Ball->DispatchBeginPlay();
	TestTrue(TEXT("Ball can be acquired"), Ball->BeginControl(Launcher));
	TestTrue(TEXT("Ball captures fixed tier at launch"), Ball->LaunchFromControl(Launcher, Launcher, FVector(1000, 0, 0)));
	Ball->RuntimeImpactConfig.Tier = EHALImpactTier::Light;
	FHitResult Hit;
	Hit.ImpactNormal = FVector(-1, 0, 0);
	auto* Receiver = Target->FindComponentByClass<UHALImpactReceiverComponent>();
	Receiver->HandleContact(Body(Target), Ball, Ball->GetPhysicsRoot(), FVector(200000, 0, 0), Hit);
	TestFalse(TEXT("Vehicle callback does not resolve launched ball"), Receiver->GetLastImpact().EventId.IsValid());
	Ball->OnPhysicsRootHit(Ball->GetPhysicsRoot(), Target, Body(Target), FVector(-200000, 0, 0), Hit);
	TestEqual(TEXT("Hit consumes captured Heavy even after source config changes"), Receiver->GetLastImpact().SourceTier, EHALImpactTier::Heavy);
	const auto Accepted = Receiver->GetLastImpact().EventId;
	TestEqual(TEXT("Attack consumed"), Ball->GetBallState(), EBasicBallState::Free);
	Receiver->HandleContact(Body(Target), Ball, Ball->GetPhysicsRoot(), FVector(200000, 0, 0), Hit);
	TestEqual(TEXT("Free-state vehicle callback merges into the attack contact"), Receiver->GetLastImpact().EventId, Accepted);
	TestEqual(TEXT("One damage application"), ASC(Target)->GetSet<UHALHealthAttributeSet>()->GetCurrentHP(), 75.0f);

	auto* Pawn = F.World->SpawnActor<ATestVehiclePawn>();
	CastChecked<UStaticMeshComponent>(Pawn->GetRootComponent())->SetStaticMesh(
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	auto* State = F.World->SpawnActor<AHALPlayerState>();
	Pawn->SetPlayerState(State);
	FHALVehicleImpactConfig Config;
	Config.Version = EHALImpactVersion::TieredV1;
	auto* PawnReceiver = Pawn->FindComponentByClass<UHALImpactReceiverComponent>();
	PawnReceiver->ApplyConfiguration(Config);
	Pawn->PreInitializeComponents();
	AddExpectedMessage(TEXT("has simple collision but no convex hull"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1); // Engine cube has valid simple box collision.
	Pawn->DispatchBeginPlay();
	auto* Control = Pawn->FindComponentByClass<UBallControlComponent>();
	auto* Held = F.World->SpawnActor<ABasicBallActor>();
	Held->BeginControl(Pawn);
	Control->HeldBall = Held;
	auto* PawnASC = State->GetHALAbilitySystemComponent();
	PawnASC->SetNumericAttributeBase(UHALImpactAttributeSet::GetTierReductionAttribute(), 1);
	AActor* Hazard = F.World->SpawnActor<AActor>();
	FHALCombatAction Medium;
	Begin(Medium, Hazard, 0);
	const auto LightResult = FCombatResolver::ResolveCombatHit(Medium, Request(Medium, Pawn, EHALImpactTier::Medium, false));
	TestEqual(TEXT("Medium reduced to Light"), LightResult.ImpactSummary.EffectiveTier, EHALImpactTier::Light);
	TestEqual(TEXT("Light below minimum drop keeps held ball"), Control->GetHeldBall(), Held);
	Control->HandleVehicleCollision(FVector(1e8, 0, 0), Hit);
	TestEqual(TEXT("Raw impulse cannot bypass tier resistance"), Control->GetHeldBall(), Held);
	PawnASC->AddLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
	FHALCombatAction Immune;
	Begin(Immune, Hazard, 0);
	FCombatResolver::ResolveCombatHit(Immune, Request(Immune, Pawn, EHALImpactTier::Heavy, false));
	TestEqual(TEXT("Immunity prevents rule drop"), Control->GetHeldBall(), Held);
	PawnASC->RemoveLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
	FHALCombatAction Heavy;
	Begin(Heavy, Hazard, 0);
	auto* Floor = NewObject<UBoxComponent>(Hazard); Hazard->SetRootComponent(Floor);
	Floor->SetMobility(EComponentMobility::Movable); Floor->SetCollisionObjectType(ECC_WorldStatic); Floor->RegisterComponent();
	auto SupportHit = Request(Heavy, Pawn, EHALImpactTier::Heavy, false);
	SupportHit.Impact.Mode = EHALImpactMode::PhysicalContact;
	SupportHit.Hit.SourceActor = Hazard; SupportHit.Hit.SourcePhysicsBody = Floor;
	SupportHit.Hit.ImpactNormal = FVector::UpVector; SupportHit.Hit.NormalImpulse = FVector(0,0,800000);
	const auto Drop = FCombatResolver::ResolveCombatHit(Heavy, SupportHit);
	TestTrue(TEXT("Effective Medium drops exactly once"), Drop.ImpactSummary.bBallDropped && Control->GetHeldBall() == nullptr);
	TestEqual(TEXT("Dropped ball returns Free"), Held->GetBallState(), EBasicBallState::Free);
	TestFalse(TEXT("Drop preserves existing reacquire lock"), Held->CanBeControlledBy(Pawn));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSpringPunchCombatTest, "HAL.FutureCreation.SpringPunch.CombatLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALSpringPunchCombatTest::RunTest(const FString& Parameters)
{
	FImpactWorld F;
	auto* Punch = F.World->SpawnActor<ASpringPunchGimmick>();
	auto* A = F.Target();
	auto* B = F.Target();
	A->SetActorLocation(FVector(200, 0, 0));
	B->SetActorLocation(FVector(300, 0, 0));
	auto Hit = [](AActor* Target)
	{
		return FHitResult(Target, Body(Target), Target->GetActorLocation(), FVector::UpVector);
	};
	Punch->EnterState(ESpringPunchState::Extending);
	const FGuid FirstId = Punch->PunchAction.GetSnapshot().AttackId;
	const auto First = Punch->ResolvePunchHit(Hit(A));
	TestTrue(TEXT("Fixed damage and authored impact accepted"), First.bHitAccepted && First.bImpactApplied);
	TestEqual(TEXT("Default damage"), First.ActualHPRemoved, 25.0f);
	TestEqual(TEXT("Default tier"), First.ImpactSummary.EffectiveTier, EHALImpactTier::Medium);
	TestEqual(TEXT("Same punch repeated callback rejected"), Punch->ResolvePunchHit(Hit(A)).RejectReason, EHALHitRejectReason::Duplicate);
	TestTrue(TEXT("Same punch accepts second target"), Punch->ResolvePunchHit(Hit(B)).bHitAccepted);
	Punch->EnterState(ESpringPunchState::Retracting);
	TestFalse(TEXT("Retracting cannot deal damage"), Punch->ResolvePunchHit(Hit(A)).bHitAccepted);
	TestFalse(TEXT("Phase end closes action"), Punch->PunchAction.IsActive());
	Punch->EnterState(ESpringPunchState::Extending);
	TestTrue(TEXT("Next punch has fresh identity"), Punch->PunchAction.GetSnapshot().AttackId != FirstId);
	TestTrue(TEXT("Next punch can hit again"), Punch->ResolvePunchHit(Hit(A)).bHitAccepted);

	ASC(B)->AddLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
	const auto ImmuneImpact = Punch->ResolvePunchHit(Hit(B));
	TestEqual(TEXT("Impact immunity still takes damage"), ImmuneImpact.ActualHPRemoved, 25.0f);
	TestEqual(TEXT("Impact immunity suppresses tier"), ImmuneImpact.ImpactSummary.EffectiveTier, EHALImpactTier::None);
	ASC(B)->RemoveLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
	ASC(B)->AddLooseGameplayTag(HALGameplayTags::State_Combat_DamageImmune);
	Punch->EnterState(ESpringPunchState::Extending);
	const auto ImmuneDamage = Punch->ResolvePunchHit(Hit(B));
	TestTrue(TEXT("Damage immunity still receives impact"), ImmuneDamage.bHitAccepted && ImmuneDamage.bImpactApplied);
	TestEqual(TEXT("Damage immune HP unchanged"), ImmuneDamage.ActualHPRemoved, 0.0f);
	TestFalse(TEXT("Immune hit is consumed"), Punch->ResolvePunchHit(Hit(B)).bHitAccepted);
	ASC(B)->RemoveLooseGameplayTag(HALGameplayTags::State_Combat_DamageImmune);
	Punch->BaseDamage = 0;
	Punch->EnterState(ESpringPunchState::Extending);
	ASC(A)->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(), 0);
	TestTrue(TEXT("HP zero and zero damage still allow impact"), Punch->ResolvePunchHit(Hit(A)).bImpactApplied);

	// A target may finish GAS initialization later during the same punch.
	auto* Late = F.Target();
	Late->SetActorLocation(FVector(2000, 0, 0));
	ASC(Late)->ClearActorInfo();
	TestEqual(TEXT("Unready target is rejected"), Punch->ResolvePunchHit(Hit(Late)).RejectReason, EHALHitRejectReason::TargetNotReady);
	ASC(Late)->InitAbilityActorInfo(Late, Late);
	TestTrue(TEXT("Rejected target can retry in the same punch"), Punch->ResolvePunchHit(Hit(Late)).bHitAccepted);
	// A separate fully configured target exercises the actual sweep and per-punch cache.
	Punch->BaseDamage = 25;
	Punch->EnterState(ESpringPunchState::Extending);
	Punch->PreviousHitBoxWorldLocation = FVector::ZeroVector;
	Punch->PunchPivot->SetRelativeLocation(FVector(400, 0, 0));
	const float Before = ASC(B)->GetSet<UHALHealthAttributeSet>()->GetCurrentHP();
	Punch->ScanPunchHits();
	TestEqual(TEXT("Swept target takes one hit"), ASC(B)->GetSet<UHALHealthAttributeSet>()->GetCurrentHP(), Before - 25);
	Punch->PreviousHitBoxWorldLocation = FVector::ZeroVector;
	Punch->ScanPunchHits();
	TestEqual(TEXT("Repeated sweep cannot double damage"), ASC(B)->GetSet<UHALHealthAttributeSet>()->GetCurrentHP(), Before - 25);
	Punch->EndPlay(EEndPlayReason::Destroyed);
	TestFalse(TEXT("EndPlay closes action"), Punch->PunchAction.IsActive());
	Punch->BaseDamage = -1;
	TestFalse(TEXT("Invalid damage rejected"), Punch->BeginPunchAttack());
	Punch->Phase.State = ESpringPunchState::Extending;
	Punch->Phase.StartedAt = F.World->GetTimeSeconds() - 1;
	Punch->Phase.Duration = 2;
	Punch->Phase.Distance = 400;
	Punch->Phase.RetractedLocation = FVector(10, 0, 0);
	Punch->OnRep_Phase();
	TestTrue(TEXT("Late phase snapshot reconstructs midpoint"), Punch->PunchPivot->GetRelativeLocation().Equals(FVector(210, 0, 0), 0.1));
	TestFalse(TEXT("Applying replicated phase never starts an attack"), Punch->PunchAction.IsActive());
	Punch->Phase.StartedAt -= 20;
	Punch->OnRep_Phase();
	TestTrue(TEXT("Expired phase clamps at endpoint"), Punch->PunchPivot->GetRelativeLocation().Equals(FVector(410, 0, 0), 0.1));
	Punch->Phase.State = ESpringPunchState::Cooldown;
	Punch->OnRep_Phase();
	TestTrue(TEXT("Rest phase restores authored origin"), Punch->PunchPivot->GetRelativeLocation().Equals(FVector(10, 0, 0), 0.1));
	TestFalse(TEXT("Rest phase disables tick"), Punch->IsActorTickEnabled());
	auto* Curve = NewObject<UCurveFloat>();
	Curve->FloatCurve.AddKey(0.f, 0.f);
	Curve->FloatCurve.AddKey(.5f, .9f);
	Curve->FloatCurve.AddKey(1.f, 1.f);
	Punch->ExtensionCurve = Curve;
	Punch->EnterState(ESpringPunchState::Extending);
	TestEqual(TEXT("Extension phase captures configured curve"), Punch->Phase.ExtensionCurve.Get(), Curve);
	Punch->ExtensionCurve = nullptr;
	Punch->Phase.StartedAt = F.World->GetTimeSeconds() - 1;
	Punch->Phase.Duration = 2;
	Punch->OnRep_Phase();
	TestTrue(TEXT("Late curve phase evaluates 90 percent extension at half time"), Punch->PunchPivot->GetRelativeLocation().Equals(FVector(370, 0, 0), .1));
	TestEqual(TEXT("Curve starts exactly retracted"), Punch->EvaluateExtensionProgress(0), 0.f);
	TestEqual(TEXT("Curve ends exactly extended"), Punch->EvaluateExtensionProgress(1), 1.f);
	Curve->FloatCurve.Reset();
	Curve->FloatCurve.AddKey(0.f, 2.f);
	Curve->FloatCurve.AddKey(1.f, 2.f);
	TestEqual(TEXT("Curve overshoot stays inside authored reach"), Punch->EvaluateExtensionProgress(.5f), 1.f);
	Curve->FloatCurve.Reset();
	TestEqual(TEXT("Empty curve falls back to linear"), Punch->EvaluateExtensionProgress(.5f), .5f);
	auto* ReturnCurve = NewObject<UCurveFloat>();
	ReturnCurve->FloatCurve.AddKey(0.f, 0.f);
	ReturnCurve->FloatCurve.AddKey(.5f, .25f);
	ReturnCurve->FloatCurve.AddKey(1.f, 1.f);
	Punch->RetractionCurve = ReturnCurve;
	Punch->EnterState(ESpringPunchState::Retracting);
	TestEqual(TEXT("Return phase captures its own curve"), Punch->Phase.RetractionCurve.Get(), ReturnCurve);
	TestNull(TEXT("Return phase does not reuse extension curve"), Punch->Phase.ExtensionCurve.Get());
	TestTrue(TEXT("Return curve starts fully extended"), Punch->PunchPivot->GetRelativeLocation().Equals(FVector(410, 0, 0), .1));
	Punch->RetractionCurve = nullptr;
	Punch->Phase.StartedAt = F.World->GetTimeSeconds() - 1;
	Punch->Phase.Duration = 2;
	Punch->OnRep_Phase();
	TestTrue(TEXT("Late return snapshot reconstructs 25 percent recovery at half time"), Punch->PunchPivot->GetRelativeLocation().Equals(FVector(310, 0, 0), .1));
	TestFalse(TEXT("Return curve never starts an attack"), Punch->PunchAction.IsActive());
	Punch->Phase.StartedAt -= 20;
	Punch->OnRep_Phase();
	TestTrue(TEXT("Return curve ends at exact origin"), Punch->PunchPivot->GetRelativeLocation().Equals(FVector(10, 0, 0), .1));
	Punch->EnterState(ESpringPunchState::Retracting);
	Punch->Phase.StartedAt = F.World->GetTimeSeconds() - 1;
	Punch->Phase.Duration = 2;
	Punch->OnRep_Phase();
	TestTrue(TEXT("Unassigned return curve preserves linear movement"), Punch->PunchPivot->GetRelativeLocation().Equals(FVector(210, 0, 0), .1));
	Punch->EndPlay(EEndPlayReason::Destroyed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStabilityContactTest, "HAL.FutureCreation.Stability.SupportVersusAttacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStabilityContactTest::RunTest(const FString& Parameters)
{
	FImpactWorld F(true);
	auto* Target = F.Target();
	auto* Shared = F.World->GetSubsystem<UVehicleStabilitySubsystem>();
	TestTrue(TEXT("Shared material applied to vehicle root"), Body(Target)->BodyInstance.GetSimplePhysicalMaterial() == Shared->GetVehicleMaterial());
	auto* Floor = F.World->SpawnActor<AActor>();
	auto* FloorBody = NewObject<UBoxComponent>(Floor);
	Floor->SetRootComponent(FloorBody); FloorBody->SetMobility(EComponentMobility::Static);
	FloorBody->SetCollisionObjectType(ECC_WorldStatic); FloorBody->RegisterComponent();
	auto Physical = [&](AActor* Source, UPrimitiveComponent* SourceBody, FVector Normal, EHALImpactMode Mode)
	{
		Body(Target)->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Body(Target)->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		FHALCombatAction Action; Begin(Action, Source, 0);
		auto Hit = Request(Action, Target, EHALImpactTier::Medium, false);
		Hit.Impact.Mode = Mode; Hit.Hit.SourceActor = Source; Hit.Hit.SourcePhysicsBody = SourceBody;
		Hit.Hit.NormalImpulse = Normal * Body(Target)->GetMass() * 150;
		Hit.Hit.ImpactNormal = Normal;
		const auto Result = FCombatResolver::ResolveCombatHit(Action, Hit);
		TestTrue(TEXT("Impact accepted independently of physical supplement"), Result.bHitAccepted && Result.bImpactApplied);
		return Result;
	};
	const auto Support = Physical(Floor, FloorBody, FVector::UpVector, EHALImpactMode::PhysicalContact);
	TestEqual(TEXT("Support retains Medium tier"), Support.ImpactSummary.EffectiveTier, EHALImpactTier::Medium);
	TestTrue(TEXT("Ordinary static support queues zero rule velocity"), Support.bSupportSupplementSuppressed && Support.RuleDeltaVelocity.IsNearlyZero());
	TestEqual(TEXT("Support does not damage"), ASC(Target)->GetSet<UHALHealthAttributeSet>()->GetCurrentHP(), 100.f);
	const auto WallResult = Physical(Floor, FloorBody, FVector::ForwardVector, EHALImpactMode::PhysicalContact);
	TestTrue(TEXT("Horizontal wall keeps bounded normal supplement"), !WallResult.bSupportSupplementSuppressed && WallResult.RuleDeltaVelocity.X > 0);
	FloorBody->SetMobility(EComponentMobility::Movable);
	const auto MovableResult = Physical(Floor, FloorBody, FVector::UpVector, EHALImpactMode::PhysicalContact);
	TestTrue(TEXT("Movable but stationary WorldStatic floor suppresses support supplement"), MovableResult.bSupportSupplementSuppressed && MovableResult.RuleDeltaVelocity.IsNearlyZero());
	FloorBody->ComponentVelocity = FVector(10,0,0);
	const auto MovingResult = Physical(Floor, FloorBody, FVector::UpVector, EHALImpactMode::PhysicalContact);
	TestTrue(TEXT("Reported component motion keeps original response"), !MovingResult.bSupportSupplementSuppressed
		&& MovingResult.SupportSourceReason == TEXT("MovingSource") && MovingResult.RuleDeltaVelocity.Z > 0);
	FloorBody->ComponentVelocity = FVector::ZeroVector;
	FloorBody->SetPhysicsLinearVelocity(FVector(10,0,0));
	TestTrue(TEXT("Kinematic fixture reports physics linear velocity"), FloorBody->GetPhysicsLinearVelocity().X > 9);
	const auto KinematicLinear = Physical(Floor, FloorBody, FVector::UpVector, EHALImpactMode::PhysicalContact);
	TestTrue(TEXT("Kinematic motion cannot hide behind zero ComponentVelocity"), !KinematicLinear.bSupportSupplementSuppressed
		&& KinematicLinear.SupportSourceReason == TEXT("MovingSource"));
	FloorBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
	FloorBody->SetPhysicsAngularVelocityInRadians(FVector(0,0,1));
	TestTrue(TEXT("Kinematic fixture reports angular velocity"), FloorBody->GetPhysicsAngularVelocityInRadians().Z > .9);
	const auto KinematicAngular = Physical(Floor, FloorBody, FVector::UpVector, EHALImpactMode::PhysicalContact);
	TestTrue(TEXT("Rotation-only source is not stationary even at its axis"), !KinematicAngular.bSupportSupplementSuppressed
		&& KinematicAngular.SupportSourceReason == TEXT("MovingSource"));
	FloorBody->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	FloorBody->SetCollisionObjectType(ECC_WorldDynamic);
	const auto DynamicResult = Physical(Floor, FloorBody, FVector::UpVector, EHALImpactMode::PhysicalContact);
	TestTrue(TEXT("Stationary WorldDynamic is not implicitly an ordinary floor"), !DynamicResult.bSupportSupplementSuppressed && DynamicResult.RuleDeltaVelocity.Z > 0);
	FloorBody->SetCollisionObjectType(ECC_WorldStatic);
	FloorBody->SetSimulatePhysics(true);
	const auto SimulatedResult = Physical(Floor, FloorBody, FVector::UpVector, EHALImpactMode::PhysicalContact);
	TestTrue(TEXT("Simulating WorldStatic remains an independent physics source"), !SimulatedResult.bSupportSupplementSuppressed && SimulatedResult.RuleDeltaVelocity.Z > 0);
	FloorBody->SetSimulatePhysics(false);
	FloorBody->SetMobility(EComponentMobility::Static);
	auto* Profile = NewObject<UHALImpactSourceComponent>(Floor); Floor->AddInstanceComponent(Profile); Profile->RegisterComponent();
	const auto AuthoredResult = Physical(Floor, FloorBody, FVector::UpVector, EHALImpactMode::PhysicalContact);
	TestTrue(TEXT("Authored physical source is not suppressed by slope"), !AuthoredResult.bSupportSupplementSuppressed && AuthoredResult.RuleDeltaVelocity.Z > 0);
	Profile->DestroyComponent();
	auto* Vehicle = F.Target();
	const auto VehicleResult = Physical(Vehicle, Body(Vehicle), FVector::UpVector, EHALImpactMode::PhysicalContact);
	TestTrue(TEXT("Vehicle contact remains active"), !VehicleResult.bSupportSupplementSuppressed && VehicleResult.RuleDeltaVelocity.Z > 0);
	auto* Ball = F.World->SpawnActor<ABasicBallActor>();
	TestTrue(TEXT("Ball retains its own physical material"), Ball->GetPhysicsRoot()->BodyInstance.GetSimplePhysicalMaterial() != Shared->GetVehicleMaterial());
	const auto BallResult = Physical(Ball, Ball->GetPhysicsRoot(), FVector::UpVector, EHALImpactMode::PhysicalContact);
	TestTrue(TEXT("Free ball contact remains active"), !BallResult.bSupportSupplementSuppressed && BallResult.RuleDeltaVelocity.Z > 0);
	const auto Attack = Physical(Floor, FloorBody, -FVector::ForwardVector, EHALImpactMode::ExplicitTier);
	TestTrue(TEXT("Explicit Medium still authors upward target"), !Attack.bSupportSupplementSuppressed && Attack.RuleMinimumUpwardSpeed >= 499);
	TestTrue(TEXT("Explicit Medium still authors flip"), Body(Target)->GetPhysicsAngularVelocityInRadians().Size() > 0);
	ASC(Target)->AddLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
	Body(Target)->SetPhysicsLinearVelocity(FVector(0,0,-90));
	FHALCombatAction Immune; Begin(Immune, Floor, 0);
	auto Hit = Request(Immune, Target, EHALImpactTier::Heavy, false);
	Hit.Impact.Mode = EHALImpactMode::PhysicalContact; Hit.Hit.SourceActor = Floor; Hit.Hit.SourcePhysicsBody = FloorBody;
	Hit.Hit.ImpactNormal = FVector::UpVector; Hit.Hit.NormalImpulse = FVector(0,0,800000);
	const auto ImmuneResult = FCombatResolver::ResolveCombatHit(Immune, Hit);
	TestEqual(TEXT("Impact immunity remains independent"), ImmuneResult.ImpactSummary.EffectiveTier, EHALImpactTier::None);
	TestEqual(TEXT("Immunity/filter never cancels falling"), Body(Target)->GetPhysicsLinearVelocity().Z, -90.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSavedSupportTest, "HAL.FutureCreation.Stability.SavedMapFloors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALSavedSupportTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Map : {TEXT("Test"), TEXT("MVPB_NetTest")})
	{
		const FString Path = FString::Printf(TEXT("/Game/Maps/%s.%s"), Map, Map);
		auto* SavedWorld = LoadObject<UWorld>(nullptr, *Path);
		if (!TestNotNull(Path+TEXT(" loads read-only"), SavedWorld)) { return false; }
		auto* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/MapTemplates/SM_Template_Map_Floor.SM_Template_Map_Floor"));
		UStaticMeshComponent* SavedFloor = nullptr;
		int32 Candidates = 0;
		for (AActor* Actor : SavedWorld->PersistentLevel->Actors)
		{
			if (!IsValid(Actor)) { continue; }
			TArray<UStaticMeshComponent*> Components; Actor->GetComponents(Components);
			for (auto* Component : Components)
			{
				if (Component->GetStaticMesh() == Mesh) { SavedFloor = Component; ++Candidates; }
			}
		}
		if (!TestEqual(Path+TEXT(" unambiguous template floor"), Candidates, 1)) { return false; }
		// Copy saved settings into a separate transient world; never change the loaded map or its actors.
		FImpactWorld F(true); auto* Target = F.Target(); auto* Floor = F.World->SpawnActor<AActor>();
		auto* FloorBody = NewObject<UStaticMeshComponent>(Floor); Floor->AddInstanceComponent(FloorBody); Floor->SetRootComponent(FloorBody);
		FloorBody->SetStaticMesh(SavedFloor->GetStaticMesh()); FloorBody->SetMobility(SavedFloor->Mobility);
		// The source may already own a live Editor physics body. Never copy its runtime handles.
		FloorBody->SetCollisionProfileName(SavedFloor->GetCollisionProfileName());
		FloorBody->SetCollisionEnabled(SavedFloor->GetCollisionEnabled());
		FloorBody->SetCollisionObjectType(SavedFloor->GetCollisionObjectType());
		FloorBody->SetCollisionResponseToChannels(SavedFloor->GetCollisionResponseToChannels());
		FloorBody->BodyInstance.bSimulatePhysics = SavedFloor->BodyInstance.bSimulatePhysics;
		FloorBody->SetWorldTransform(SavedFloor->GetComponentTransform()); FloorBody->RegisterComponent();
		FHALCombatAction Action; Begin(Action, Floor, 0);
		auto Hit = Request(Action, Target, EHALImpactTier::Medium, false);
		Hit.Impact.Mode = EHALImpactMode::PhysicalContact; Hit.Hit.SourceActor = Floor; Hit.Hit.SourcePhysicsBody = FloorBody;
		Hit.Hit.ImpactNormal = FVector::UpVector; Hit.Hit.ImpactPoint = FloorBody->GetComponentLocation();
		Hit.Hit.NormalImpulse = FVector::UpVector * Body(Target)->GetMass() * 150;
		const auto Result = FCombatResolver::ResolveCombatHit(Action, Hit);
		TestTrue(Path+TEXT(" real saved floor gets zero support command"), Result.bHitAccepted && Result.bImpactApplied
			&& Result.bSupportSupplementSuppressed && Result.RuleDeltaVelocity.IsNearlyZero());
		TestEqual(Path+TEXT(" Medium tier retained"), Result.ImpactSummary.EffectiveTier, EHALImpactTier::Medium);
		AddInfo(FString::Printf(TEXT("Saved support: Map=%s Mobility=%d ObjectType=%d Simulated=%d Reason=%s Suppressed=%d RuleDV=%s"),
			Map, int32(FloorBody->Mobility), int32(FloorBody->GetCollisionObjectType()), FloorBody->IsSimulatingPhysics(),
			*Result.SupportSourceReason.ToString(), Result.bSupportSupplementSuppressed, *Result.RuleDeltaVelocity.ToString()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStabilitySnapshotTest, "HAL.FutureCreation.Stability.ValidationAndWorldSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStabilitySnapshotTest::RunTest(const FString& Parameters)
{
	float LegacyMass = 0;
	FVector LegacyInertia = FVector::ZeroVector;
	{
		FImpactWorld F; auto* Target = F.Target();
		LegacyMass = Body(Target)->GetMass(); LegacyInertia = Body(Target)->GetInertiaTensor();
	}
	{
		FImpactWorld F(true); auto* Target = F.Target();
		TestEqual(TEXT("Default-density shared material preserves configured mass"), Body(Target)->GetMass(), LegacyMass, .01f);
		TestTrue(TEXT("Same geometry and mass preserve inertia after material update"), Body(Target)->GetInertiaTensor().Equals(LegacyInertia, .1));
	}
	FScopedHALStabilityTestRules Settings(true);
	auto* Rules = Settings.Settings->SharedRules.Get();
	FString Error;
	TestTrue(TEXT("Test material valid"), Rules->Validate(Error));
	Rules->MaximumSupportSlopeDegrees = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Nonfinite slope rejected"), Rules->Validate(Error));
	Rules->MaximumSupportSlopeDegrees = 81;
	TestFalse(TEXT("Excess slope rejected"), Rules->Validate(Error));
	Rules->MaximumSupportSlopeDegrees = 45;
	Rules->VehiclePhysicalMaterial->Restitution = -1;
	TestFalse(TEXT("Invalid material rejected"), Rules->Validate(Error));
	Rules->VehiclePhysicalMaterial->Restitution = 0;
	uint32 First = 0;
	{
		FImpactWorld F(true);
		auto* Original = F.Stability.Settings->SharedRules.Get();
		auto* A = F.Target();
		auto* Shared = F.World->GetSubsystem<UVehicleStabilitySubsystem>(); First = Shared->GetSignature();
		Original->MaximumSupportSlopeDegrees = 60; Original->VehiclePhysicalMaterial->Restitution = .1f;
		auto* B = F.Target();
		TestEqual(TEXT("Midmatch source edit cannot alter frozen restitution"), Shared->GetVehicleMaterial()->Restitution, 0.f);
		TestEqual(TEXT("One snapshot for every vehicle"), Body(A)->BodyInstance.GetSimplePhysicalMaterial(), Body(B)->BodyInstance.GetSimplePhysicalMaterial());
		TestEqual(TEXT("Snapshot signature frozen"), Shared->GetSignature(), First);
	}
	{
		FImpactWorld F(true);
		F.Stability.Settings->SharedRules.Get()->MaximumSupportSlopeDegrees = 60;
		F.Target();
		TestNotEqual(TEXT("New world consumes edited values"), F.World->GetSubsystem<UVehicleStabilitySubsystem>()->GetSignature(), First);
		TestEqual(TEXT("New world applies new slope"), F.World->GetSubsystem<UVehicleStabilitySubsystem>()->GetContactRules().MinimumSupportUpDot, .5f, .0001f);
	}
	{
		FImpactWorld F(true); F.Stability.Settings->SharedRules.Get()->bReadyForUse = false;
		AddExpectedMessage(TEXT("Invalid VehicleStabilityRules"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
		auto* Invalid = F.Target();
		TestFalse(TEXT("Invalid selected DA cannot silently fall back to old physics"), Body(Invalid)->IsSimulatingPhysics());
	}
	{
		FImpactWorld F(true); auto* Target = F.Target(); auto* Receiver = Target->FindComponentByClass<UHALImpactReceiverComponent>();
		TestTrue(TEXT("Authority signature initialized"), Receiver->HasMatchingStabilityRules());
		Target->SetRole(ROLE_SimulatedProxy);
		FindFProperty<FUInt32Property>(Receiver->GetClass(), TEXT("StabilitySignature"))->SetPropertyValue_InContainer(Receiver, 1);
		AddExpectedMessage(TEXT("VehicleStabilityRules mismatch"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
		Receiver->ProcessEvent(Receiver->FindFunctionChecked(TEXT("OnRep_StabilitySignature")), nullptr);
		TestFalse(TEXT("Mismatch stops client simulation"), Receiver->HasMatchingStabilityRules() || Body(Target)->IsSimulatingPhysics());
		Target->SetRole(ROLE_Authority);
	}
	FVehicleContactStabilityRules Policy; Policy.bSuppressSupportSupplement = true; Policy.MinimumSupportUpDot = FMath::Cos(PI/4);
	TestTrue(TEXT("Shallow support accepted"), Policy.SuppressesSupplement(true, FVector(0,.5,1), FVector(0,.5,1)));
	TestFalse(TEXT("Steep wall excluded"), Policy.SuppressesSupplement(true, FVector(1,0,.1), FVector::UpVector));
	TestFalse(TEXT("Ceiling excluded"), Policy.SuppressesSupplement(true, -FVector::UpVector, -FVector::UpVector));
	TestFalse(TEXT("Impulse alone cannot turn wall into support"), Policy.SuppressesSupplement(true, FVector::ForwardVector, FVector::UpVector));
	TestFalse(TEXT("Invalid direction excluded"), Policy.SuppressesSupplement(true, FVector::UpVector, FVector(std::numeric_limits<double>::infinity())));
	return true;
}

#endif
