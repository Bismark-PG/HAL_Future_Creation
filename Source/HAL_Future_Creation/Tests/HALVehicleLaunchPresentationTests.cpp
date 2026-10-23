#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "HALVehiclePresentationTestSupport.h"
#include "../HALPlayerState.h"
#include "../HALDebugSettings.h"
#include "../VehicleHealthComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALLaunchVisualMathTest, "HAL.FutureCreation.Presentation.Launch.EnvelopeAndConfirmation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALLaunchVisualMathTest::RunTest(const FString&)
{
	using namespace HALVehiclePresentation;
	FHALLaunchReboundConfig Config; Config.bEnabled = true;
	TestEqual(TEXT("Envelope begins at neutral"), LaunchEnvelope(Config, 0), 0.f);
	TestEqual(TEXT("Configured rise reaches exact peak"), LaunchEnvelope(Config, Config.RiseSeconds), 1.f);
	TestEqual(TEXT("Configured return reaches exact rest"), LaunchEnvelope(Config, Config.RiseSeconds + Config.ReturnSeconds), 0.f);
	TestEqual(TEXT("Return midpoint remains visible"), LaunchEnvelope(Config, Config.RiseSeconds + Config.ReturnSeconds / 2), .5f, .0001f);
	for (int32 FPS : {30,60,120})
	{
		FHALLaunchVisualState State;
		TestTrue(TEXT("New local prediction begins once"), PredictLaunch(Config, 1, State));
		for (int32 Index = 1; Index <= FPS; ++Index)
		{
			const float Pitch = AdvanceLaunch(Config, 1.f / FPS, State);
			TestEqual(TEXT("Analytic envelope has no integration-dependent response"), Pitch, 8 * LaunchEnvelope(Config, float(Index) / FPS), .001f);
		}
		TestEqual(TEXT("Pulse retires after return"), State.Pulses.Num(), 0);
		TestFalse(TEXT("Expired prediction cannot replay on late confirmation"), AcceptLaunch(Config, 1, 0, State));
	}
	FHALLaunchVisualState State;
	PredictLaunch(Config, 7, State); AdvanceLaunch(Config, .04f, State);
	const float Age = State.Pulses[0].Age;
	TestTrue(TEXT("Prediction gets one confirmation"), AcceptLaunch(Config, 7, .01f, State));
	TestEqual(TEXT("Confirmation never restarts prediction age"), State.Pulses[0].Age, Age);
	TestFalse(TEXT("Reliable result and multicast do not double confirm"), AcceptLaunch(Config, 7, .01f, State));
	TestEqual(TEXT("Only one visible start"), State.StartedCount, 1u);
	TestTrue(TEXT("Other new shot begins"), PredictLaunch(Config, 8, State));
	TestFalse(TEXT("Out-of-order unknown event cannot restart an old shot"), AcceptLaunch(Config, 6, .02f, State));
	TestTrue(TEXT("Accepted remote shot starts at authoritative age"), AcceptLaunch(Config, 9, .1f, State));
	TestEqual(TEXT("Remote age compensation is retained"), State.Pulses.Last().Age, .1f);
	TestFalse(TEXT("Too old event ignored"), AcceptLaunch(Config, 10, .6f, State));
	TestFalse(TEXT("Zero sequence is invalid"), AcceptLaunch(Config, 0, 0, State));
	TestFalse(TEXT("Nonfinite age is invalid"), AcceptLaunch(Config, 10, std::numeric_limits<float>::quiet_NaN(), State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALLaunchVisualRejectTest, "HAL.FutureCreation.Presentation.Launch.RejectionBoundsAndComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALLaunchVisualRejectTest::RunTest(const FString&)
{
	using namespace HALVehiclePresentation;
	FHALLaunchReboundConfig Config; Config.bEnabled = true;
	FHALLaunchVisualState State;
	PredictLaunch(Config, 1, State); AdvanceLaunch(Config, Config.RiseSeconds, State);
	TestTrue(TEXT("Pending prediction accepts rejection"), RejectLaunch(Config, 1, State));
	TestEqual(TEXT("Rejection begins from current offset, without snapping"), AdvanceLaunch(Config, 0, State), 8.f);
	TestEqual(TEXT("Rejection midpoint fades smoothly"), AdvanceLaunch(Config, Config.RejectionFadeSeconds / 2, State), 4.f, .0001f);
	TestEqual(TEXT("Rejection returns fully to neutral"), AdvanceLaunch(Config, Config.RejectionFadeSeconds / 2, State), 0.f, .0001f);
	TestFalse(TEXT("Rejected shot cannot be replayed by duplicate acceptance"), AcceptLaunch(Config, 1, 0, State));
	TestEqual(TEXT("Only one rejection counted"), State.RejectedCount, 1u);
	for (uint32 Sequence = 2; Sequence <= 30; ++Sequence) { PredictLaunch(Config, Sequence, State); }
	TestEqual(TEXT("Overlapping shot storage is bounded"), State.Pulses.Num(), 8);
	const float LaunchPitch = AdvanceLaunch(Config, Config.RiseSeconds, State);
	FHALChassisPresentationConfig Chassis;
	FHALChassisPresentationState Driving; Driving.PitchDegrees = 20; Driving.RollDegrees = -40;
	float CombinedPitch = 0;
	const auto Rotation = ComposeChassisRotation(Chassis, Driving, Config, LaunchPitch, CombinedPitch);
	TestEqual(TEXT("Combined driving/shot pitch is bounded"), CombinedPitch, 30.f);
	FHALChassisPresentationState Expected; Expected.PitchDegrees = 30; Expected.RollDegrees = -30;
	TestTrue(TEXT("Composition uses calibrated axes and both final limits"), Rotation.Equals(ChassisRotation(Chassis, Expected), .0001));
	TestEqual(TEXT("Composition does not overwrite the driving filter"), Driving.PitchDegrees, 20.f);
	Driving.PitchDegrees = -30;
	ComposeChassisRotation(Chassis, Driving, Config, 8, CombinedPitch);
	TestEqual(TEXT("Shot peak is not cancelled by opposite braking lean"), CombinedPitch, 8.f);
	ComposeChassisRotation(Chassis, Driving, Config, 0, CombinedPitch);
	TestEqual(TEXT("Driving lean returns unchanged at shot rest"), CombinedPitch, -30.f);
	State = {}; State.LastSeenSequence = MAX_uint32 - 1;
	TestTrue(TEXT("Pre-wrap sequence starts"), PredictLaunch(Config, MAX_uint32, State));
	TestTrue(TEXT("Post-wrap sequence starts"), PredictLaunch(Config, 1, State));
	Config.bEnabled = false;
	TestEqual(TEXT("Disable clears active shot channel"), AdvanceLaunch(Config, .02f, State), 0.f);
	TestEqual(TEXT("Disable retains sequence watermark"), State.LastSeenSequence, 1u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALLaunchVisualConfigTest, "HAL.FutureCreation.Presentation.Launch.ConfigurationCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALLaunchVisualConfigTest::RunTest(const FString&)
{
	auto* Config = NewObject<UHALVehiclePresentationConfig>();
	for (auto& Wheel : Config->Wheels) { Wheel.RadiusCm = 30; }
	FString Error;
	TestFalse(TEXT("Old assets do not opt into shot visuals"), Config->LaunchRebound.bEnabled);
	TestTrue(TEXT("Valid old settings remain valid"), Config->ValidateSettings(Error));
	Config->LaunchRebound.RiseSeconds = 0;
	TestFalse(TEXT("Zero rise cannot divide in the envelope"), Config->ValidateSettings(Error));
	Config->LaunchRebound = {}; Config->LaunchRebound.PitchDegrees = -12;
	TestTrue(TEXT("Model-specific opposite pitch is configurable"), Config->ValidateSettings(Error));
	Config->LaunchRebound.PitchDegrees = 31;
	TestFalse(TEXT("Unbounded amplitude rejected"), Config->ValidateSettings(Error));
	Config->LaunchRebound = {}; Config->Chassis.TotalPitchLimitDegrees = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("Nonfinite composition limit rejected"), Config->ValidateSettings(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALLaunchVisualComponentTest, "HAL.FutureCreation.Presentation.Launch.AuthorityPathAndPhysicsBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALLaunchVisualComponentTest::RunTest(const FString&)
{
	const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
		.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
		.ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	auto* Class = LoadClass<ATestVehiclePawn>(nullptr, TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
	auto* Pawn = Class ? World->SpawnActor<ATestVehiclePawn>(Class, FVector(0,0,1000), FRotator::ZeroRotator) : nullptr;
	if (!TestNotNull(TEXT("Saved vehicle"), Pawn)) { return false; }
	Pawn->PreInitializeComponents(); Pawn->InitializeComponents(); Pawn->PostInitializeComponents();
	auto* PC = World->SpawnActor<APlayerController>(); PC->SetAsLocalPlayerController();
	auto* PS = World->SpawnActor<AHALPlayerState>(); PS->SetOwner(PC); PC->SetPlayerState(PS);
	PS->PreInitializeComponents(); PS->InitializeComponents(); PS->PostInitializeComponents(); PS->DispatchBeginPlay(); PC->Possess(Pawn);
	Pawn->DispatchBeginPlay(); // Health initialization is part of the real launch snapshot prerequisite.
	if (!TestTrue(TEXT("Native Avatar health/combat lifecycle is ready"), Pawn->FindComponentByClass<UVehicleHealthComponent>()->IsHealthReady())) { return false; }
	auto* Component = FHALPresentationTestAccess::EnableLaunchProbe(Pawn);
	FHALPresentationTestAccess::StartLaunch(Pawn);
	TestEqual(TEXT("No ball cannot publish an accepted visual"), FHALPresentationTestAccess::LaunchState(Component).StartedCount, 0u);
	auto* BallClass = GetDefault<UHALDebugSettings>()->DefaultBallClass.LoadSynchronous();
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Ball = BallClass ? World->SpawnActor<ABasicBallActor>(BallClass, FVector(500,0,1000), FRotator::ZeroRotator, Spawn) : nullptr;
	if (!TestNotNull(TEXT("Saved ball"), Ball)) { return false; }
	Ball->PreInitializeComponents(); Ball->InitializeComponents(); Ball->PostInitializeComponents();
	if (!TestTrue(TEXT("Fixture obtains native controlled state"), FHALPresentationTestAccess::GiveBall(Pawn, Ball))) { return false; }
	FHALPresentationTestAccess::StartLaunch(Pawn);
	TestEqual(TEXT("Actual successful ball launch publishes one visual"), FHALPresentationTestAccess::LaunchState(Component).StartedCount, 1u);
	TestEqual(TEXT("Ball entered launched state"), Ball->GetBallState(), EBasicBallState::Launched);
	auto* Body = FHALPresentationTestAccess::Body(Pawn);
	const auto Transform = Body->GetComponentTransform();
	const auto Velocity = Body->GetPhysicsLinearVelocity(), Angular = Body->GetPhysicsAngularVelocityInRadians();
	const float Mass = Body->GetMass(); const auto Collision = Body->GetCollisionEnabled();
	for (int32 Index = 0; Index < 5; ++Index) { Component->TickComponent(.01f, LEVELTICK_All, nullptr); }
	const auto Frame = Component->GetPresentationFrame();
	TestEqual(TEXT("Dedicated shot channel reaches configured peak without driving lean"), Frame.LaunchPitchDegrees, 8.f, .001f);
	TestTrue(TEXT("Final chassis output uses the existing rotation path"), !Frame.ChassisRotation.IsNearlyZero());
	TestTrue(TEXT("Animation never adds a second physical recoil"), Body->GetComponentTransform().Equals(Transform) && Body->GetPhysicsLinearVelocity().Equals(Velocity)
		&& Body->GetPhysicsAngularVelocityInRadians().Equals(Angular));
	TestEqual(TEXT("Mass unchanged"), Body->GetMass(), Mass); TestEqual(TEXT("Collision unchanged"), Body->GetCollisionEnabled(), Collision);
	Component->TickComponent(.5f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Long gap clears visual rebound"), Component->GetPresentationFrame().ChassisRotation.IsNearlyZero());
	Component->ReceiveLaunchResult(2, true, 0);
	TestEqual(TEXT("Delayed confirmation cannot resurrect cleared shot"), FHALPresentationTestAccess::LaunchState(Component).StartedCount, 1u);
	return true;
}
#endif
