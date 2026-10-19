#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALMovementAbilityTestSupport.h"
#include "../HALMovementAttributeSet.h"
#include "../HALMovementBoostEffect.h"
#include "../HALImpactImmunityEffects.h"
#include "../HALHealthAttributeSet.h"
#include "../HALDebugSettings.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../TestVehiclePawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	struct FMovementAbilityWorld
	{
		UWorld* World;
		AHALMatchGameState* Match;
		AHALPlayerController* Controller;
		AHALPlayerState* State;
		ATestVehiclePawn* Vehicle;
		UHALAbilitySystemComponent* ASC;
		uint64 LastAdvance = MAX_uint64;
		FMovementAbilityWorld()
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
			ASC = State->GetHALAbilitySystemComponent(); HALMovementAbilityTests::ConfigurePrimary(State);
			Vehicle = SpawnVehicle(); if (Vehicle) { Controller->Possess(Vehicle); Vehicle->DispatchBeginPlay(); }
		}
		ATestVehiclePawn* SpawnVehicle()
		{
			auto* Class = LoadClass<ATestVehiclePawn>(nullptr, TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
			if (!Class) { return nullptr; }
			FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Pawn = World->SpawnActor<ATestVehiclePawn>(Class, FVector(0,0,1000), FRotator::ZeroRotator, Params);
			Pawn->PreInitializeComponents(); Pawn->InitializeComponents(); Pawn->PostInitializeComponents(); return Pawn;
		}
		~FMovementAbilityWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
		bool Activate() { return Vehicle->RequestPrimarySkill(); }
		bool Boosted() const { return ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Boost); }
		float Remaining() const { return ASC->GetEffectTimeRemaining(HALGameplayTags::State_Movement_Boost); }
		bool ActionEnded() const
		{
			for (const auto& Spec : ASC->GetActivatableAbilities()) { if (Spec.IsActive()) { return false; } }
			return true;
		}
	};
	class FAdvanceMovementAbilityWorld : public IAutomationLatentCommand
	{
		TSharedRef<FMovementAbilityWorld> Fixture; FAutomationTestBase* Test;
		float Seconds; TOptional<double> Target; double Deadline = 0; TFunction<void()> Check;
	public:
		FAdvanceMovementAbilityWorld(FAutomationTestBase* InTest, TSharedRef<FMovementAbilityWorld> InFixture, float InSeconds, TFunction<void()> InCheck)
			: Fixture(InFixture), Test(InTest), Seconds(InSeconds), Check(MoveTemp(InCheck)) {}
		virtual bool Update() override
		{
			if (!Target.IsSet()) { Target = Fixture->World->GetTimeSeconds() + Seconds; Deadline = FPlatformTime::Seconds()+30; }
			if (FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("Movement ability World/GAS timer timeout")); return true; }
			if (Fixture->World->GetTimeSeconds()+0.001>=Target.GetValue()) { Check(); return true; }
			if (Fixture->LastAdvance==GFrameCounter) { return false; }
			Fixture->LastAdvance=GFrameCounter; Fixture->World->Tick(LEVELTICK_All,0.05f); return false;
		}
	};
	void After(FAutomationTestBase* Test,TSharedRef<FMovementAbilityWorld> Fixture,float Seconds,TFunction<void()> Check)
	{
		FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FAdvanceMovementAbilityWorld>(Test,Fixture,Seconds,MoveTemp(Check)));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMovementAbilityRefreshTest,"HAL.FutureCreation.MovementAbility.ActivationRefreshAndNaturalExpiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALMovementAbilityRefreshTest::RunTest(const FString& Parameters)
{
	auto F=MakeShared<FMovementAbilityWorld>(); if (!TestNotNull(TEXT("Saved vehicle loads"),F->Vehicle)) { return false; }
	TestTrue(TEXT("Exactly one movement Primary replaces immunity"),HALMovementAbilityTests::HasMovementPrimary(F->ASC));
	TestFalse(TEXT("Waiting rejects skill"),F->Activate());
	F->Match->SetMatchPhase(EHALMatchPhase::Playing);
	// The real skill must work with both development apply mechanisms unavailable.
	auto* Switch=IConsoleManager::Get().FindConsoleVariable(TEXT("hal.MovementBoostDebug")); const int32 Previous=Switch->GetInt();
	auto* Settings=GetMutableDefault<UHALDebugSettings>(); const auto PreviousClass=Settings->MovementBoostEffectClass;
	Switch->Set(0,ECVF_SetByCode); Settings->MovementBoostEffectClass.Reset();
	TestTrue(TEXT("Primary skill activates independently of debug command/settings"),F->Activate());
	TestTrue(TEXT("Same-frame skill reapplication refreshes"),F->Activate());
	Switch->Set(Previous,ECVF_SetByCode); Settings->MovementBoostEffectClass=PreviousClass;
	TestTrue(TEXT("GA ends immediately while GE remains active"),F->ActionEnded() && F->Boosted());
	TestEqual(TEXT("One target layer"),F->ASC->GetTagCount(HALGameplayTags::State_Movement_Boost),1);
	TestEqual(TEXT("No immunity"),F->ASC->GetTagCount(HALGameplayTags::State_Combat_ImpactImmune),0);
	TestEqual(TEXT("No immunity cooldown"),F->ASC->GetEffectTimeRemaining(HALGameplayTags::Cooldown_ImpactImmunity),0.f);
	TestEqual(TEXT("Acceleration contribution"),F->ASC->GetSet<UHALMovementAttributeSet>()->GetAccelerationMultiplierDelta(),0.5f);
	After(this,F,2,[this,F]
	{
		TestTrue(TEXT("Second press can refresh despite the earlier GE still running"),F->Activate());
		TestTrue(TEXT("Refresh restarts five game seconds"),FMath::IsNearlyEqual(F->Remaining(),5.f,0.01f));
		TestTrue(TEXT("GA again ended immediately"),F->ActionEnded());
	});
	After(this,F,3.2f,[this,F]{ TestTrue(TEXT("Refresh survives original expiry"),F->Boosted()); });
	After(this,F,2,[this,F]
	{
		TestFalse(TEXT("GAS naturally expires five seconds after refresh"),F->Boosted());
		TestEqual(TEXT("Acceleration restored"),F->ASC->GetSet<UHALMovementAttributeSet>()->GetAccelerationMultiplierDelta(),0.f);
		TestEqual(TEXT("Speed restored"),F->ASC->GetSet<UHALMovementAttributeSet>()->GetSpeedLimitMultiplierDelta(),0.f);
		TestTrue(TEXT("Can use same Primary again after expiry"),F->Activate());
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMovementAbilityLifecycleTest,"HAL.FutureCreation.MovementAbility.AvatarAndOtherCooldownIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALMovementAbilityLifecycleTest::RunTest(const FString& Parameters)
{
	auto F=MakeShared<FMovementAbilityWorld>(); if (!TestNotNull(TEXT("Saved vehicle loads"),F->Vehicle)) { return false; }
	F->Match->SetMatchPhase(EHALMatchPhase::Playing);
	F->ASC->ApplyGameplayEffectToSelf(GetDefault<UHALImpactImmunityCooldownEffect>(),1,F->ASC->MakeEffectContext());
	TestTrue(TEXT("Another skill cooldown cannot block movement skill"),F->Activate());
	F->ASC->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(),75);
	F->State->GrantVehicleAbilities(); TestTrue(TEXT("Repeated grant does not duplicate Primary"),HALMovementAbilityTests::HasMovementPrimary(F->ASC));
	auto* Old=F->Vehicle; auto* Next=F->SpawnVehicle(); if (!TestNotNull(TEXT("Replacement loads"),Next)) { return false; }
	F->Vehicle=Next; F->Controller->Possess(Next); Next->DispatchBeginPlay();
	TestFalse(TEXT("Avatar replacement clears boost"),F->Boosted());
	TestTrue(TEXT("Movement skill remains exactly one Primary"),HALMovementAbilityTests::HasMovementPrimary(F->ASC));
	TestEqual(TEXT("HP preserved"),F->ASC->GetSet<UHALHealthAttributeSet>()->GetCurrentHP(),75.f);
	TestTrue(TEXT("Other cooldown preserved"),F->ASC->HasMatchingGameplayTag(HALGameplayTags::Cooldown_ImpactImmunity));
	TestTrue(TEXT("New Avatar can activate"),F->Activate()); F->ASC->ClearAvatarIfCurrent(Old); Old->Destroy();
	TestTrue(TEXT("Delayed old cleanup preserves new boost"),F->Boosted());
	Next->Destroy(); TestFalse(TEXT("Pawn destruction clears active movement GE"),F->Boosted());
	TestFalse(TEXT("No Avatar rejects input"),F->ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary));
	F->State->Destroy(); TestEqual(TEXT("Owner exit clears all effects"),F->ASC->GetActiveEffects(FGameplayEffectQuery()).Num(),0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMovementAbilityGuardTest,"HAL.FutureCreation.MovementAbility.InvalidEffectAndAvatarGuards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALMovementAbilityGuardTest::RunTest(const FString& Parameters)
{
	auto F=MakeShared<FMovementAbilityWorld>(); if (!TestNotNull(TEXT("Saved vehicle loads"),F->Vehicle)) { return false; }
	F->Match->SetMatchPhase(EHALMatchPhase::Playing);
	UGameplayAbility* Instance=nullptr;
	for (auto& Spec:F->ASC->GetActivatableAbilities()) { if (Spec.Ability->IsA<UHALMovementBoostAbility>()) { Instance=Spec.GetPrimaryInstance(); } }
	if (!TestNotNull(TEXT("Instanced movement ability"),Instance)) { return false; }
	auto* Property=FindFProperty<FClassProperty>(Instance->GetClass(),TEXT("MovementEffectClass"));
	if (!TestNotNull(TEXT("Editor-configurable GE reference"),Property)) { return false; }
	Property->SetObjectPropertyValue_InContainer(Instance,nullptr);
	TestFalse(TEXT("Missing GE rejects without fallback"),F->Activate()); TestFalse(TEXT("No effect was applied"),F->Boosted());
	Property->SetObjectPropertyValue_InContainer(Instance,UHALMovementBoostEffect::StaticClass());
	auto* Effect=GetMutableDefault<UHALMovementBoostEffect>(); const auto Previous=Effect->DurationMagnitude;
	Effect->DurationMagnitude=FScalableFloat(0.f);
	TestFalse(TEXT("Non-positive duration rejects"),F->Activate()); Effect->DurationMagnitude=Previous;
	const auto PreviousPeriod=Effect->Period; Effect->Period=FScalableFloat(1.f);
	TestFalse(TEXT("Periodic GE cannot permanently modify movement base values"),F->Activate()); Effect->Period=PreviousPeriod;
	TestFalse(TEXT("Rejected periodic GE applies no movement effect"),F->Boosted());
	F->ASC->InitAbilityActorInfo(F->State,F->World->SpawnActor<AActor>());
	TestFalse(TEXT("Non-vehicle Avatar rejects"),F->ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary));
	F->ASC->InitAbilityActorInfo(F->State,F->Vehicle);
	TestTrue(TEXT("Valid config activates again"),F->Activate());
	return true;
}
#endif
