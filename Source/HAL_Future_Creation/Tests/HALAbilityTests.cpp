#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALImpactImmunityEffects.h"
#include "../HALImpactAttributeSet.h"
#include "../HALHealthAttributeSet.h"
#include "../HALGameplayTags.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerState.h"
#include "../HALPlayerController.h"
#include "../TestVehiclePawn.h"
#include "../VehicleNetPhysicsData.h"
#include "../VehicleDefinition.h"
#include "../VehicleKnockbackSettings.h"
#include "../CombatResolver.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace
{
	struct FAbilityWorld
	{
		UWorld* World;
		AHALMatchGameState* Match;
		AHALPlayerState* State;
		AHALPlayerController* Controller;
		ATestVehiclePawn* Vehicle;
		UHALAbilitySystemComponent* ASC;
		uint64 LastAdvanceFrame = MAX_uint64;
		FAbilityWorld()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
				.ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Match = World->SpawnActor<AHALMatchGameState>();
			World->SetGameState(Match);
			Controller = World->SpawnActor<AHALPlayerController>();
			Controller->SetAsLocalPlayerController();
			State = World->SpawnActor<AHALPlayerState>();
			State->SetOwner(Controller);
			Controller->SetPlayerState(State);
			State->PreInitializeComponents();
			State->InitializeComponents();
			State->PostInitializeComponents(); // RouteEndPlay requires the normal actor initialization flag.
			State->DispatchBeginPlay();
			ASC = State->GetHALAbilitySystemComponent();
			Vehicle = SpawnVehicle();
			if (Vehicle) { Controller->Possess(Vehicle); Vehicle->DispatchBeginPlay(); }
		}
		ATestVehiclePawn* SpawnVehicle()
		{
			// Exercise the current saved tuning without changing any asset or global rule selection.
			UClass* Class = LoadClass<ATestVehiclePawn>(nullptr, TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
			if (!Class) { return nullptr; }
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Pawn = World->SpawnActor<ATestVehiclePawn>(Class, FVector(0, 0, 1000), FRotator::ZeroRotator, Params);
			Pawn->PreInitializeComponents();
			Pawn->InitializeComponents();
			Pawn->PostInitializeComponents();
			return Pawn;
		}
		~FAbilityWorld()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
		bool Activate() { return ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary); }
		bool Immune() const { return ASC->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune); }
		float Cooldown() const { return ASC->GetEffectTimeRemaining(HALGameplayTags::Cooldown_ImpactImmunity); }
	};

	/** Tick real World/GAS timers on successive engine frames; never fake GFrameCounter or remove on a test timer. */
	class FAdvanceAbilityWorld : public IAutomationLatentCommand
	{
		TSharedRef<FAbilityWorld> Fixture;
		FAutomationTestBase* Test;
		float Seconds;
		TOptional<double> Target;
		double Deadline = 0;
		TFunction<void()> Check;
	public:
		FAdvanceAbilityWorld(FAutomationTestBase* InTest, TSharedRef<FAbilityWorld> InFixture, float InSeconds, TFunction<void()> InCheck)
			: Fixture(InFixture), Test(InTest), Seconds(InSeconds), Check(MoveTemp(InCheck)) {}
		virtual bool Update() override
		{
			if (!Target.IsSet()) { Target = Fixture->World->GetTimeSeconds() + Seconds; Deadline = FPlatformTime::Seconds() + 30; }
			if (FPlatformTime::Seconds() > Deadline) { Test->AddError(TEXT("Ability World/GAS timers did not advance within 30 seconds.")); return true; }
			if (Fixture->World->GetTimeSeconds() + 0.001 >= Target.GetValue()) { Check(); return true; }
			if (Fixture->LastAdvanceFrame == GFrameCounter) { return false; }
			Fixture->LastAdvanceFrame = GFrameCounter;
			Fixture->World->Tick(LEVELTICK_All, 0.05f);
			return false;
		}
	};
	void After(FAutomationTestBase* Test, TSharedRef<FAbilityWorld> Fixture, float Seconds, TFunction<void()> Check)
	{
		FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FAdvanceAbilityWorld>(Test, Fixture, Seconds, MoveTemp(Check)));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALAbilityActivationTest, "HAL.FutureCreation.Ability.ActivationDamageAndCooldown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALAbilityActivationTest::RunTest(const FString& Parameters)
{
	auto F = MakeShared<FAbilityWorld>();
	if (!TestNotNull(TEXT("Saved vehicle loads"), F->Vehicle)) { return false; }
	TestTrue(TEXT("Primary grant exists"), F->ASC->HasAbilityForInput(HALGameplayTags::Input_Ability_Primary));
	TestFalse(TEXT("Waiting rejects activation"), F->Activate());
	TestEqual(TEXT("Rejection consumes no cooldown"), F->Cooldown(), 0.0f);
	F->Match->SetMatchPhase(EHALMatchPhase::Playing);
	UVehicleKnockbackSettings* Rules = GetMutableDefault<UVehicleKnockbackSettings>();
	const auto PreviousSource = Rules->ConfigurationSource;
	Rules->ConfigurationSource = EVehicleConfigurationSource::Legacy;
	Rules->InitializeRules();
	TestFalse(TEXT("Legacy impact rollback cannot advertise unsupported immunity"), F->Activate());
	Rules->ConfigurationSource = PreviousSource;
	Rules->InitializeRules();
	TestTrue(TEXT("Playing activates"), F->Activate());
	TestTrue(TEXT("Immune immediately on authority"), F->Immune());
	TestTrue(TEXT("Cooldown starts at activation"), FMath::IsNearlyEqual(F->Cooldown(), 20.0f, 0.1f));
	for (int32 Press = 0; Press < 5; ++Press) { TestFalse(TEXT("Repeated request rejected"), F->Activate()); }
	TestEqual(TEXT("One immunity and one cooldown effect"), F->ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), 2);
	TestFalse(TEXT("GA ends; GE owns duration"), F->ASC->GetActivatableAbilities()[0].IsActive());
	FHALAttackSourceSnapshot Source;
	Source.SourceActor = F->World->SpawnActor<AActor>();
	Source.FixedDamage = 25;
	FHALCombatAction Action;
	Action.Begin(*Source.SourceActor, Source);
	FHALCombatHitRequest Hit;
	Hit.AttackId = Action.GetSnapshot().AttackId;
	Hit.Hit.TargetActor = F->Vehicle;
	Hit.Hit.TargetPhysicsBody = Cast<UPrimitiveComponent>(F->Vehicle->GetRootComponent());
	Hit.Hit.ImpactNormal = FVector(-1, 0, 0);
	Hit.bApplyImpact = true;
	Hit.Impact.Mode = EHALImpactMode::ExplicitTier;
	Hit.Impact.Tier = EHALImpactTier::Heavy;
	const auto Result = FCombatResolver::ResolveCombatHit(Action, Hit);
	TestEqual(TEXT("Immunity does not prevent 25 damage"), Result.ActualHPRemoved, 25.0f);
	TestEqual(TEXT("Heavy resolves to None"), Result.ImpactSummary.EffectiveTier, EHALImpactTier::None);
	TestEqual(TEXT("Real receiver suppressed requested impact"), Result.ImpactOutcome, EHALImpactOutcome::Suppressed);
	TestFalse(TEXT("No rule impulse or drop"), Result.bImpactApplied || Result.ImpactSummary.bBallDropped);
	After(this, F, 8.2f, [this, F]
	{
		TestFalse(TEXT("Eight-second effect expired through GAS timers"), F->Immune());
		TestTrue(TEXT("Cooldown still has about 12s"), F->Cooldown() > 11.5f && F->Cooldown() < 12.2f);
		TestFalse(TEXT("End of immunity does not refund cooldown"), F->Activate());
	});
	After(this, F, 12.0f, [this, F]
	{
		TestEqual(TEXT("Twenty-second cooldown expired"), F->Cooldown(), 0.0f);
		TestTrue(TEXT("Can activate again at t20, not t28"), F->Activate());
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALAbilityRefreshTest, "HAL.FutureCreation.Ability.RefreshDispelAndIndependentStacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALAbilityRefreshTest::RunTest(const FString& Parameters)
{
	auto F = MakeShared<FAbilityWorld>();
	if (!TestNotNull(TEXT("Saved vehicle loads"), F->Vehicle)) { return false; }
	F->Match->SetMatchPhase(EHALMatchPhase::Playing);
	TestTrue(TEXT("Activate native ability"), F->Activate());
	After(this, F, 4.0f, [this, F]
	{
		const float BeforeCooldown = F->Cooldown();
		F->ASC->ApplyGameplayEffectToSelf(GetDefault<UHALImpactImmunityEffect>(), 1, F->ASC->MakeEffectContext());
		TestTrue(TEXT("Reapply refreshes to eight, not twelve"), FMath::IsNearlyEqual(
			F->ASC->GetEffectTimeRemaining(HALGameplayTags::State_Combat_ImpactImmune), 8.0f, 0.1f));
		TestTrue(TEXT("Refresh does not restart cooldown"), FMath::IsNearlyEqual(F->Cooldown(), BeforeCooldown, 0.05f));
		TestEqual(TEXT("Refresh has one immunity stack"), F->ASC->GetTagCount(HALGameplayTags::State_Combat_ImpactImmune), 1);
	});
	After(this, F, 4.2f, [this, F] { TestTrue(TEXT("Refreshed effect survives original t8"), F->Immune()); });
	After(this, F, 4.0f, [this, F]
	{
		TestFalse(TEXT("Refreshed effect expires at t12"), F->Immune());
		const auto Native = F->ASC->ApplyGameplayEffectToSelf(GetDefault<UHALImpactImmunityEffect>(), 1, F->ASC->MakeEffectContext());
		UGameplayEffect* Independent = NewObject<UGameplayEffect>();
		Independent->DurationPolicy = EGameplayEffectDurationType::Infinite;
		FInheritedTagContainer Granted;
		Granted.AddTag(HALGameplayTags::State_Combat_ImpactImmune);
		Independent->FindOrAddComponent<UTargetTagsGameplayEffectComponent>().SetAndApplyTargetTagChanges(Granted);
		const auto Other = F->ASC->ApplyGameplayEffectToSelf(Independent, 1, F->ASC->MakeEffectContext());
		F->Controller->HALImpactDebug(F->State->GetPlayerId(), 0, true);
		F->ASC->RemoveActiveGameplayEffect(Native);
		TestTrue(TEXT("Dispelling this GE preserves another GE"), F->Immune());
		F->ASC->RemoveActiveGameplayEffect(Other);
		TestTrue(TEXT("Other GE removal preserves independent debug source"), F->Immune());
		F->Controller->HALImpactDebug(F->State->GetPlayerId(), 0, false);
		TestFalse(TEXT("No stuck immunity after all sources removed"), F->Immune());
		TestTrue(TEXT("Dispel does not refund cooldown"), F->Cooldown() > 0);
		UGameplayEffect* Stackable = NewObject<UGameplayEffect>();
		Stackable->DurationPolicy = EGameplayEffectDurationType::Infinite;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Stackable->StackingType = EGameplayEffectStackingType::AggregateByTarget; // Same installed-engine export limitation.
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		Stackable->StackLimitCount = 3;
		auto& Modifier = Stackable->Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = UHALImpactAttributeSet::GetTierReductionAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FScalableFloat(1);
		const float Base = F->ASC->GetSet<UHALImpactAttributeSet>()->GetTierReduction();
		const auto Stack = F->ASC->ApplyGameplayEffectToSelf(Stackable, 1, F->ASC->MakeEffectContext());
		F->ASC->ApplyGameplayEffectToSelf(Stackable, 1, F->ASC->MakeEffectContext());
		TestEqual(TEXT("Another GE may accumulate two stacks"), F->ASC->GetSet<UHALImpactAttributeSet>()->GetTierReduction(), Base + 2);
		F->ASC->RemoveActiveGameplayEffect(Stack, 1);
		TestEqual(TEXT("One stack can be removed"), F->ASC->GetSet<UHALImpactAttributeSet>()->GetTierReduction(), Base + 1);
		F->ASC->RemoveActiveGameplayEffect(Stack);
		TestEqual(TEXT("Removing remaining stack restores base"), F->ASC->GetSet<UHALImpactAttributeSet>()->GetTierReduction(), Base);
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALAbilityAvatarTest, "HAL.FutureCreation.Ability.AvatarAndOwnerCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALAbilityAvatarTest::RunTest(const FString& Parameters)
{
	auto F = MakeShared<FAbilityWorld>();
	if (!TestNotNull(TEXT("Saved vehicle loads"), F->Vehicle)) { return false; }
	F->Match->SetMatchPhase(EHALMatchPhase::Playing);
	TestTrue(TEXT("Activate before rebinding"), F->Activate());
	F->State->GrantVehicleAbilities();
	F->ASC->InitAbilityActorInfo(F->State, F->Vehicle);
	TestEqual(TEXT("Repeated binding/grant does not duplicate spec"), F->ASC->GetActivatableAbilities().Num(), 1);
	TestTrue(TEXT("Same Avatar rebind preserves immunity"), F->Immune());
	After(this, F, 2.0f, [this, F]
	{
		ATestVehiclePawn* Old = F->Vehicle;
		F->Vehicle = F->SpawnVehicle();
		if (!TestNotNull(TEXT("Replacement vehicle loads"), F->Vehicle)) { return; }
		F->Controller->Possess(F->Vehicle);
		F->Vehicle->DispatchBeginPlay();
		TestFalse(TEXT("New Avatar does not inherit old immunity"), F->Immune());
		TestTrue(TEXT("Cooldown remains on PlayerState"), F->Cooldown() > 17.5f && F->Cooldown() < 18.2f);
		TestEqual(TEXT("New Avatar still has one grant"), F->ASC->GetActivatableAbilities().Num(), 1);
		F->ASC->ApplyGameplayEffectToSelf(GetDefault<UHALImpactImmunityEffect>(), 1, F->ASC->MakeEffectContext());
		F->ASC->ClearAvatarIfCurrent(Old);
		Old->Destroy();
		TestTrue(TEXT("Old cleanup cannot clear the new Avatar effect"), F->Immune());
		F->Vehicle->Destroy();
		TestFalse(TEXT("Pawn destruction removes Avatar effects even during weak-pointer invalidation"), F->Immune());
		TestNull(TEXT("Pawn destruction clears Avatar"), F->ASC->GetAvatarActor());
		TestTrue(TEXT("No Avatar cannot activate"), !F->Activate());
		TestTrue(TEXT("Pawn loss does not reset cooldown"), F->Cooldown() > 0);
		F->State->Destroy();
		TestEqual(TEXT("Owner exit removes all active effects"), F->ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), 0);
		TestEqual(TEXT("Owner exit removes grants"), F->ASC->GetActivatableAbilities().Num(), 0);
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALAbilityInputIsolationTest, "HAL.FutureCreation.Ability.DiscreteInputDoesNotReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALAbilityInputIsolationTest::RunTest(const FString& Parameters)
{
	FVehicleNetInputData Input;
	Input.InputSequence = 1;
	Input.Cmd.Throttle = 0.7f;
	Input.Cmd.bPrimarySkill = true;
	TestFalse(TEXT("Continuous RPC rejects a skill edge"), Input.IsValidContinuousInput());
	Input.Sanitize();
	TestFalse(TEXT("History capture strips skill input"), Input.Cmd.bPrimarySkill);
	TestEqual(TEXT("Driving input remains"), Input.Cmd.Throttle, 0.7f);
	FVehiclePhysicsHistoryInput Write;
	Write.Input = Input;
	Write.Input.Cmd.bPrimarySkill = true;
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	bool Success = false;
	Write.NetSerialize(Writer, nullptr, Success);
	FVehiclePhysicsHistoryInput Read;
	Read.Input.Cmd.bPrimarySkill = true;
	FMemoryReader Reader(Bytes);
	Read.NetSerialize(Reader, nullptr, Success);
	TestTrue(TEXT("History serialization succeeds"), Success);
	TestFalse(TEXT("Deserialized frames cannot trigger GA"), Read.Input.Cmd.bPrimarySkill);
	TestEqual(TEXT("Continuous throttle still roundtrips"), Read.Input.Cmd.Throttle, 0.7f);
	return true;
}

#endif
