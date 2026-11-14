#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALVehicleAbilityTestSupport.h"
#include "HALRoomTestSupport.h"
#include "../HALBullChargeAbility.h"
#include "../HALBullChargeComponent.h"
#include "../HALImpactReceiverComponent.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALHealthAttributeSet.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../HALMovementEffectBridge.h"
#include "../VehicleHealthComponent.h"
#include "../BasicBallActor.h"
#include "../BallControlComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/ScopeExit.h"
#include "EventsData.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include <limits>

struct FHALBullChargeTestAccess
{
	static uint32 FXCount(ATestVehiclePawn* Pawn)
	{ return Pawn->FindComponentByClass<UHALBullChargeComponent>()->ReceivedImpactFXCount; }
	static bool FXDriving(ATestVehiclePawn* Pawn)
	{
		auto* C = Pawn->FindComponentByClass<UHALBullChargeComponent>(); C->PublishDrivingVisual(); return C->VisualState.bDriving;
	}
	static void FlushContacts(ATestVehiclePawn* Pawn)
	{ Pawn->FindComponentByClass<UHALBullChargeComponent>()->ResolveDeferredContacts(Pawn->GetWorld(), LEVELTICK_All, 0); }
	static bool Acquire(ATestVehiclePawn* Pawn, ABasicBallActor* Ball)
	{ return Pawn->FindComponentByClass<UBallControlComponent>()->AcquireBall(Ball); }
	static void SetHeldReference(ATestVehiclePawn* Pawn, ABasicBallActor* Ball)
	{ Pawn->FindComponentByClass<UBallControlComponent>()->HeldBall = Ball; }
	static void Pose(ATestVehiclePawn* Pawn, int32 Frame = 10)
	{
		auto* Movement = Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>();
		Movement->bShouldRunPhysics.Store(true);
		Movement->PublishedStability.bValid = true;
		Movement->PublishedStability.bGrounded = true;
		Movement->PublishedStability.bDrivingAllowed = true;
		Movement->PublishedStability.Up = Movement->PublishedStability.GroundNormal = FVector::UpVector;
		Movement->PublishedContact.bValid = true;
		Movement->PublishedContact.Position = Pawn->GetActorLocation();
		Movement->PublishedContact.Rotation = Pawn->GetActorQuat();
		auto Snapshot = FHALRoomTestAccess::Flush(Pawn->FindComponentByClass<UHALMovementEffectBridge>());
		Snapshot.Stamp(nullptr, Frame, 1.f / 60);
		auto& Status = Movement->PublishedMovementStatus;
		Status.Definition = Status.Value = Snapshot;
		Status.ServerFrame = Frame;
		Status.bReady = true;
	}
	static void Contact(ATestVehiclePawn* Source, UPrimitiveComponent* Target, FVector Before, FVector OtherBefore = FVector::ZeroVector)
	{
		static double Batch = 100;
		Chaos::FCollisionEventData Event;
		Event.CollisionData.TimeCreated = ++Batch;
		auto& Data = Event.CollisionData.AllCollisionsArray.AddDefaulted_GetRef();
		Data.Proxy1 = CastChecked<UPrimitiveComponent>(Source->GetRootComponent())->GetBodyInstance()->GetPhysicsActor();
		Data.Proxy2 = Target->GetBodyInstance()->GetPhysicsActor();
		Data.Velocity1 = FVector::ZeroVector;
		Data.DeltaVelocity1 = -Before;
		Data.Velocity2 = FVector::ZeroVector;
		Data.DeltaVelocity2 = -OtherBefore;
		Data.SolverTime = Batch;
		Source->FindComponentByClass<UHALBullChargeComponent>()->CollisionEvents(Event);
	}
};

namespace
{
	struct FChargeTestLifetime
	{
		UWorld* World;
		UHALBullChargeConfig* Previous;
		FChargeTestLifetime(UWorld* InWorld, UHALBullChargeConfig* InPrevious) : World(InWorld), Previous(InPrevious) {}
		~FChargeTestLifetime()
		{
			GetMutableDefault<UHALBullChargeAbility>()->SkillConfig = Previous;
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
	};
	class FAdvanceChargeWorld : public IAutomationLatentCommand
	{
		TSharedRef<FChargeTestLifetime> Lifetime;
		double Until;
		uint64 LastFrame = MAX_uint64;
		TFunction<void()> Check;
	public:
		FAdvanceChargeWorld(TSharedRef<FChargeTestLifetime> InLifetime, float Seconds, TFunction<void()> InCheck)
			: Lifetime(InLifetime), Until(InLifetime->World->GetTimeSeconds() + Seconds), Check(MoveTemp(InCheck)) {}
		bool Update() override
		{
			if (Lifetime->World->GetTimeSeconds() >= Until) { Check(); return true; }
			if (LastFrame != GFrameCounter) { LastFrame = GFrameCounter; Lifetime->World->Tick(LEVELTICK_All, .05f); }
			return false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBullChargeProfileTest, "HAL.FutureCreation.BullCharge.MotionHistoryAndSerialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBullChargeProfileTest::RunTest(const FString&)
{
	FHALMovementSnapshot Snapshot;
	Snapshot.Epoch = FGuid::NewGuid(); Snapshot.Revision = 1;
	Snapshot.Overdrive.Activation = FGuid::NewGuid(); Snapshot.Overdrive.bActive = true;
	Snapshot.Stamp(nullptr, 100, 1.f / 60);
	TestTrue(TEXT("Valid stamped charge"), Snapshot.IsValid());
	TestTrue(TEXT("No boost before charge"), Snapshot.EvaluateScales(99).Equals(FVector::OneVector));
	TestTrue(TEXT("Gentle initial force"), FMath::IsNearlyEqual(Snapshot.EvaluateScales(100).X, .4));
	auto Saturated = Snapshot;
	Saturated.bHasAreaState = true;
	Saturated.AreaAcceleration.OrdinaryDelta = 3;
	TestEqual(TEXT("Charge ramp preserves contributions before final clamp"), Saturated.EvaluateScales(100).X, 3.);
	TestTrue(TEXT("Full overdrive at ramp end"), Snapshot.EvaluateScales(220).Equals(FVector(1.5, 1.7, 1), .0001));
	TestTrue(TEXT("Ordinary parameters at duration boundary"), Snapshot.EvaluateScales(400).Equals(FVector::OneVector));
	TestTrue(TEXT("Separate recovery after gameplay"), Snapshot.Overdrive.IsRecovering(400));
	TestEqual(TEXT("Steering restored after recovery"), Snapshot.Overdrive.SteeringAt(430), 1.f);
	auto Stunned = Snapshot; Stunned.Revision = 2; Stunned.Overdrive.bStunned = Stunned.bControlsBlocked = true;
	Stunned.Stamp(&Snapshot, 150, 1.f / 60);
	TestEqual(TEXT("Stun stop has exact frame"), Stunned.Overdrive.StunFrame, 150);
	TestEqual(TEXT("Stun does not restart ramp"), Stunned.Overdrive.StartFrame, 100);
	auto InvalidBoundary = Stunned;
	InvalidBoundary.Overdrive.StunFrame = 151;
	TestFalse(TEXT("A received snapshot cannot invent a future stun boundary"), InvalidBoundary.IsValid());
	InvalidBoundary = Stunned; InvalidBoundary.Overdrive.StartFrame = 151;
	TestFalse(TEXT("A received snapshot cannot invent a future activation boundary"), InvalidBoundary.IsValid());
	auto Refreshed = Stunned; Refreshed.Revision = 3; Refreshed.Stamp(&Stunned, 170, 1.f / 60);
	TestEqual(TEXT("Other buffs do not reapply stun stop"), Refreshed.Overdrive.StunFrame, 150);
	auto Stopped = Snapshot; Stopped.Revision = 4; Stopped.Overdrive.bActive = false;
	Stopped.Stamp(&Snapshot, 200, 1.f / 60);
	TestEqual(TEXT("Early exit stamps recovery boundary"), Stopped.Overdrive.EndFrame, 200);
	auto Cancelled = Snapshot; Cancelled.Revision = 1; Cancelled.Overdrive.bActive = false;
	Cancelled.Stamp(nullptr, 100, 1.f / 60);
	TestFalse(TEXT("Cancellation before first physics acknowledgement cannot start a charge"), Cancelled.Overdrive.IsDriving(101));
	TestFalse(TEXT("Cancelled zero-length activation cannot brake or weaken steering"), Cancelled.Overdrive.IsRecovering(100));
	FHALMovementTimeline Timeline;
	Timeline.Append(Snapshot, 250); Timeline.Append(Stunned, 250); Timeline.Append(Stopped, 250);
	TArray<uint8> Bytes;
	{ FMemoryWriter Writer(Bytes); TestTrue(TEXT("Serialize full charge history"), Timeline.Serialize(Writer)); }
	FHALMovementTimeline Read;
	{ FMemoryReader Reader(Bytes); TestTrue(TEXT("Deserialize full charge history"), Read.Serialize(Reader)); }
	FHALMovementSnapshot Old;
	Read.Query(150, Old);
	TestTrue(TEXT("Historical stun and ramp survive expiry"), Old.Equals(Stunned));
	Snapshot.Overdrive.FinalSpeedScale = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Nonfinite motion rejected"), Snapshot.IsValidValues());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBullChargeGeometryTest, "HAL.FutureCreation.BullCharge.FrontSpeedDamageAndCaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBullChargeGeometryTest::RunTest(const FString&)
{
	const auto* Config = GetDefault<UHALBullChargeConfig>();
	FString Error;
	TestTrue(TEXT("Default charge is configurable and valid"), Config->Validate(Error));
	float Speed;
	TestTrue(TEXT("Frontal closing hit"), Config->Qualifies(FVector::ForwardVector, FVector::ForwardVector,
		FVector::ForwardVector, FVector(2500, 0, 0), FVector::ZeroVector, Speed));
	TestFalse(TEXT("Side scrape excluded"), Config->Qualifies(FVector::ForwardVector, FVector::RightVector,
		FVector::ForwardVector, FVector(2500, 0, 0), FVector::ZeroVector, Speed));
	TestFalse(TEXT("Target pulling away excluded"), Config->Qualifies(FVector::ForwardVector, FVector::ForwardVector,
		FVector::ForwardVector, FVector(2500, 0, 0), FVector(3000, 0, 0), Speed));
	TestFalse(TEXT("Falling speed cannot qualify"), Config->Qualifies(FVector::ForwardVector, FVector::ForwardVector,
		FVector::ForwardVector, FVector(10, 0, 5000), FVector::ZeroVector, Speed));
	TestTrue(TEXT("Damage increases with forward speed"), Config->DamageMultiplier(600) < Config->DamageMultiplier(2500));
	TestEqual(TEXT("Damage capped"), Config->DamageMultiplier(100000), Config->MaximumDamageMultiplier);
	TestEqual(TEXT("Ball speed capped"), Config->BallSpeed(100000), Config->MaximumBallSpeed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBullChargeGASTest, "HAL.FutureCreation.BullCharge.GASContactsCooldownStunAndBall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBullChargeGASTest::RunTest(const FString&)
{
	const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Ability = GetMutableDefault<UHALBullChargeAbility>();
	auto* Previous = Ability->SkillConfig.Get();
	auto Lifetime = MakeShared<FChargeTestLifetime>(World, Previous);
	auto* Config = NewObject<UHALBullChargeConfig>(Ability);
	Config->MinimumDamageMultiplier = Config->MaximumDamageMultiplier = 1;
	Config->FX.bEnabled = true; // Exercise the real accepted-contact VFX boundary without requiring rendering.
	Ability->SkillConfig = Config;
	auto* Match = World->SpawnActor<AHALMatchGameState>(); World->SetGameState(Match); Match->SetMatchPhase(EHALMatchPhase::Playing);
	int32 PlayerId = 0;
	const auto MakeCar = [&]()
	{
		auto* Class = LoadClass<ATestVehiclePawn>(nullptr, TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_04.BP_VehiclePawn_Car_04_C"));
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Car = World->SpawnActor<ATestVehiclePawn>(Class, FVector(0, ++PlayerId * 1500, 2000), FRotator::ZeroRotator, Params);
		auto* Definition = FHALVehicleAbilityTestAccess::Configure(Car, Ability->GetClass());
		Definition->Combat.Version = EHALCombatConfigVersion::AttackSnapshotV1;
		FHALVehicleAbilityTestAccess::RuntimeConfig(Car, Definition->Abilities);
		Car->PreInitializeComponents(); Car->InitializeComponents(); Car->PostInitializeComponents();
		auto* PC = World->SpawnActor<AHALPlayerController>(); PC->SetAsLocalPlayerController();
		auto* PS = World->SpawnActor<AHALPlayerState>(); PS->SetOwner(PC); PC->SetPlayerState(PS); PS->SetPlayerId(PlayerId);
		PS->PreInitializeComponents(); PS->InitializeComponents(); PS->PostInitializeComponents(); PS->DispatchBeginPlay();
		PS->ChangeVehicleAvatar(Car, 1); Car->DispatchBeginPlay(); FHALBullChargeTestAccess::Pose(Car);
		return Car;
	};
	auto* A = MakeCar(); auto* B = MakeCar();
	auto* ASC = CastChecked<UHALAbilitySystemComponent>(A->GetAbilitySystemComponent());
	auto* TargetASC = CastChecked<UHALAbilitySystemComponent>(B->GetAbilitySystemComponent());
	const auto MakeBall = [&]()
	{
		auto* Class = LoadClass<ABasicBallActor>(nullptr, TEXT("/Game/Blueprints/Balls/BP_BasicBall.BP_BasicBall_C"));
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Ball = World->SpawnActor<ABasicBallActor>(Class, A->GetActorLocation() + FVector(200, 0, 0), FRotator::ZeroRotator, Params);
		Ball->PreInitializeComponents();
		Ball->DispatchBeginPlay();
		return Ball;
	};
	auto* Held = MakeBall();
	TestTrue(TEXT("Holding a ball before charge"), FHALBullChargeTestAccess::Acquire(A, Held));
	auto* Stale = MakeBall();
	FHALBullChargeTestAccess::SetHeldReference(A, Stale);
	ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary);
	TestFalse(TEXT("Failed held-ball transaction rolls back active effect"), UHALBullChargeComponent::IsActiveOn(A));
	TestFalse(TEXT("Failed held-ball transaction rolls back cooldown"), ASC->HasMatchingGameplayTag(HALGameplayTags::Cooldown_BullCharge));
	const auto Rollback = FHALRoomTestAccess::Flush(A->FindComponentByClass<UHALMovementEffectBridge>());
	TestFalse(TEXT("Failed held-ball transaction publishes no charge or recovery"), Rollback.Overdrive.Activation.IsValid());
	FHALBullChargeTestAccess::SetHeldReference(A, Held);
	const FVector BeforeShot = CastChecked<UPrimitiveComponent>(A->GetRootComponent())->GetPhysicsLinearVelocity();
	TestTrue(TEXT("Primary activates through normal GAS"), ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary));
	TestTrue(TEXT("Held ball becomes a launch owned by the truck"), Held->GetBallState() == EBasicBallState::Launched && Held->GetLaunchedBy() == A);
	TestTrue(TEXT("Skill shot adds no recoil"), CastChecked<UPrimitiveComponent>(A->GetRootComponent())->GetPhysicsLinearVelocity().Equals(BeforeShot));
	TestFalse(TEXT("Repeat request cannot refresh"), ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary));
	TestTrue(TEXT("Impact immunity is granted"), ASC->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune));
	FHALBullChargeTestAccess::Pose(A);
	auto* Free = MakeBall();
	FHitResult BallHit; BallHit.ImpactPoint = Free->GetActorLocation(); BallHit.ImpactNormal = -FVector::ForwardVector;
	FHALBullChargeTestAccess::Contact(A, Free->GetPhysicsRoot(), FVector(3000, 0, 0));
	TestTrue(TEXT("Frontal free ball is launched through shared commit"), A->FindComponentByClass<UHALBullChargeComponent>()->HandleFreeBall(*Free, BallHit));
	TestTrue(TEXT("Ball receives launcher attribution"), Free->GetLaunchedBy() == A && Free->GetBallState() == EBasicBallState::Launched);
	TestFalse(TEXT("Already launched ball cannot be stolen"), A->FindComponentByClass<UHALBullChargeComponent>()->HandleFreeBall(*Free, BallHit));
	TestEqual(TEXT("A ball launch does not emit the vehicle-impact burst"), FHALBullChargeTestAccess::FXCount(A), uint32(0));
	for (const bool bSampleFirst : {false, true})
	{
		auto* Deferred = MakeBall();
		FHitResult DeferredHit; DeferredHit.ImpactPoint = Deferred->GetActorLocation(); DeferredHit.ImpactNormal = -FVector::ForwardVector;
		auto* Receiver = A->FindComponentByClass<UHALImpactReceiverComponent>();
		auto* VehicleBody = CastChecked<UPrimitiveComponent>(A->GetRootComponent());
		if (bSampleFirst) { FHALBullChargeTestAccess::Contact(A, Deferred->GetPhysicsRoot(), FVector(3000, 0, 0)); }
		Receiver->HandleContact(VehicleBody, Deferred, Deferred->GetPhysicsRoot(), FVector(100, 0, 0), DeferredHit);
		Receiver->HandleContact(VehicleBody, Deferred, Deferred->GetPhysicsRoot(), FVector(100, 0, 0), DeferredHit);
		TestTrue(TEXT("Engine callbacks reserve a free ball until the collision batch is complete"), Deferred->GetBallState() == EBasicBallState::Free);
		if (!bSampleFirst) { FHALBullChargeTestAccess::Contact(A, Deferred->GetPhysicsRoot(), FVector(3000, 0, 0)); }
		FHALBullChargeTestAccess::FlushContacts(A);
		TestTrue(TEXT("Both event-handler orders launch once through the authority contact adapter"), Deferred->GetBallState() == EBasicBallState::Launched && Deferred->GetLaunchedBy() == A);
		const uint32 Sequence = Deferred->GetBallStateSequence();
		FHALBullChargeTestAccess::FlushContacts(A);
		TestEqual(TEXT("Repeated flush cannot launch the same contact twice"), Deferred->GetBallStateSequence(), Sequence);
	}
	auto* RootA = CastChecked<UPrimitiveComponent>(A->GetRootComponent());
	auto* RootB = CastChecked<UPrimitiveComponent>(B->GetRootComponent());
	FHitResult Hit; Hit.ImpactPoint = A->GetActorLocation() + FVector(200, 0, 0); Hit.ImpactNormal = -FVector::ForwardVector;
	FHALBullChargeTestAccess::Contact(A, RootB, FVector(3000, 0, 0));
	const float InitialHP = TargetASC->GetSet<UHALHealthAttributeSet>()->GetCurrentHP();
	TestTrue(TEXT("Pre-solve frontal hit accepted even when GT body is stopped"), UHALBullChargeComponent::HandleVehiclePair(*RootA, *RootB, FVector::ZeroVector, Hit));
	const float FirstHP = TargetASC->GetSet<UHALHealthAttributeSet>()->GetCurrentHP();
	TestTrue(TEXT("Hit removes HP through resolver/GAS"), FirstHP < InitialHP);
	TestEqual(TEXT("One accepted vehicle contact emits one cosmetic event"), FHALBullChargeTestAccess::FXCount(A), uint32(1));
	UHALBullChargeComponent::HandleVehiclePair(*RootA, *RootB, FVector::ZeroVector, Hit);
	TestEqual(TEXT("Duplicate callback is inside target cooldown"), TargetASC->GetSet<UHALHealthAttributeSet>()->GetCurrentHP(), FirstHP);
	TestEqual(TEXT("Target cooldown also prevents repeated impact flashes"), FHALBullChargeTestAccess::FXCount(A), uint32(1));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FAdvanceChargeWorld>(Lifetime, Config->TargetCooldownSeconds + .05f,
		[this, Lifetime, A, B, ASC, TargetASC, RootA, RootB, Hit, FirstHP]()
		{
			FHALBullChargeTestAccess::Pose(A, 70);
			FHALBullChargeTestAccess::Contact(A, RootB, FVector(3000, 0, 0));
			TestTrue(TEXT("Same target can be struck after internal cooldown"), UHALBullChargeComponent::HandleVehiclePair(*RootA, *RootB, FVector::ZeroVector, Hit));
			TestTrue(TEXT("Second accepted hit removes HP"), TargetASC->GetSet<UHALHealthAttributeSet>()->GetCurrentHP() < FirstHP);
			TestEqual(TEXT("New accepted hit produces a second cosmetic event"), FHALBullChargeTestAccess::FXCount(A), uint32(2));
			TestTrue(TEXT("Stun accepted despite impact immunity"), ASC->ApplyStun(.2f, B));
			TestTrue(TEXT("Stun does not end charge"), UHALBullChargeComponent::IsActiveOn(A));
			FHALBullChargeTestAccess::Pose(A, 75);
			TestTrue(TEXT("Stun reaches replayable motion"), A->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetMovementEffectStatus().Definition.Overdrive.bStunned);
			TestFalse(TEXT("Stun pauses propulsion presentation while retaining the skill"), FHALBullChargeTestAccess::FXDriving(A));
			FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FAdvanceChargeWorld>(Lifetime, .3f, [this, Lifetime, A, ASC]()
			{
				TestTrue(TEXT("Charge survives stun expiry"), !ASC->IsStunned() && UHALBullChargeComponent::IsActiveOn(A));
				A->FindComponentByClass<UHALBullChargeComponent>()->Stop();
				TestFalse(TEXT("Cleanup removes only charge immunity"), ASC->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune));
				TestTrue(TEXT("Avatar cleanup retains cooldown"), ASC->HasMatchingGameplayTag(HALGameplayTags::Cooldown_BullCharge));
			}));
		}));
	return true;
}
#endif
