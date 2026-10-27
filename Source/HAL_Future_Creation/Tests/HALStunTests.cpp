#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "HALMovementAbilityTestSupport.h"
#include "../HALStunEffect.h"
#include "../HALHealthAttributeSet.h"
#include "../HALMovementAttributeSet.h"
#include "../HALMovementTypes.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../TestVehiclePawn.h"
#include "../BasicBallActor.h"
#include "../BallControlComponent.h"
#include "../VehicleRecoveryComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "HAL/IConsoleManager.h"
#include <limits>

struct FHALStunTestAccess
{
	static bool Acquire(UBallControlComponent* C, ABasicBallActor* B) { return C->AcquireBall(B); }
	static bool Recover(UVehicleRecoveryComponent* C) { return C->Start(FVehicleStabilityObservation()); }
};

namespace HALStunTests
{
	struct FFixture
	{
		UWorld* World;
		AHALPlayerState* State;
		ATestVehiclePawn* Pawn;
		UHALAbilitySystemComponent* ASC;
		FFixture()
		{
			const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
				.ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			auto* Match = World->SpawnActor<AHALMatchGameState>(); World->SetGameState(Match);
			Match->SetMatchPhase(EHALMatchPhase::Playing);
			auto* PC = World->SpawnActor<AHALPlayerController>(); PC->SetAsLocalPlayerController();
			State = World->SpawnActor<AHALPlayerState>(); State->SetOwner(PC); PC->SetPlayerState(State);
			State->PreInitializeComponents(); State->InitializeComponents(); State->PostInitializeComponents(); State->DispatchBeginPlay();
			FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Class = LoadClass<ATestVehiclePawn>(nullptr, TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
			Pawn = World->SpawnActor<ATestVehiclePawn>(Class, FVector(0,0,1000), FRotator::ZeroRotator, Spawn);
			if (Pawn)
			{
				Pawn->PreInitializeComponents(); Pawn->InitializeComponents(); Pawn->PostInitializeComponents();
				PC->Possess(Pawn); Pawn->DispatchBeginPlay();
			}
			ASC = State->GetHALAbilitySystemComponent();
			HALMovementAbilityTests::ConfigurePrimary(State);
		}
		~FFixture() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	};
	class FWait : public IAutomationLatentCommand
	{
		TSharedRef<FFixture> Fixture; FAutomationTestBase* Test; double Target, Deadline;
		uint64 LastFrame = MAX_uint64; TFunction<void()> Check;
	public:
		FWait(TSharedRef<FFixture> In, FAutomationTestBase* T, float Seconds, TFunction<void()> Fn)
			: Fixture(In), Test(T), Target(In->World->GetTimeSeconds()+Seconds), Deadline(FPlatformTime::Seconds()+30), Check(MoveTemp(Fn)) {}
		virtual bool Update() override
		{
			if (Fixture->World->GetTimeSeconds() >= Target) { Check(); return true; }
			if (FPlatformTime::Seconds() > Deadline) { Test->AddError(TEXT("Stun game-time expiry timeout")); return true; }
			if (LastFrame != GFrameCounter) { LastFrame = GFrameCounter; Fixture->World->Tick(LEVELTICK_All, .05f); }
			return false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStunGameplayTest, "HAL.FutureCreation.Stun.PermissionsDropAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStunGameplayTest::RunTest(const FString& Parameters)
{
	HALStunTests::FFixture F;
	if (!TestNotNull(TEXT("Configured player"), F.Pawn)) { return false; }
	auto* Balls = F.Pawn->FindComponentByClass<UBallControlComponent>();
	auto* Ball = F.World->SpawnActor<ABasicBallActor>(); Ball->DispatchBeginPlay();
	TestTrue(TEXT("Can use existing boost before stun"), F.Pawn->RequestPrimarySkill());
	TestTrue(TEXT("Can hold before stun"), FHALStunTestAccess::Acquire(Balls, Ball));
	const FVector Velocity = Ball->GetPhysicsRoot()->GetPhysicsLinearVelocity();
	TestFalse(TEXT("Zero duration rejected"), F.ASC->ApplyStun(0, F.Pawn));
	TestFalse(TEXT("Negative duration rejected"), F.ASC->ApplyStun(-1, F.Pawn));
	TestFalse(TEXT("Nonfinite duration rejected"), F.ASC->ApplyStun(std::numeric_limits<float>::quiet_NaN(), F.Pawn));
	TestTrue(TEXT("Stun applies"), F.ASC->ApplyStun(3, F.Pawn));
	TestTrue(TEXT("Actor query sees GAS state"), UHALAbilitySystemComponent::IsActorStunned(F.Pawn));
	TestTrue(TEXT("Drops immediately to Free"), Ball->GetBallState() == EBasicBallState::Free && !Balls->GetHeldBall());
	TestTrue(TEXT("Drop preserves velocity"), Ball->GetPhysicsRoot()->GetPhysicsLinearVelocity().Equals(Velocity));
	auto* Fresh = F.World->SpawnActor<ABasicBallActor>(); Fresh->DispatchBeginPlay();
	TestFalse(TEXT("Fresh ball blocked independent of reacquire lock"), Fresh->CanBeControlledBy(F.Pawn));
	TestFalse(TEXT("Direct ball ownership blocked"), Fresh->BeginControl(F.Pawn));
	TestFalse(TEXT("Control component acquisition blocked"), FHALStunTestAccess::Acquire(Balls, Fresh));
	FVector Recoil(1); TestFalse(TEXT("Launch blocked"), Balls->LaunchHeldBall(Recoil));
	TestTrue(TEXT("Rejected launch has no recoil"), Recoil.IsZero());
	TestFalse(TEXT("Primary action blocked"), F.Pawn->RequestPrimarySkill());
	for (const auto& Spec : F.ASC->GetActivatableAbilities())
	{ TestFalse(TEXT("Direct GAS activation cannot bypass stun"), F.ASC->TryActivateAbility(Spec.Handle)); }
	TestFalse(TEXT("Recovery start blocked"), FHALStunTestAccess::Recover(F.Pawn->FindComponentByClass<UVehicleRecoveryComponent>()));
	TestTrue(TEXT("Already active boost remains"), F.ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_Boost));
	const uint32 Sequence = Ball->GetBallStateSequence();
	TestTrue(TEXT("Short repeat accepted without shortening"), F.ASC->ApplyStun(1, Fresh));
	TestTrue(TEXT("Original remaining retained"), F.ASC->GetEffectTimeRemaining(HALGameplayTags::State_Stun) > 2.9f);
	TestTrue(TEXT("Longer source extends"), F.ASC->ApplyStun(5, Fresh));
	TestEqual(TEXT("One active canonical stun"), F.ASC->GetTagCount(HALGameplayTags::State_Stun), 1);
	TestEqual(TEXT("Repeat does not repeat drop"), Ball->GetBallStateSequence(), Sequence);
	F.ASC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(FGameplayTagContainer(HALGameplayTags::Effect_Stun)));
	TestFalse(TEXT("Dispel removes state"), F.ASC->IsStunned());
	TestTrue(TEXT("Fresh pickup recovers"), Fresh->CanBeControlledBy(F.Pawn));
	TestTrue(TEXT("Ability permission recovers"), F.Pawn->RequestPrimarySkill());
	F.ASC->ApplyStun(3, nullptr); F.ASC->ClearAvatarIfCurrent(F.Pawn);
	TestFalse(TEXT("Avatar exit clears stun"), F.ASC->IsStunned());
	TestFalse(TEXT("Missing Avatar rejected"), F.ASC->ApplyStun(3, nullptr));
	TestEqual(TEXT("Stun never changes HP"), F.ASC->GetNumericAttribute(UHALHealthAttributeSet::GetCurrentHPAttribute()), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStunDebugCommandTest, "HAL.FutureCreation.Stun.DebugConsoleTargetAndValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStunDebugCommandTest::RunTest(const FString& Parameters)
{
	HALStunTests::FFixture F;
	if (!TestNotNull(TEXT("Configured player"), F.Pawn)) { return false; }
	auto* PC = Cast<AHALPlayerController>(F.Pawn->GetController());
	F.State->SetPlayerId(777);
	const auto* Function = PC->FindFunction(TEXT("HALStun"));
	TestTrue(TEXT("Console command registered as Exec"), Function && Function->HasAnyFunctionFlags(FUNC_Exec));
	auto* Enabled = IConsoleManager::Get().FindConsoleVariable(TEXT("hal.StunDebug"));
	if (!TestNotNull(TEXT("Authority debug switch"),Enabled)) { return false; }
	const int32 Previous = Enabled->GetInt(); Enabled->Set(1,ECVF_SetByCode);
	for (const float Invalid : {0.f,-1.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})
	{ PC->HALStun(777,Invalid); TestFalse(TEXT("Invalid command duration rejected"),F.ASC->IsStunned()); }
	PC->HALStun(-1,3); PC->HALStun(999999,3);
	TestFalse(TEXT("Invalid or missing ID never targets own player"),F.ASC->IsStunned());
	F.World->GetGameState<AHALMatchGameState>()->SetMatchPhase(EHALMatchPhase::WaitingForPlayers);
	PC->HALStun(777,3); TestFalse(TEXT("Waiting command rejected"),F.ASC->IsStunned());
	F.World->GetGameState<AHALMatchGameState>()->SetMatchPhase(EHALMatchPhase::Playing);
	Enabled->Set(0,ECVF_SetByCode); PC->HALStun(777,3);
	TestFalse(TEXT("Server debug switch rejects command"),F.ASC->IsStunned()); Enabled->Set(1,ECVF_SetByCode);
	F.ASC->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(),0);
	PC->HALStun(777,3); TestFalse(TEXT("Dead target rejected"),F.ASC->IsStunned());
	F.ASC->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(),100);
	// Add a second possessed player in the same world to verify target selection, not just self-stun.
	auto* OtherPC = F.World->SpawnActor<AHALPlayerController>();
	auto* OtherState = F.World->SpawnActor<AHALPlayerState>();
	OtherState->SetOwner(OtherPC); OtherPC->SetPlayerState(OtherState); OtherState->SetPlayerId(778);
	OtherState->PreInitializeComponents(); OtherState->InitializeComponents(); OtherState->PostInitializeComponents(); OtherState->DispatchBeginPlay();
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Other = F.World->SpawnActor<ATestVehiclePawn>(F.Pawn->GetClass(),FVector(3000,0,1000),FRotator::ZeroRotator,Spawn);
	if (TestNotNull(TEXT("Second player"),Other))
	{
		Other->PreInitializeComponents(); Other->InitializeComponents(); Other->PostInitializeComponents(); OtherPC->Possess(Other); Other->DispatchBeginPlay();
		auto* OtherASC = OtherState->GetHALAbilitySystemComponent();
		OtherPC->HALStun(777,30); TestFalse(TEXT("Nonlocal authority controller cannot invoke host command"),F.ASC->IsStunned());
		auto* Ball = F.World->SpawnActor<ABasicBallActor>(); Ball->DispatchBeginPlay();
		auto* Balls = Other->FindComponentByClass<UBallControlComponent>();
		TestTrue(TEXT("Target holds ball before command"),FHALStunTestAccess::Acquire(Balls,Ball));
		PC->HALStun(778,3);
		TestTrue(TEXT("Host command stuns selected other player"),OtherASC->IsStunned());
		TestFalse(TEXT("Unselected host unaffected"),F.ASC->IsStunned());
		TestTrue(TEXT("Command immediately drops target ball"),!Balls->GetHeldBall() && Ball->GetBallState()==EBasicBallState::Free);
		PC->HALStun(778,1); TestTrue(TEXT("Short command never shortens"),OtherASC->GetEffectTimeRemaining(HALGameplayTags::State_Stun)>2.9f);
		PC->HALStun(778,5); TestTrue(TEXT("Long command extends"),OtherASC->GetEffectTimeRemaining(HALGameplayTags::State_Stun)>4.9f);
		TestEqual(TEXT("Command keeps one stun"),OtherASC->GetTagCount(HALGameplayTags::State_Stun),1);
	}
	PC->HALStun(777,2); TestTrue(TEXT("Host can also select own player"),F.ASC->IsStunned());
	PC->HALGASStatus();
	Enabled->Set(Previous,ECVF_SetByCode);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStunExpiryTest, "HAL.FutureCreation.Stun.NaturalGameTimeExpiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStunExpiryTest::RunTest(const FString& Parameters)
{
	auto F = MakeShared<HALStunTests::FFixture>();
	if (!TestNotNull(TEXT("Configured player"), F->Pawn)) { return false; }
	TestTrue(TEXT("Timed stun applied"), F->ASC->ApplyStun(.3f, F->Pawn));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<HALStunTests::FWait>(F, this, .15f, [this,F]()
	{
		TestTrue(TEXT("Active before deadline"), F->ASC->IsStunned());
		TestTrue(TEXT("Extend from a second source"), F->ASC->ApplyStun(.4f, nullptr));
		FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<HALStunTests::FWait>(F, this, .25f, [this,F]()
		{
			TestTrue(TEXT("Still active beyond original expiry"), F->ASC->IsStunned());
			FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<HALStunTests::FWait>(F, this, .3f, [this,F]()
			{
				TestFalse(TEXT("Naturally expires after extension"), F->ASC->IsStunned());
				TestTrue(TEXT("Action recovers on expiry"), F->Pawn->RequestPrimarySkill());
			}));
		}));
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStunHistoryTest, "HAL.FutureCreation.Stun.DiscretePhysicsHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStunHistoryTest::RunTest(const FString& Parameters)
{
	FHALMovementSnapshot A; A.Epoch = FGuid::NewGuid(); A.Revision = 1; A.EffectiveServerFrame = 10;
	FHALMovementTimeline Timeline; Timeline.Append(A,64);
	auto B = A; B.Revision = 2; B.EffectiveServerFrame = 15; B.bControlsBlocked = true; Timeline.Append(B,64);
	auto C = A; C.Revision = 3; C.EffectiveServerFrame = 25; Timeline.Append(C,64); Timeline.ExtendTo(30,64);
	TArray<uint8> Bytes; { FMemoryWriter Writer(Bytes); TestTrue(TEXT("Stun history serializes"), Timeline.Serialize(Writer)); }
	FHALMovementTimeline Read; { FMemoryReader Reader(Bytes); TestTrue(TEXT("Stun history deserializes"), Read.Serialize(Reader)); }
	FHALMovementSnapshot Value;
	Read.Query(14, Value); TestFalse(TEXT("Before stun"), Value.bControlsBlocked);
	Read.Query(15, Value); TestTrue(TEXT("Exact stun boundary"), Value.bControlsBlocked);
	Read.Query(24, Value); TestTrue(TEXT("Replay preserves past stun after expiry"), Value.bControlsBlocked);
	Read.Query(25, Value); TestFalse(TEXT("Exact expiry boundary"), Value.bControlsBlocked);
	auto Conflict = B; Conflict.bControlsBlocked = false;
	TestFalse(TEXT("Permission difference is a physics mismatch"), B.Equals(Conflict));
	return true;
}
#endif
