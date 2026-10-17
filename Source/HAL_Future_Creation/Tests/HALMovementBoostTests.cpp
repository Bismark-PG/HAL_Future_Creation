#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../HALMovementBoostEffect.h"
#include "../HALMovementAttributeSet.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALDebugSettings.h"
#include "../HALGameplayTags.h"
#include "../HALHealthAttributeSet.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../TestVehiclePawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	struct FBoostWorld
	{
		UWorld* World;
		AHALMatchGameState* Match;
		AHALPlayerController* Controller;
		AHALPlayerState* State;
		ATestVehiclePawn* Vehicle;
		UHALAbilitySystemComponent* ASC;
		TSoftClassPtr<UHALMovementBoostEffect> PreviousClass = GetDefault<UHALDebugSettings>()->MovementBoostEffectClass;
		uint64 LastAdvanceFrame = MAX_uint64;
		IConsoleVariable* DebugEnabled = IConsoleManager::Get().FindConsoleVariable(TEXT("hal.MovementBoostDebug"));
		int32 PreviousDebugEnabled = DebugEnabled->GetInt();
		FBoostWorld()
		{
			const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
				.ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Match = World->SpawnActor<AHALMatchGameState>(); World->SetGameState(Match);
			Controller = World->SpawnActor<AHALPlayerController>(); Controller->SetAsLocalPlayerController();
			State = World->SpawnActor<AHALPlayerState>(); State->SetOwner(Controller); Controller->SetPlayerState(State);
			State->PreInitializeComponents(); State->InitializeComponents(); State->PostInitializeComponents(); State->DispatchBeginPlay();
			ASC = State->GetHALAbilitySystemComponent(); Vehicle = SpawnVehicle();
			if (Vehicle) { Controller->Possess(Vehicle); Vehicle->DispatchBeginPlay(); }
			GetMutableDefault<UHALDebugSettings>()->MovementBoostEffectClass = UHALMovementBoostEffect::StaticClass();
			DebugEnabled->Set(1, ECVF_SetByCode);
		}
		ATestVehiclePawn* SpawnVehicle()
		{
			auto* Class = LoadClass<ATestVehiclePawn>(nullptr, TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
			if (!Class) { return nullptr; }
			FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Pawn = World->SpawnActor<ATestVehiclePawn>(Class, FVector(0,0,1000), FRotator::ZeroRotator, Params);
			Pawn->PreInitializeComponents(); Pawn->InitializeComponents(); Pawn->PostInitializeComponents(); return Pawn;
		}
		~FBoostWorld()
		{
			World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
			GetMutableDefault<UHALDebugSettings>()->MovementBoostEffectClass = PreviousClass;
			DebugEnabled->Set(PreviousDebugEnabled, ECVF_SetByCode);
		}
		bool Active() const { return ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Boost); }
		float Remaining() const { return ASC->GetEffectTimeRemaining(HALGameplayTags::State_Movement_Boost); }
		const UHALMovementAttributeSet* Values() const { return ASC->GetSet<UHALMovementAttributeSet>(); }
	};
	class FAdvanceBoostWorld : public IAutomationLatentCommand
	{
		TSharedRef<FBoostWorld> Fixture; FAutomationTestBase* Test; float Seconds;
		TOptional<double> Target; double Deadline = 0; TFunction<void()> Check;
	public:
		FAdvanceBoostWorld(FAutomationTestBase* InTest, TSharedRef<FBoostWorld> InFixture, float InSeconds, TFunction<void()> InCheck)
			: Fixture(InFixture), Test(InTest), Seconds(InSeconds), Check(MoveTemp(InCheck)) {}
		virtual bool Update() override
		{
			if (!Target.IsSet()) { Target = Fixture->World->GetTimeSeconds() + Seconds; Deadline = FPlatformTime::Seconds()+30; }
			if (FPlatformTime::Seconds() > Deadline) { Test->AddError(TEXT("Boost World/GAS timer timeout")); return true; }
			if (Fixture->World->GetTimeSeconds()+0.001 >= Target.GetValue()) { Check(); return true; }
			if (Fixture->LastAdvanceFrame == GFrameCounter) { return false; }
			Fixture->LastAdvanceFrame = GFrameCounter; Fixture->World->Tick(LEVELTICK_All, 0.05f); return false;
		}
	};
	void After(FAutomationTestBase* Test, TSharedRef<FBoostWorld> Fixture, float Seconds, TFunction<void()> Check)
	{
		FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FAdvanceBoostWorld>(Test, Fixture, Seconds, MoveTemp(Check)));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBoostExpiryTest, "HAL.FutureCreation.Movement.BoostFiveGameSecondExpiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBoostExpiryTest::RunTest(const FString& Parameters)
{
	auto F = MakeShared<FBoostWorld>(); if (!TestNotNull(TEXT("Authored Avatar loads"), F->Vehicle)) { return false; }
	F->Match->SetMatchPhase(EHALMatchPhase::Playing); F->Controller->HALMovementBoost();
	TestTrue(TEXT("Debug path applied the real boost GE"), F->Active());
	TestTrue(TEXT("Native duration is five game seconds"), FMath::IsNearlyEqual(F->Remaining(), 5.f, 0.01f));
	TestEqual(TEXT("Acceleration contribution"), F->Values()->GetAccelerationMultiplierDelta(), 0.5f);
	TestEqual(TEXT("Speed limit contribution"), F->Values()->GetSpeedLimitMultiplierDelta(), 0.25f);
	After(this,F,4.8f,[this,F]{ TestTrue(TEXT("Effect still active before five"), F->Active()); });
	After(this,F,0.4f,[this,F]
	{
		TestFalse(TEXT("Natural GAS expiry after five, no manual removal"), F->Active());
		TestEqual(TEXT("Acceleration contribution removed"), F->Values()->GetAccelerationMultiplierDelta(), 0.f);
		TestEqual(TEXT("Speed contribution removed"), F->Values()->GetSpeedLimitMultiplierDelta(), 0.f);
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBoostRefreshTest, "HAL.FutureCreation.Movement.BoostCrossSourceRefreshAndSelectiveDispel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBoostRefreshTest::RunTest(const FString& Parameters)
{
	auto F = MakeShared<FBoostWorld>(); if (!TestNotNull(TEXT("Authored Avatar loads"), F->Vehicle)) { return false; }
	F->Match->SetMatchPhase(EHALMatchPhase::Playing); F->Controller->HALMovementBoost();
	After(this,F,2.f,[this,F]
	{
		auto* SourceState = F->World->SpawnActor<AHALPlayerState>();
		auto* Source = SourceState->GetHALAbilitySystemComponent(); Source->InitAbilityActorInfo(SourceState, SourceState);
		const auto Spec=Source->MakeOutgoingSpec(UHALMovementBoostEffect::StaticClass(),1,Source->MakeEffectContext());
		Source->ApplyGameplayEffectSpecToTarget(*Spec.Data,F->ASC);
		TestEqual(TEXT("Across sources one target layer"), F->ASC->GetTagCount(HALGameplayTags::State_Movement_Boost),1);
		TestEqual(TEXT("Refresh never doubles acceleration"), F->Values()->GetAccelerationMultiplierDelta(),0.5f);
		TestEqual(TEXT("Refresh never doubles speed"), F->Values()->GetSpeedLimitMultiplierDelta(),0.25f);
		TestTrue(TEXT("Refresh restarts five, does not add remaining three"), FMath::IsNearlyEqual(F->Remaining(),5.f,0.01f));
	});
	After(this,F,3.2f,[this,F]{ TestTrue(TEXT("Refreshed effect survives original expiry"),F->Active()); });
	After(this,F,2.f,[this,F]
	{
		TestFalse(TEXT("Natural expiry five seconds after refresh"),F->Active());
		UGameplayEffect* Other = NewObject<UGameplayEffect>(); Other->DurationPolicy = EGameplayEffectDurationType::Infinite;
		FGameplayModifierInfo Mod; Mod.Attribute=UHALMovementAttributeSet::GetAccelerationMultiplierDeltaAttribute();
		Mod.ModifierOp=EGameplayModOp::Additive; Mod.ModifierMagnitude=FScalableFloat(-0.2f); Other->Modifiers.Add(Mod);
		const auto Independent=F->ASC->ApplyGameplayEffectToSelf(Other,1,F->ASC->MakeEffectContext());
		F->Controller->HALMovementBoost(); F->Controller->HALMovementBoost();
		TestTrue(TEXT("Different GE combines independently"),FMath::IsNearlyEqual(F->Values()->GetAccelerationMultiplierDelta(),0.3f));
		F->Controller->HALClearMovementBoost(); F->Controller->HALClearMovementBoost();
		TestFalse(TEXT("Selective clear is idempotent"),F->Active());
		TestTrue(TEXT("Clear preserves independent movement GE"),FMath::IsNearlyEqual(F->Values()->GetAccelerationMultiplierDelta(),-0.2f));
		F->ASC->RemoveActiveGameplayEffect(Independent);
		TestEqual(TEXT("All contributions removed"),F->Values()->GetAccelerationMultiplierDelta(),0.f);
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBoostLifecycleTest, "HAL.FutureCreation.Movement.BoostDebugGuardsAndAvatarCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBoostLifecycleTest::RunTest(const FString& Parameters)
{
	auto F = MakeShared<FBoostWorld>(); if (!TestNotNull(TEXT("Authored Avatar loads"), F->Vehicle)) { return false; }
	F->Controller->HALMovementBoost(); TestFalse(TEXT("Waiting phase rejects request"),F->Active());
	F->Match->SetMatchPhase(EHALMatchPhase::Playing);
	F->DebugEnabled->Set(0,ECVF_SetByCode);
	F->Controller->HALMovementBoost(); TestFalse(TEXT("Server debug switch rejects application"),F->Active());
	F->DebugEnabled->Set(1,ECVF_SetByCode);
	GetMutableDefault<UHALDebugSettings>()->MovementBoostEffectClass.Reset();
	F->Controller->HALMovementBoost(); TestFalse(TEXT("Missing configured class has no fallback"),F->Active());
	GetMutableDefault<UHALDebugSettings>()->MovementBoostEffectClass=UHALMovementBoostEffect::StaticClass();
	F->Controller->HALMovementBoost();
	F->DebugEnabled->Set(0,ECVF_SetByCode); F->Controller->HALClearMovementBoost();
	TestFalse(TEXT("Disabled apply switch still permits selective cleanup"),F->Active());
	F->DebugEnabled->Set(1,ECVF_SetByCode); F->Controller->HALMovementBoost();
	TestTrue(TEXT("Original primary remains granted"), F->ASC->HasAbilityForInput(HALGameplayTags::Input_Ability_Primary));
	TestTrue(TEXT("Original immunity can still activate"),F->ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary));
	F->Controller->HALClearMovementBoost();
	TestTrue(TEXT("Boost clear preserves immunity"),F->ASC->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune));
	TestTrue(TEXT("Boost creates no independent cooldown or extra ability"), F->ASC->GetActivatableAbilities().Num()==1);
	F->ASC->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(),75);
	F->Controller->HALMovementBoost();
	auto* Old = F->Vehicle; auto* Replacement=F->SpawnVehicle();
	if (!TestNotNull(TEXT("Replacement Avatar loads"),Replacement)) { return false; }
	F->Vehicle=Replacement; F->Controller->Possess(Replacement); Replacement->DispatchBeginPlay();
	TestFalse(TEXT("Avatar scope clears boost"),F->Active());
	TestEqual(TEXT("New Avatar neutral movement"),F->Values()->GetAccelerationMultiplierDelta(),0.f);
	TestEqual(TEXT("HP retained"),F->ASC->GetSet<UHALHealthAttributeSet>()->GetCurrentHP(),75.f);
	TestTrue(TEXT("Other cooldown retained"),F->ASC->HasMatchingGameplayTag(HALGameplayTags::Cooldown_ImpactImmunity));
	F->Controller->HALMovementBoost(); F->ASC->ClearAvatarIfCurrent(Old); Old->Destroy();
	TestTrue(TEXT("Delayed old cleanup cannot erase new Avatar boost"),F->Active());
	Replacement->Destroy(); TestFalse(TEXT("Pawn destruction clears boost"),F->Active());
	TestEqual(TEXT("No lingering movement contribution"),F->Values()->GetAccelerationMultiplierDelta(),0.f);
	F->Controller->HALMovementBoost(); TestFalse(TEXT("No valid Avatar rejects request"),F->Active());
	F->State->Destroy(); TestEqual(TEXT("Owner exit clears remaining effects"),F->ASC->GetActiveEffects(FGameplayEffectQuery()).Num(),0);
	return true;
}
#endif
