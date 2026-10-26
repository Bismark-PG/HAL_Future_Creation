#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALVehiclePresentationTestSupport.h"
#include "HALVehicleStabilityTestRules.h"
#include "../HALPlayerState.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALImpactAttributeSet.h"
#include "../HALHealthAttributeSet.h"
#include "../HALImpactReceiverComponent.h"
#include "../HALGameplayTags.h"
#include "../CombatResolver.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include <limits>

namespace
{
	FHALImpactVisualEvent Event(uint32 Sequence, EHALImpactTier Tier = EHALImpactTier::Heavy, FVector Direction = FVector::ForwardVector)
	{ FHALImpactVisualEvent Out; Out.Sequence = Sequence; Out.Tier = Tier; Out.LocalDirection = Direction; return Out; }
	struct FImpactVisualWorld
	{
		FScopedHALStabilityTestRules Stability { true };
		UWorld* World;
		ATestVehiclePawn* Pawn;
		UHALVehiclePresentationComponent* Presentation;
		FImpactVisualWorld()
		{
			const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
				.ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			auto* Class = LoadClass<ATestVehiclePawn>(nullptr, TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
			Pawn = World->SpawnActor<ATestVehiclePawn>(Class, FVector(0,0,1000), FRotator::ZeroRotator);
			Pawn->PreInitializeComponents(); Pawn->InitializeComponents(); Pawn->PostInitializeComponents();
			auto* PC = World->SpawnActor<APlayerController>(); PC->SetAsLocalPlayerController();
			auto* PS = World->SpawnActor<AHALPlayerState>(); PS->SetOwner(PC); PC->SetPlayerState(PS);
			PS->PreInitializeComponents(); PS->InitializeComponents(); PS->PostInitializeComponents(); PS->DispatchBeginPlay(); PC->Possess(Pawn);
			Pawn->DispatchBeginPlay();
			Presentation = FHALPresentationTestAccess::EnableImpactProbe(Pawn);
		}
		~FImpactVisualWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
		UPrimitiveComponent* Body() const { return FHALPresentationTestAccess::Body(Pawn); }
		UHALAbilitySystemComponent* ASC() const { return CastChecked<UHALAbilitySystemComponent>(Pawn->GetAbilitySystemComponent()); }
		FHALCombatHitResult Hit(EHALImpactTier Tier, bool bDamage = false)
		{
			auto* Source = World->SpawnActor<AActor>();
			FHALAttackSourceSnapshot Snapshot; Snapshot.SourceActor = Source; Snapshot.FixedDamage = bDamage ? 25 : 0;
			FHALCombatAction Action; Action.Begin(*Source, Snapshot);
			FHALCombatHitRequest Request; Request.AttackId = Action.GetSnapshot().AttackId;
			Request.bApplyDamage = bDamage; Request.bApplyImpact = true;
			Request.Hit.TargetActor = Pawn; Request.Hit.TargetPhysicsBody = Body(); Request.Hit.ImpactNormal = -FVector::ForwardVector;
			Request.Impact.Mode = EHALImpactMode::ExplicitTier; Request.Impact.Tier = Tier;
			return FCombatResolver::ResolveCombatHit(Action, Request);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALImpactVisualMathTest, "HAL.FutureCreation.Presentation.Impact.TiersDirectionsAndFrameRates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALImpactVisualMathTest::RunTest(const FString&)
{
	using namespace HALVehiclePresentation;
	FHALImpactReboundConfig Config; Config.bEnabled = true;
	FVector Local;
	TestTrue(TEXT("Capture direction in target-local space"), ImpactLocalDirection(FVector::RightVector, FRotator(0,90,0).Quaternion(), .1f, Local) && Local.Equals(FVector::ForwardVector, .001));
	TestFalse(TEXT("Vertical impulse invents no horizontal animation"), ImpactLocalDirection(FVector::UpVector, FQuat::Identity, .1f, Local));
	TestFalse(TEXT("Near vertical direction is rejected"), ImpactLocalDirection(FVector(.001,0,1), FQuat::Identity, .1f, Local));
	TestFalse(TEXT("Invalid rotation fails closed"), ImpactLocalDirection(FVector::ForwardVector, FQuat(0,0,0,0), .1f, Local));
	for (const auto Tier : {EHALImpactTier::Light, EHALImpactTier::Medium, EHALImpactTier::Heavy})
	{
		const auto& Settings = *Config.GetTier(Tier);
		TestEqual(TEXT("Rise reaches exact peak"), ImpactEnvelope(Settings, Settings.RiseSeconds), 1.f);
		TestEqual(TEXT("Return reaches exact rest"), ImpactEnvelope(Settings, Settings.RiseSeconds + Settings.ReturnSeconds), 0.f);
		for (int32 FPS : {30,60,120})
		{
			FHALImpactVisualState State;
			const FVector Direction = FVector(-1,1,0).GetSafeNormal();
			TestTrue(TEXT("Each effective tier starts"), AcceptImpact(Config, Event(1,Tier,Direction), 0, true, State));
			for (int32 Index = 1; Index <= FPS; ++Index)
			{
				const auto Output = AdvanceImpact(Config, 1.f/FPS, State);
				const float Envelope = ImpactEnvelope(Settings, float(Index)/FPS);
				TestEqual(TEXT("Pitch envelope independent of time step"), float(Output.Degrees.X), float(Direction.X)*Settings.PitchDegrees*Envelope, .001f);
				TestEqual(TEXT("Side direction and component magnitude preserved"), float(Output.Degrees.Y), float(Direction.Y)*Settings.RollDegrees*Envelope, .001f);
			}
			TestTrue(TEXT("No persistent angle after pulse"), State.Pulses.IsEmpty());
		}
	}
	FHALChassisPresentationConfig Chassis;
	FHALChassisPresentationState Driving; Driving.PitchDegrees = -30; Driving.RollDegrees = 30;
	FHALLaunchReboundConfig Launch;
	FHALImpactVisualOutput Impact; Impact.Degrees = FVector2D(3,-6); Impact.Weights = FVector2D(1,1);
	float Pitch = 0, Roll = 0;
	ComposeImpactRotation(Chassis, Driving, Launch, -8, Impact, Pitch, Roll);
	TestEqual(TEXT("Opposing driving and shot lean cannot cancel peak impact"), Pitch, 3.f);
	TestEqual(TEXT("Impact peak overrides opposing side lean"), Roll, -6.f);
	Impact.Degrees = FVector2D(100,-100);
	ComposeImpactRotation(Chassis, Driving, Launch, 8, Impact, Pitch, Roll);
	TestEqual(TEXT("Combined pitch stays bounded"), Pitch, 30.f); TestEqual(TEXT("Combined roll stays bounded"), Roll, -30.f);
	TestEqual(TEXT("Driving state is immutable"), Driving.PitchDegrees, -30.f);
	FHALImpactVisualState Diagonal;
	AcceptImpact(Config, Event(1,EHALImpactTier::Light,FVector(-1,1,0).GetSafeNormal()), 0, true, Diagonal);
	const auto DiagonalPeak = AdvanceImpact(Config, .04f, Diagonal);
	Driving.PitchDegrees = 30; Driving.RollDegrees = -30;
	ComposeImpactRotation(Chassis, Driving, Launch, 8, DiagonalPeak, Pitch, Roll);
	TestEqual(TEXT("Diagonal pitch remains readable against opposite driving"), Pitch, float(-3/FMath::Sqrt(2.)), .001f);
	TestEqual(TEXT("Diagonal roll remains readable against opposite driving"), Roll, float(3/FMath::Sqrt(2.)), .001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALImpactVisualIdentityTest, "HAL.FutureCreation.Presentation.Impact.IdentityPoseAndConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALImpactVisualIdentityTest::RunTest(const FString&)
{
	using namespace HALVehiclePresentation;
	FHALImpactReboundConfig Config;
	FString Error;
	TestFalse(TEXT("Old assets opt out"), Config.bEnabled);
	TestTrue(TEXT("Default impact settings valid"), Config.Validate(Error));
	Config.Heavy.PitchDegrees = 31; TestFalse(TEXT("Amplitude bounded"), Config.Validate(Error));
	Config = {}; Config.Medium.RiseSeconds = 0; TestFalse(TEXT("No zero envelope divisor"), Config.Validate(Error));
	Config = {}; Config.PitchSign = 0; TestFalse(TEXT("Direction sign cannot hide misconfiguration"), Config.Validate(Error));
	Config = {}; Config.MaximumEventAgeSeconds = std::numeric_limits<float>::infinity(); TestFalse(TEXT("Finite event age required"), Config.Validate(Error));
	Config = {}; Config.bEnabled = true;
	FHALImpactVisualState State;
	TestFalse(TEXT("None does not start"), AcceptImpact(Config, Event(1,EHALImpactTier::None), 0, true, State));
	TestTrue(TEXT("First received result starts once"), AcceptImpact(Config, Event(1), 0, true, State));
	TestFalse(TEXT("Repeated result does not restart"), AcceptImpact(Config, Event(1), 0, true, State));
	TestFalse(TEXT("Late event discarded"), AcceptImpact(Config, Event(2), .6f, true, State));
	TestFalse(TEXT("Seen late identity cannot restart with a changed age"), AcceptImpact(Config, Event(2), 0, true, State));
	TestFalse(TEXT("Blocked pose consumes identity without playback"), AcceptImpact(Config, Event(3), 0, false, State));
	TestFalse(TEXT("Becoming upright does not revive old blocked event"), AcceptImpact(Config, Event(3), 0, true, State));
	for (uint32 Sequence = 4; Sequence <= 30; ++Sequence) { AcceptImpact(Config, Event(Sequence), 0, true, State); }
	TestEqual(TEXT("Overlap storage bounded"), State.Pulses.Num(), 8);
	AdvanceImpact(Config, .04f, State); FadeImpact(Config, State);
	TestEqual(TEXT("Pose fade begins at current level"), float(AdvanceImpact(Config, 0, State).Degrees.X), 80.f, .001f);
	TestEqual(TEXT("Pose fade midpoint smooth"), float(AdvanceImpact(Config, .04f, State).Degrees.X), 40.f, .001f);
	TestTrue(TEXT("Pose fade clears pulses permanently"), AdvanceImpact(Config, .04f, State).Degrees.IsNearlyZero(.001) && State.Pulses.IsEmpty());
	State.LastSeenSequence = MAX_uint32-1;
	TestTrue(TEXT("Prewrap sequence accepted"), AcceptImpact(Config, Event(MAX_uint32), 0, true, State));
	TestTrue(TEXT("Postwrap sequence accepted"), AcceptImpact(Config, Event(1), 0, true, State));
	TestFalse(TEXT("Zero sequence rejected"), AcceptImpact(Config, Event(0), 0, true, State));
	Config.bEnabled = false; AdvanceImpact(Config, .02f, State);
	TestTrue(TEXT("Disable clears pulses but retains watermark"), State.Pulses.IsEmpty() && State.LastSeenSequence == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALImpactVisualAuthorityTest, "HAL.FutureCreation.Presentation.Impact.AuthorityResultAndPhysicsBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALImpactVisualAuthorityTest::RunTest(const FString&)
{
	FImpactVisualWorld F;
	const auto First = F.Hit(EHALImpactTier::Heavy);
	TestTrue(TEXT("Zero damage Heavy remains visible"), First.bHitAccepted && First.ActualHPRemoved == 0 && FHALPresentationTestAccess::ImpactState(F.Presentation).StartedCount == 1);
	F.ASC()->SetNumericAttributeBase(UHALImpactAttributeSet::GetTierReductionAttribute(), 2);
	const auto Reduced = F.Hit(EHALImpactTier::Heavy);
	TestEqual(TEXT("Reduced Heavy uses effective Light"), Reduced.ImpactSummary.EffectiveTier, EHALImpactTier::Light);
	TestEqual(TEXT("Reduced visual pulse uses Light"), FHALPresentationTestAccess::ImpactState(F.Presentation).Pulses.Last().Tier, EHALImpactTier::Light);
	F.ASC()->SetNumericAttributeBase(UHALImpactAttributeSet::GetTierReductionAttribute(), 0);
	F.ASC()->AddLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
	const auto Immune = F.Hit(EHALImpactTier::Heavy, true);
	TestTrue(TEXT("Impact immunity retains damage but no new animation"), Immune.ActualHPRemoved == 25 && FHALPresentationTestAccess::ImpactState(F.Presentation).StartedCount == 2);
	F.ASC()->RemoveLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
	F.ASC()->AddLooseGameplayTag(HALGameplayTags::State_Combat_DamageImmune);
	const auto DamageImmune = F.Hit(EHALImpactTier::Heavy, true);
	TestTrue(TEXT("Damage immunity still permits effective impact animation"), DamageImmune.ActualHPRemoved == 0 && FHALPresentationTestAccess::ImpactState(F.Presentation).StartedCount == 3);
	F.Hit(EHALImpactTier::None);
	TestEqual(TEXT("None adds no visual pulse"), FHALPresentationTestAccess::ImpactState(F.Presentation).StartedCount, 3u);
	const auto Transform = F.Body()->GetComponentTransform(); const auto Velocity = F.Body()->GetPhysicsLinearVelocity();
	const auto Angular = F.Body()->GetPhysicsAngularVelocityInRadians(); const float Mass = F.Body()->GetMass();
	const float HP = F.ASC()->GetSet<UHALHealthAttributeSet>()->GetCurrentHP(); const auto Collision = F.Body()->GetCollisionEnabled();
	F.Presentation->TickComponent(.04f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Existing Chassis output includes impact"), F.Presentation->GetPresentationFrame().ImpactPitchDegrees > 0);
	TestTrue(TEXT("Visual tick does not change physical transform or velocities"), F.Body()->GetComponentTransform().Equals(Transform) && F.Body()->GetPhysicsLinearVelocity().Equals(Velocity) && F.Body()->GetPhysicsAngularVelocityInRadians().Equals(Angular));
	TestTrue(TEXT("Visual tick does not change mass, collision or HP"), F.Body()->GetMass() == Mass && F.Body()->GetCollisionEnabled() == Collision && F.ASC()->GetSet<UHALHealthAttributeSet>()->GetCurrentHP() == HP);
	FHALPresentationTestAccess::SetRecoveryExecuting(F.Pawn, true);
	F.Presentation->TickComponent(.04f, LEVELTICK_All, nullptr); F.Presentation->TickComponent(.04f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Self-righting fades old impact poses"), F.Presentation->GetPresentationFrame().ImpactPitchDegrees == 0);
	const uint32 Starts = FHALPresentationTestAccess::ImpactState(F.Presentation).StartedCount;
	F.Hit(EHALImpactTier::Light);
	TestEqual(TEXT("Self-righting rejects new visual playback"), FHALPresentationTestAccess::ImpactState(F.Presentation).StartedCount, Starts);
	FHALPresentationTestAccess::SetRecoveryExecuting(F.Pawn, false);
	F.Hit(EHALImpactTier::Light); F.Presentation->TickComponent(.5f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Long gap clears impact channel"), F.Presentation->GetPresentationFrame().ImpactPitchDegrees == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALImpactVisualContactTest, "HAL.FutureCreation.Presentation.Impact.SupportNaturalZeroAndContactEpisodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALImpactVisualContactTest::RunTest(const FString&)
{
	FImpactVisualWorld F;
	auto* Source = F.World->SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Source); Source->AddInstanceComponent(Box); Source->SetRootComponent(Box);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Box->SetCollisionObjectType(ECC_WorldStatic); Box->RegisterComponent();
	auto Send = [&](FVector Direction, FVector Normal)
	{
		FHALAttackSourceSnapshot Snapshot; Snapshot.SourceActor = Source;
		FHALCombatAction Action; Action.Begin(*Source, Snapshot);
		FHALCombatHitRequest Request; Request.AttackId = Action.GetSnapshot().AttackId;
		Request.bApplyDamage = false; Request.bApplyImpact = true; Request.Impact.Mode = EHALImpactMode::PhysicalContact; Request.Impact.Tier = EHALImpactTier::Medium;
		Request.Hit.TargetActor = F.Pawn; Request.Hit.TargetPhysicsBody = F.Body(); Request.Hit.SourceActor = Source; Request.Hit.SourcePhysicsBody = Box;
		Request.Hit.ImpactNormal = Normal; Request.Hit.NormalImpulse = Direction * F.Body()->GetMass() * 1000;
		const auto Result = FCombatResolver::ResolveCombatHit(Action, Request);
		TestEqual(TEXT("Duplicate action cannot publish twice"), FCombatResolver::ResolveCombatHit(Action, Request).RejectReason, EHALHitRejectReason::Duplicate);
		return Result;
	};
	const auto Support = Send(FVector(.2,0,1).GetSafeNormal(), FVector::UpVector);
	TestTrue(TEXT("Real support result suppressed despite retained effective tier"), Support.bSupportSupplementSuppressed && Support.ImpactSummary.EffectiveTier == EHALImpactTier::Medium);
	TestEqual(TEXT("Suppressed support does not animate"), FHALPresentationTestAccess::ImpactState(F.Presentation).StartedCount, 0u);
	const auto Wall = Send(FVector::ForwardVector, -FVector::ForwardVector);
	TestTrue(TEXT("Natural response already sufficient still has a valid result"), Wall.bImpactApplied && Wall.RuleDeltaVelocity.IsNearlyZero() && !Wall.bSupportSupplementSuppressed);
	TestEqual(TEXT("Zero additional rule impulse still animates real side hit"), FHALPresentationTestAccess::ImpactState(F.Presentation).StartedCount, 1u);
	// A distinct body has not been suppressed by the explicit requests above.
	auto* Other = F.World->SpawnActor<AActor>(); auto* OtherBox = NewObject<UBoxComponent>(Other);
	Other->AddInstanceComponent(OtherBox); Other->SetRootComponent(OtherBox);
	OtherBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); OtherBox->SetCollisionObjectType(ECC_WorldStatic); OtherBox->RegisterComponent();
	auto* Receiver = F.Pawn->FindComponentByClass<UHALImpactReceiverComponent>();
	FHitResult Hit; Hit.ImpactNormal = -FVector::ForwardVector;
	Receiver->HandleContact(F.Body(), Other, OtherBox, FVector::ForwardVector * 1000 * F.Body()->GetMass(), Hit);
	const uint32 Starts = FHALPresentationTestAccess::ImpactState(F.Presentation).StartedCount;
	Receiver->HandleContact(F.Body(), Other, OtherBox, FVector::ForwardVector * 1000 * F.Body()->GetMass(), Hit);
	TestTrue(TEXT("New contact starts once, continuous contact does not restart"), Starts == 2 && FHALPresentationTestAccess::ImpactState(F.Presentation).StartedCount == Starts);
	return true;
}
#endif
