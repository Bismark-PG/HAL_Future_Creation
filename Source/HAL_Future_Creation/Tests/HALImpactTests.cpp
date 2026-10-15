#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "../HALImpactTypes.h"
#include "../HALImpactAttributeSet.h"
#include "../HALImpactReceiverComponent.h"
#include "../HALImpactSourceComponent.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALGameplayTags.h"
#include "../HALHealthAttributeSet.h"
#include "../CombatResolver.h"
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
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace
{
	struct FImpactWorld
	{
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
		FImpactWorld()
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
	FImpactWorld F;
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
	const auto Drop = FCombatResolver::ResolveCombatHit(Heavy, Request(Heavy, Pawn, EHALImpactTier::Heavy, false));
	TestTrue(TEXT("Effective Medium drops exactly once"), Drop.ImpactSummary.bBallDropped && Control->GetHeldBall() == nullptr);
	TestEqual(TEXT("Dropped ball returns Free"), Held->GetBallState(), EBasicBallState::Free);
	TestFalse(TEXT("Drop preserves existing reacquire lock"), Held->CanBeControlledBy(Pawn));
	return true;
}

#endif
