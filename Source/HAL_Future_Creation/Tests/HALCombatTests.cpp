#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../CombatResolver.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALCombatAttributeSet.h"
#include "../HALCombatEffectContext.h"
#include "../HALHealthAttributeSet.h"
#include "../HALGameplayTags.h"
#include "../PassiveTestVehicle.h"
#include "../BasicBallActor.h"
#include "../VehicleDefinition.h"
#include "../VehicleHealthComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include <limits>

namespace
{
	struct FHALCombatTestWorld
	{
		UWorld* World;
		FHALCombatTestWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
				.ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
				true, ERHIFeatureLevel::Num, &Values);
			if (GEngine && World) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
		}
		~FHALCombatTestWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) { GEngine->DestroyWorldContext(World); }
			}
		}
		APassiveTestVehicle* Target(bool bMigrated = false)
		{
			APassiveTestVehicle* Actor = World->SpawnActor<APassiveTestVehicle>();
			UVehicleDefinition* Definition = NewObject<UVehicleDefinition>();
			Definition->bReadyForUse = true;
			Definition->CollisionShape = EVehicleCollisionShape::Box;
			Definition->Visual.Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			Definition->Combat.Version = bMigrated ? EHALCombatConfigVersion::AttackSnapshotV1 : EHALCombatConfigVersion::LegacyFixedDamage;
			const FEnumProperty* Source = FindFProperty<FEnumProperty>(Actor->GetClass(), TEXT("ConfigurationSource"));
			Source->GetUnderlyingProperty()->SetIntPropertyValue(Source->ContainerPtrToValuePtr<void>(Actor),
				uint64(EVehicleConfigurationSource::Definition));
			FindFProperty<FObjectProperty>(Actor->GetClass(), TEXT("Definition"))->SetObjectPropertyValue_InContainer(Actor, Definition);
			Actor->PreInitializeComponents();
			Actor->DispatchBeginPlay();
			return Actor;
		}
	};
	UHALAbilitySystemComponent* GetASC(APassiveTestVehicle* Target)
	{
		return CastChecked<UHALAbilitySystemComponent>(Target->GetAbilitySystemComponent());
	}
	FHALCombatHitRequest HitRequest(const FHALCombatAction& Action, AActor* Target, bool bImpact = false)
	{
		FHALCombatHitRequest Request;
		Request.AttackId = Action.GetSnapshot().AttackId;
		Request.PulseSequence = Action.GetPulseSequence();
		Request.Hit.TargetActor = Target;
		Request.bApplyImpact = bImpact;
		Request.Hit.TargetPhysicsBody = Cast<UPrimitiveComponent>(Target->GetRootComponent());
		Request.Hit.ImpactNormal = FVector(-1, 0, 0);
		Request.Hit.SourceVelocity = FVector(2000, 0, 0);
		Request.Hit.SourceMass = 35;
		return Request;
	}
	FHALAttackSourceSnapshot FixedSource(AActor* Actor, float Damage = 25)
	{
		FHALAttackSourceSnapshot Source;
		Source.SourceActor = Actor;
		Source.FixedDamage = Damage;
		return Source;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCombatSnapshotTest, "HAL.FutureCreation.Combat.SnapshotAndSourceLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCombatSnapshotTest::RunTest(const FString& Parameters)
{
	FHALCombatTestWorld Fixture;
	APassiveTestVehicle* Attacker = Fixture.Target(true);
	APassiveTestVehicle* Target = Fixture.Target(true);
	AActor* SourceActor = Fixture.World->SpawnActor<AActor>();
	UHALAbilitySystemComponent* AttackerASC = GetASC(Attacker);
	// A real removable GAS modifier expires after capture. The flight must retain its old values.
	UGameplayEffect* Buff = NewObject<UGameplayEffect>();
	Buff->DurationPolicy = EGameplayEffectDurationType::Infinite;
	FGameplayModifierInfo& Modifier = Buff->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UHALCombatAttributeSet::GetOutputMultiplierDeltaAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FScalableFloat(0.25f);
	const FActiveGameplayEffectHandle Handle = AttackerASC->ApplyGameplayEffectToSelf(Buff, 1, AttackerASC->MakeEffectContext());
	FHALAttackSourceSnapshot Source;
	TestTrue(TEXT("Migrated authority captures effective attack"), FCombatResolver::CaptureVehicleAttack(
		*SourceActor, *Attacker, 1, -0.05f, Source));
	Source.InstigatorPlayerId = 42;
	Source.SourceTag = HALGameplayTags::Combat_Source_Ball;
	FHALCombatAction Action;
	TestTrue(TEXT("Action opens"), Action.Begin(*SourceActor, Source));
	AttackerASC->RemoveActiveGameplayEffect(Handle);
	TestEqual(TEXT("Buff really removed from live source"), AttackerASC->GetSet<UHALCombatAttributeSet>()->GetOutputMultiplierDelta(), 0.0f);
	GetASC(Target)->SetNumericAttributeBase(UHALCombatAttributeSet::GetDamageTakenMultiplierAttribute(), 0.5f);
	int32 ContextPlayerId = INDEX_NONE;
	FGuid ContextAttackId;
	GetASC(Target)->OnGameplayEffectAppliedDelegateToSelf.AddLambda(
		[&](UAbilitySystemComponent*, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle)
		{
			const FGameplayEffectContext* Context = Spec.GetContext().Get();
			if (Context && Context->GetScriptStruct() == FHALCombatEffectContext::StaticStruct())
			{
				const auto* Stable = static_cast<const FHALCombatEffectContext*>(Context);
				ContextPlayerId = Stable->InstigatorPlayerId;
				ContextAttackId = Stable->AttackId;
			}
		});
	TestTrue(TEXT("Source Actor actually destroyed"), SourceActor->Destroy());
	TestTrue(TEXT("Source Avatar and ASC actually destroyed"), Attacker->Destroy());
	TestFalse(TEXT("Snapshot source weak reference is now invalid"), Action.GetSnapshot().SourceActor.IsValid());
	TestFalse(TEXT("Snapshot instigator weak reference is now invalid"), Action.GetSnapshot().InstigatorActor.IsValid());
	const FHALCombatHitResult Result = FCombatResolver::ResolveCombatHit(Action, HitRequest(Action, Target));
	TestTrue(TEXT("Source and source ASC destruction do not cancel captured attack"), Result.bHitAccepted);
	TestTrue(TEXT("Flight keeps captured modifier sum"), FMath::IsNearlyEqual(Result.RawDamage, 30.0f));
	TestTrue(TEXT("Target defense is read at impact"), FMath::IsNearlyEqual(Result.ResolvedDamage, 15.0f));
	TestTrue(TEXT("Actual damage reaches GAS"), FMath::IsNearlyEqual(Result.ActualHPRemoved, 15.0f));
	TestEqual(TEXT("Effect context keeps departed player's stable attribution"), ContextPlayerId, 42);
	TestEqual(TEXT("Effect context uses the action ID"), ContextAttackId, Action.GetSnapshot().AttackId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCombatDedupTest, "HAL.FutureCreation.Combat.PulsesTargetsAndDedup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCombatDedupTest::RunTest(const FString& Parameters)
{
	FHALCombatTestWorld Fixture;
	AActor* Source = Fixture.World->SpawnActor<AActor>();
	APassiveTestVehicle* First = Fixture.Target();
	APassiveTestVehicle* Second = Fixture.Target();
	FHALCombatAction Action;
	TestTrue(TEXT("Fixed source without ASC is legal"), Action.Begin(*Source, FixedSource(Source)));
	const FHALCombatHitRequest FirstRequest = HitRequest(Action, First);
	TestTrue(TEXT("First target accepted"), FCombatResolver::ResolveCombatHit(Action, FirstRequest).bHitAccepted);
	TestTrue(TEXT("Same pulse can hit another target"), FCombatResolver::ResolveCombatHit(Action, HitRequest(Action, Second)).bHitAccepted);
	TestEqual(TEXT("Duplicate callback rejected"), FCombatResolver::ResolveCombatHit(Action, FirstRequest).RejectReason, EHALHitRejectReason::Duplicate);
	TestEqual(TEXT("Duplicate cannot damage again"), First->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(), 75.0f);
	TestFalse(TEXT("Same pulse cannot reopen"), Action.AdvancePulse(1));
	TestTrue(TEXT("A later attack segment opens"), Action.AdvancePulse(2));
	TestEqual(TEXT("Old pulse never reopens after cache clear"), FCombatResolver::ResolveCombatHit(Action, FirstRequest).RejectReason, EHALHitRejectReason::InvalidAction);
	TestTrue(TEXT("New pulse may hit same target"), FCombatResolver::ResolveCombatHit(Action, HitRequest(Action, First)).bHitAccepted);
	TestEqual(TEXT("New segment damage"), First->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(), 50.0f);
	const FHALCombatHitRequest ClosedRequest = HitRequest(Action, First);
	Action.Close();
	TestEqual(TEXT("Closed events cannot be replayed"), FCombatResolver::ResolveCombatHit(Action, ClosedRequest).RejectReason, EHALHitRejectReason::ClosedAction);
	TestTrue(TEXT("New lifetime gets a fresh ID"), Action.Begin(*Source, FixedSource(Source)));
	TestEqual(TEXT("Previous lifetime cannot hit new action"), FCombatResolver::ResolveCombatHit(Action, ClosedRequest).RejectReason, EHALHitRejectReason::InvalidAction);
	FHALCombatTestWorld OtherWorld;
	TestEqual(TEXT("Foreign world target is rejected"), FCombatResolver::ResolveCombatHit(Action,
		HitRequest(Action, OtherWorld.Target())).RejectReason, EHALHitRejectReason::InvalidTarget);
	Second->SetRole(ROLE_SimulatedProxy);
	TestEqual(TEXT("Client proxy cannot receive authority damage"), FCombatResolver::ResolveCombatHit(Action,
		HitRequest(Action, Second)).RejectReason, EHALHitRejectReason::NonAuthority);
	Second->SetRole(ROLE_Authority);
	FHALAttackSourceSnapshot SelfPolicy = FixedSource(Source);
	SelfPolicy.InstigatorActor = First;
	SelfPolicy.bCanHitInstigator = false;
	FHALCombatAction Excluded;
	Excluded.Begin(*Source, SelfPolicy);
	TestEqual(TEXT("Ball-style self exclusion belongs to action policy"), FCombatResolver::ResolveCombatHit(Excluded,
		HitRequest(Excluded, First)).RejectReason, EHALHitRejectReason::ExcludedInstigator);
	SelfPolicy.bCanHitInstigator = true;
	FHALCombatAction SelfDamage;
	SelfDamage.Begin(*Source, SelfPolicy);
	TestTrue(TEXT("Explicit self-damage action can use common entry"), FCombatResolver::ResolveCombatHit(SelfDamage,
		HitRequest(SelfDamage, First)).bHitAccepted);
	FHALCombatAction Capacity;
	Capacity.Begin(*Source, FixedSource(Source));
	for (int32 Index = 0; Index < 256; ++Index)
	{
		FHALCombatHitRequest Request = HitRequest(Capacity, Fixture.World->SpawnActor<AActor>());
		Request.bApplyDamage = false;
		if (!TestTrue(TEXT("Bounded pulse accepts distinct targets"), FCombatResolver::ResolveCombatHit(Capacity, Request).bHitAccepted)) { return false; }
	}
	FHALCombatHitRequest Overflow = HitRequest(Capacity, Fixture.World->SpawnActor<AActor>());
	Overflow.bApplyDamage = false;
	TestEqual(TEXT("Capacity does not evict records and permit old repeats"), FCombatResolver::ResolveCombatHit(Capacity, Overflow).RejectReason, EHALHitRejectReason::CapacityExceeded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCombatIndependentResultsTest, "HAL.FutureCreation.Combat.DamageImpactIndependence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCombatIndependentResultsTest::RunTest(const FString& Parameters)
{
	FHALCombatTestWorld Fixture;
	AActor* Source = Fixture.World->SpawnActor<AActor>();
	APassiveTestVehicle* Target = Fixture.Target();
	UHALAbilitySystemComponent* ASC = GetASC(Target);
	GetMutableDefault<UVehicleKnockbackSettings>()->InitializeRules();
	FHALCombatAction HalfDamage;
	HalfDamage.Begin(*Source, FixedSource(Source));
	ASC->SetNumericAttributeBase(UHALCombatAttributeSet::GetDamageTakenMultiplierAttribute(), 0.5f);
	const FHALCombatHitResult Half = FCombatResolver::ResolveCombatHit(HalfDamage, HitRequest(HalfDamage, Target, true));
	TestEqual(TEXT("Half resistance changes damage only"), Half.ActualHPRemoved, 12.5f);
	TestTrue(TEXT("Half damage retains impact"), Half.bImpactApplied);
	TestEqual(TEXT("Existing physical measurement retained"), Half.ImpactSpeed, 2000.0f);
	ASC->AddLooseGameplayTag(HALGameplayTags::State_Combat_DamageImmune);
	FHALCombatAction Immune;
	Immune.Begin(*Source, FixedSource(Source));
	const FHALCombatHitResult NoDamage = FCombatResolver::ResolveCombatHit(Immune, HitRequest(Immune, Target, true));
	TestTrue(TEXT("Immune hit is accepted"), NoDamage.bHitAccepted);
	TestTrue(TEXT("Immunity stops only damage"), NoDamage.bDamageImmune && NoDamage.ResolvedDamage == 0 && NoDamage.ActualHPRemoved == 0);
	TestTrue(TEXT("Damage immunity retains authored impact"), NoDamage.bImpactApplied);
	TestEqual(TEXT("Damage immunity cannot reduce physical tier"), NoDamage.KnockbackTier, Half.KnockbackTier);
	ASC->RemoveLooseGameplayTag(HALGameplayTags::State_Combat_DamageImmune);
	ASC->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(), 0);
	FHALCombatAction ZeroHP;
	ZeroHP.Begin(*Source, FixedSource(Source));
	const FHALCombatHitResult Dead = FCombatResolver::ResolveCombatHit(ZeroHP, HitRequest(ZeroHP, Target, true));
	TestTrue(TEXT("Zero HP is not elimination or hit rejection"), Dead.bHitAccepted && Dead.bImpactApplied);
	TestEqual(TEXT("Zero HP cannot go negative"), Dead.ActualHPRemoved, 0.0f);
	FHALCombatAction ZeroDamage;
	ZeroDamage.Begin(*Source, FixedSource(Source, 0));
	TestTrue(TEXT("Zero damage still produces valid impact"), FCombatResolver::ResolveCombatHit(ZeroDamage, HitRequest(ZeroDamage, Target, true)).bImpactApplied);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCombatPureDamageTest, "HAL.FutureCreation.Combat.PureDamageAndActualRemoval",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCombatPureDamageTest::RunTest(const FString& Parameters)
{
	FHALCombatTestWorld Fixture;
	AActor* Source = Fixture.World->SpawnActor<AActor>();
	APassiveTestVehicle* Target = Fixture.Target();
	UHALAbilitySystemComponent* ASC = GetASC(Target);
	UPrimitiveComponent* Body = CastChecked<UPrimitiveComponent>(Target->GetRootComponent());
	Body->SetSimulatePhysics(false);
	ASC->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(), 10);
	FHALCombatAction Action;
	Action.Begin(*Source, FixedSource(Source));
	FHALCombatHitRequest Request = HitRequest(Action, Target, true);
	Request.Hit.ImpactNormal = FVector(std::numeric_limits<float>::quiet_NaN(), 0, 0);
	const FHALCombatHitResult Result = FCombatResolver::ResolveCombatHit(Action, Request);
	TestTrue(TEXT("Pure damage needs no physics body or valid normal"), Result.bHitAccepted);
	TestEqual(TEXT("Missing physics is isolated to impact branch"), Result.ImpactOutcome, EHALImpactOutcome::NoPhysicsBody);
	TestEqual(TEXT("Resolved damage preserves requested result"), Result.ResolvedDamage, 25.0f);
	TestEqual(TEXT("Actual removal is bounded by remaining HP"), Result.ActualHPRemoved, 10.0f);
	Body->SetSimulatePhysics(true);
	ASC->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(), 100);
	FHALCombatAction BadImpact;
	BadImpact.Begin(*Source, FixedSource(Source));
	Request = HitRequest(BadImpact, Target, true);
	Request.Hit.SourceVelocity.X = std::numeric_limits<float>::infinity();
	const FHALCombatHitResult InvalidImpact = FCombatResolver::ResolveCombatHit(BadImpact, Request);
	TestTrue(TEXT("Invalid impact data cannot reject legal damage"), InvalidImpact.bHitAccepted);
	TestEqual(TEXT("Damage remains 25 despite invalid impact"), InvalidImpact.ActualHPRemoved, 25.0f);
	TestEqual(TEXT("Invalid physical input gives explicit outcome"), InvalidImpact.ImpactOutcome, EHALImpactOutcome::InvalidMeasurement);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCombatBallConsumptionTest, "HAL.FutureCreation.Combat.BallMigrationAndConsumption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCombatBallConsumptionTest::RunTest(const FString& Parameters)
{
	FHALCombatTestWorld Fixture;
	APassiveTestVehicle* Launcher = Fixture.Target(true);
	APassiveTestVehicle* Target = Fixture.Target(true);
	ABasicBallActor* LegacyBall = Fixture.World->SpawnActor<ABasicBallActor>();
	LegacyBall->DispatchBeginPlay();
	TestTrue(TEXT("Legacy ball acquires"), LegacyBall->BeginControl(Launcher));
	TestTrue(TEXT("Legacy ball launches unchanged"), LegacyBall->LaunchFromControl(Launcher, Launcher, FVector(2200, 0, 0)));
	const FHALAttackSourceSnapshot LegacySnapshot = LegacyBall->GetBallRepState().AttackSnapshot;
	float LegacyDamage;
	TestTrue(TEXT("Legacy saved fixed damage is captured"), LegacySnapshot.CalculateRawDamage(LegacyDamage));
	TestEqual(TEXT("Legacy still deals 25"), LegacyDamage, 25.0f);
	TestEqual(TEXT("New C++ fields did not migrate old ball implicitly"), LegacySnapshot.DamageMode, EHALDamageMode::SourceFixedDamage);
	ABasicBallActor* Ball = Fixture.World->SpawnActor<ABasicBallActor>();
	Ball->DispatchBeginPlay();
	Ball->RuntimeDamageConfig.Version = EHALCombatConfigVersion::AttackSnapshotV1;
	TestTrue(TEXT("Migrated ball acquires"), Ball->BeginControl(Launcher));
	TestTrue(TEXT("Migrated ball captures vehicle attack"), Ball->LaunchFromControl(Launcher, Launcher, FVector(2200, 0, 0)));
	TestEqual(TEXT("Atomic snapshot carries scaled mode"), Ball->GetBallRepState().AttackSnapshot.DamageMode, EHALDamageMode::VehicleAttackScaled);
	GetASC(Target)->AddLooseGameplayTag(HALGameplayTags::State_Combat_DamageImmune);
	FHitResult Hit;
	Hit.ImpactNormal = FVector(-1, 0, 0);
	UPrimitiveComponent* TargetBody = CastChecked<UPrimitiveComponent>(Target->GetRootComponent());
	Ball->OnPhysicsRootHit(Ball->GetPhysicsRoot(), Target, TargetBody, FVector::ZeroVector, Hit);
	TestEqual(TEXT("Damage immune target still consumes this ball attack"), Ball->GetBallState(), EBasicBallState::Free);
	TestEqual(TEXT("Immunity preserves target HP"), Target->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(), 100.0f);
	TestFalse(TEXT("Consumed action closes"), Ball->LaunchAction.IsActive());
	const uint32 Sequence = Ball->GetBallStateSequence();
	Ball->OnPhysicsRootHit(Ball->GetPhysicsRoot(), Target, TargetBody, FVector::ZeroVector, Hit);
	TestEqual(TEXT("Repeated collision cannot consume again"), Ball->GetBallStateSequence(), Sequence);
	APassiveTestVehicle* UnmigratedLauncher = Fixture.Target(false);
	ABasicBallActor* Miswired = Fixture.World->SpawnActor<ABasicBallActor>();
	Miswired->RuntimeDamageConfig.Version = EHALCombatConfigVersion::AttackSnapshotV1;
	Miswired->BeginControl(UnmigratedLauncher);
	AddExpectedMessage(TEXT("scaled launch rejected; migrate the vehicle combat config before the ball"),
		ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	TestFalse(TEXT("Migrated ball refuses unmigrated attacker instead of inventing attack values"),
		Miswired->LaunchFromControl(UnmigratedLauncher, UnmigratedLauncher, FVector(2200, 0, 0)));
	TestEqual(TEXT("Failed launch keeps original controlled state"), Miswired->GetBallState(), EBasicBallState::Controlled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCombatConfigurationTest, "HAL.FutureCreation.Combat.ConfigurationAndContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCombatConfigurationTest::RunTest(const FString& Parameters)
{
	FHALCombatTestWorld Fixture;
	APassiveTestVehicle* Target = Fixture.Target(true);
	UHALAbilitySystemComponent* ASC = GetASC(Target);
	ASC->SetNumericAttributeBase(UHALCombatAttributeSet::GetAttackPowerAttribute(), 40);
	FVehicleCombatConfig Config;
	Config.Version = EHALCombatConfigVersion::AttackSnapshotV1;
	TestTrue(TEXT("Repeat binding accepts initialized owner"), ASC->InitializeCombat(Config));
	TestEqual(TEXT("Repeat binding cannot erase active attack changes"), ASC->GetSet<UHALCombatAttributeSet>()->GetAttackPower(), 40.0f);
	FString Error;
	Config.OutputMultiplierDelta = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN config rejected"), Config.Validate(Error));
	FHALAttackSourceSnapshot Source;
	Source.DamageMode = EHALDamageMode::VehicleAttackScaled;
	Source.CapturedAttackPower = 25;
	Source.MultiplierDeltaSum = -2;
	float Damage;
	TestTrue(TEXT("Negative aggregate multiplier is valid"), Source.CalculateRawDamage(Damage));
	TestEqual(TEXT("Aggregate clamps at zero, never heals"), Damage, 0.0f);
	Source.CapturedAttackPower = MAX_flt;
	Source.MultiplierDeltaSum = 2;
	TestFalse(TEXT("Overflow cannot poison HP"), Source.CalculateRawDamage(Damage));
	FHALCombatEffectContext Context;
	Context.AttackId = FGuid::NewGuid();
	Context.PulseSequence = 3;
	Context.InstigatorPlayerId = 42;
	Context.SourceTag = HALGameplayTags::Combat_Source_Ball;
	Context.EffectTags.AddTag(HALGameplayTags::State_Combat_DamageImmune);
	TArray<uint8> Bytes;
	bool bSuccess;
	{
		FMemoryWriter Writer(Bytes);
		FObjectAndNameAsStringProxyArchive Names(Writer, false);
		Context.NetSerialize(Names, nullptr, bSuccess);
	}
	TestTrue(TEXT("Stable GE context writes"), bSuccess);
	FHALCombatEffectContext Read;
	{
		FMemoryReader Reader(Bytes);
		FObjectAndNameAsStringProxyArchive Names(Reader, false);
		Read.NetSerialize(Names, nullptr, bSuccess);
	}
	TestTrue(TEXT("Stable GE context reads"), bSuccess);
	TestEqual(TEXT("Context preserves attack"), Read.AttackId, Context.AttackId);
	TestEqual(TEXT("Context preserves pulse"), Read.PulseSequence, 3u);
	TestEqual(TEXT("Context preserves player attribution"), Read.InstigatorPlayerId, 42);
	TestTrue(TEXT("Context preserves payload tags"), Read.EffectTags == Context.EffectTags);
	FHitResult Hit;
	Hit.ImpactPoint = FVector(1, 2, 3);
	Context.AddHitResult(Hit);
	TUniquePtr<FGameplayEffectContext> Copy(Context.Duplicate());
	TestTrue(TEXT("Context duplication deep-copies hit data"), Copy->GetHitResult() != Context.GetHitResult());
	TestTrue(TEXT("Context type survives duplication"), Copy->GetScriptStruct() == FHALCombatEffectContext::StaticStruct());
	return true;
}

#endif
