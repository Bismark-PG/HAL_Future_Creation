#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALVehiclePresentationTestSupport.h"
#include "HALLegacyCombatTestRules.h"
#include "../HALVehicleAnimInstance.h"
#include "../HALPlayerState.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "ReferenceSkeleton.h"
#include <limits>

namespace
{
	UHALVehiclePresentationConfig* MeasuredConfig()
	{
		auto* Config = NewObject<UHALVehiclePresentationConfig>();
		for (auto& Wheel : Config->Wheels) { Wheel.RadiusCm = 30; }
		return Config;
	}
	FReferenceSkeleton MakeRig(const UHALVehiclePresentationConfig& Config, bool bWrongWheelParent = false)
	{
		FReferenceSkeleton Rig;
		{
		FReferenceSkeletonModifier Modifier(Rig, nullptr);
		Modifier.Add(FMeshBoneInfo(Config.RootBone, Config.RootBone.ToString(), INDEX_NONE), FTransform::Identity);
		Modifier.Add(FMeshBoneInfo(Config.ChassisBone, Config.ChassisBone.ToString(), 0), FTransform::Identity);
		for (const auto& Wheel : Config.Wheels)
		{
			const int32 Parent = Rig.GetRawBoneNum();
			Modifier.Add(FMeshBoneInfo(Wheel.SuspensionBone, Wheel.SuspensionBone.ToString(), bWrongWheelParent ? 1 : 0), FTransform::Identity);
			Modifier.Add(FMeshBoneInfo(Wheel.SteeringBone, Wheel.SteeringBone.ToString(), Parent), FTransform::Identity);
			Modifier.Add(FMeshBoneInfo(Wheel.SpinBone, Wheel.SpinBone.ToString(), Parent + 1), FTransform::Identity);
		}
		} // Modifier rebuilds the final bone maps before returning the fixture.
		return Rig;
	}
	struct FPresentationWorld
	{
		FScopedHALLegacyCombatTestRules Rules;
		UWorld* World;
		ATestVehiclePawn* Pawn;
		bool bNoRigFixtureCreated = false;
		FPresentationWorld(bool bWithoutRig = false)
		{
			const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
				.ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			auto* Class = LoadClass<ATestVehiclePawn>(nullptr, TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
			Pawn = Class ? World->SpawnActor<ATestVehiclePawn>(Class, FVector(0,0,1000), FRotator::ZeroRotator) : nullptr;
			if (bWithoutRig) { bNoRigFixtureCreated = FHALPresentationTestAccess::SelectNoRigFixture(Pawn); }
			if (Pawn) { Pawn->PreInitializeComponents(); Pawn->InitializeComponents(); Pawn->PostInitializeComponents(); }
		}
		~FPresentationWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALPresentationMathTest, "HAL.FutureCreation.Presentation.WheelMathAndFrameRates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALPresentationMathTest::RunTest(const FString& Parameters)
{
	using namespace HALVehiclePresentation;
	TestEqual(TEXT("cm/s and radius produce degrees"), AdvanceWheel(0, 30 * PI, 30, 1, 0.5f), 90.f, 0.001f);
	TestEqual(TEXT("Reversing actual movement reverses wheel"), AdvanceWheel(0, -30 * PI, 30, 1, 0.5f), -90.f, 0.001f);
	TestEqual(TEXT("Mirrored local axle reverses sign"), AdvanceWheel(0, 30 * PI, 30, -1, 0.5f), -90.f, 0.001f);
	TestEqual(TEXT("Wrapping retains incremental angle"), AdvanceWheel(175, 30 * PI, 30, 1, 0.1f), -167.f, 0.001f);
	TestEqual(TEXT("Invalid radius cannot divide"), AdvanceWheel(0, 100, 0, 1, 0.1f), 0.f);
	TestEqual(TEXT("Nonfinite input cannot poison graph"), AdvanceWheel(0, std::numeric_limits<float>::infinity(), 30, 1, 0.1f), 0.f);
	for (const int32 FPS : {30, 60, 120})
	{
		float Angle = 0, Steer = 0;
		for (int32 Index = 0; Index < FPS; ++Index)
		{
			Angle = AdvanceWheel(Angle, 30 * PI / 2, 30, 1, 1.f / FPS);
			Steer = SmoothSteering(Steer, 25, 0.12f, 1.f / FPS);
		}
		TestEqual(TEXT("Same physical interval at each FPS"), Angle, 90.f, 0.002f);
		TestEqual(TEXT("Exponential steering is frame-rate independent"), Steer, 25.f * (1.f - FMath::Exp(-1.f / 0.12f)), 0.002f);
	}
	TestTrue(TEXT("Y axle rotates forward toward down"), AxisRotation(EHALBoneAxis::Y, 90).RotateVector(FVector::ForwardVector).Equals(-FVector::UpVector, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALPresentationConfigTest, "HAL.FutureCreation.Presentation.ConfigurationAndRigContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALPresentationConfigTest::RunTest(const FString& Parameters)
{
	FString Error;
	auto* Config = NewObject<UHALVehiclePresentationConfig>();
	TestFalse(TEXT("Unmeasured default cannot enter gameplay"), Config->Validate(Error));
	TestFalse(TEXT("Default radii require calibration"), Config->ValidateSettings(Error));
	Config = MeasuredConfig();
	TestTrue(TEXT("Measured settings are valid without importing assets"), Config->ValidateSettings(Error));
	const auto Rig = MakeRig(*Config);
	TestTrue(TEXT("Screenshot hierarchy is accepted"), Config->ValidateRig(Rig, Error));
	Config->Wheels.Swap(0, 3);
	TestTrue(TEXT("Explicit wheel slots ignore skeleton/array traversal order"), Config->ValidateRig(Rig, Error));
	Config->Wheels[0].Wheel = Config->Wheels[1].Wheel;
	TestFalse(TEXT("Duplicate wheel identities rejected"), Config->ValidateSettings(Error));
	Config = MeasuredConfig();
	TestFalse(TEXT("Wheels under Chassis would inherit unwanted body lean"), Config->ValidateRig(MakeRig(*Config, true), Error));
	Config->Wheels[0].RadiusCm = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN radius rejected at runtime"), Config->ValidateSettings(Error));
	Config->Wheels[0].RadiusCm = 30; Config->Wheels[0].SpinSign = 0;
	TestFalse(TEXT("Zero direction is not calibration"), Config->ValidateSettings(Error));
	Config->Wheels[0].SpinSign = 1; Config->Wheels[0].SpinBone = TEXT("MissingWheel");
	TestFalse(TEXT("Missing control bone rejected"), Config->ValidateRig(Rig, Error));
	Config = MeasuredConfig(); Config->Chassis.PitchAxis = Config->Chassis.RollAxis;
	TestFalse(TEXT("Chassis pitch and roll cannot share a calibrated axis"), Config->ValidateSettings(Error));
	Config = MeasuredConfig(); Config->Chassis.PitchSign = 0;
	TestFalse(TEXT("Chassis direction must be +/-1"), Config->ValidateSettings(Error));
	Config = MeasuredConfig(); Config->Chassis.ResponseSeconds = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Chassis response must be finite"), Config->ValidateSettings(Error));
	Config = MeasuredConfig(); Config->Chassis.FullPitchAcceleration = Config->Chassis.AccelerationDeadZone;
	TestFalse(TEXT("Chassis dead zone cannot consume its full acceleration range"), Config->ValidateSettings(Error));
	Config = MeasuredConfig(); Config->Chassis.MaxRollDegrees = 11;
	TestFalse(TEXT("Chassis visual amplitude is bounded"), Config->ValidateSettings(Error));
	Config = MeasuredConfig(); Config->bReadyForUse = true;
	TestFalse(TEXT("Ready cannot substitute for actual mesh and AnimClass"), Config->Validate(Error));
	Config->Mesh = NewObject<USkeletalMesh>();
	Config->Mesh->SetSkeleton(NewObject<USkeleton>());
	Config->Mesh->SetRefSkeleton(Rig);
	Config->AnimClass = UHALVehicleAnimInstance::StaticClass();
	TestTrue(TEXT("Transient native fixture validates the full data contract without saving assets"), Config->Validate(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALPresentationFallbackTest, "HAL.FutureCreation.Presentation.OptionalRigPreservesGameplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALPresentationFallbackTest::RunTest(const FString& Parameters)
{
	FPresentationWorld F(true);
	if (!TestNotNull(TEXT("Saved vehicle"), F.Pawn)) { return false; }
	if (!TestTrue(TEXT("No-rig case uses its own definition without editing the saved player"), F.bNoRigFixtureCreated)) { return false; }
	auto* Component = F.Pawn->FindComponentByClass<UHALVehiclePresentationComponent>();
	auto* Mesh = F.Pawn->FindComponentByClass<USkeletalMeshComponent>();
	auto* Body = FHALPresentationTestAccess::Body(F.Pawn);
	TestTrue(TEXT("Existing gameplay config still valid"), F.Pawn->IsConfigurationValid());
	TestFalse(TEXT("No optional asset leaves presentation tick disabled"), Component->IsComponentTickEnabled());
	TestFalse(TEXT("No rig selected"), Component->IsUsingSkeletalPresentation());
	TestFalse(TEXT("Inactive physics does not publish stale cosmetic input"), F.Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetPresentationInput().bValid);
	TestEqual(TEXT("Skeleton has no collision"), Mesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Skeleton is not a physical vehicle"), Mesh->IsSimulatingPhysics());
	const float Mass = Body->GetMass();
	const auto Transform = Body->GetComponentTransform();
	const bool bSimulates = Body->IsSimulatingPhysics();
	auto* Fallback = F.Pawn->FindComponentByClass<UStaticMeshComponent>();
	// Explicitly get the existing visible static mesh, rather than depending on component traversal order.
	for (auto* Candidate : TInlineComponentArray<UStaticMeshComponent*>(F.Pawn))
	{
		if (Candidate->GetFName() == TEXT("VisualMesh")) { Fallback = Candidate; break; }
	}
	AddExpectedError(TEXT("skeletal presentation fallback"), EAutomationExpectedErrorFlags::Contains, 1);
	Component->Configure(NewObject<UHALVehiclePresentationConfig>(), Body, Fallback, Mesh, false);
	TestFalse(TEXT("Unready model is not activated"), Component->IsUsingSkeletalPresentation());
	TestTrue(TEXT("Invalid cosmetic configuration does not disable driving"), F.Pawn->IsConfigurationValid());
	TestEqual(TEXT("Mass unchanged"), Body->GetMass(), Mass);
	TestEqual(TEXT("Simulation unchanged"), Body->IsSimulatingPhysics(), bSimulates);
	TestTrue(TEXT("Physics transform unchanged"), Body->GetComponentTransform().Equals(Transform));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALPresentationFrameTest, "HAL.FutureCreation.Presentation.SnapshotAndResetLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALPresentationFrameTest::RunTest(const FString& Parameters)
{
	FPresentationWorld F;
	if (!TestNotNull(TEXT("Saved vehicle"), F.Pawn)) { return false; }
	auto* Controller = F.World->SpawnActor<APlayerController>();
	Controller->SetAsLocalPlayerController();
	auto* State = F.World->SpawnActor<AHALPlayerState>();
	State->SetOwner(Controller); Controller->SetPlayerState(State);
	State->PreInitializeComponents(); State->InitializeComponents(); State->PostInitializeComponents(); State->DispatchBeginPlay();
	Controller->Possess(F.Pawn);
	auto* Component = FHALPresentationTestAccess::EnableProbe(F.Pawn);
	auto* Body = FHALPresentationTestAccess::Body(F.Pawn);
	FHALPresentationTestAccess::SetInput(F.Pawn, 0.6f);
	Body->SetPhysicsLinearVelocity(Body->GetForwardVector() * 300);
	Component->TickComponent(0.02f, LEVELTICK_All, nullptr);
	Component->TickComponent(0.02f, LEVELTICK_All, nullptr);
	const auto First = Component->GetPresentationFrame();
	TestTrue(TEXT("Headless native frame is valid"), First.bValid);
	TestTrue(TEXT("Local steering responds without an RTT"), First.SteeringDegrees > 0);
	TestTrue(TEXT("Actual velocity rolls wheels"), !First.BL.SpinRotation.IsNearlyZero());
	TestTrue(TEXT("Rear steer defaults to neutral"), First.BL.SteeringRotation.IsNearlyZero());
	auto* Anim = NewObject<UHALVehicleAnimInstance>(F.Pawn->FindComponentByClass<USkeletalMeshComponent>());
	Anim->NativeInitializeAnimation(); Anim->NativeUpdateAnimation(0.02f);
	TestTrue(TEXT("Native AnimInstance pulls frame"), Anim->Presentation.bValid);
	Component->TickComponent(0.02f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("AnimGraph copy does not mutate while component advances"), Anim->Presentation.BL.SpinRotation.Equals(First.BL.SpinRotation));
	Component->TickComponent(0.5f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Large gap resets instead of accumulating"), Component->GetPresentationFrame().bValid);
	Component->TickComponent(0.02f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("First resumed sample seeds neutral roll"), Component->GetPresentationFrame().BL.SpinRotation.IsNearlyZero());
	Body->SetWorldLocation(Body->GetComponentLocation() + FVector(2000,0,0), false, nullptr, ETeleportType::TeleportPhysics);
	Component->TickComponent(0.02f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Teleport invalidates old sample"), Component->GetPresentationFrame().bValid);
	F.Pawn->Destroy(); Anim->NativeUpdateAnimation(0.02f);
	TestFalse(TEXT("Destroyed Pawn cannot leave old animation data"), Anim->Presentation.bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALChassisMathTest, "HAL.FutureCreation.Presentation.ChassisAccelerationAndFrameRates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALChassisMathTest::RunTest(const FString& Parameters)
{
	using namespace HALVehiclePresentation;
	FHALChassisPresentationConfig Config;
	Config.bEnabled = true;
	for (const int32 FPS : {30, 60, 120})
	{
		FHALChassisPresentationState State;
		FHALChassisMotionSample Sample;
		Sample.bDrivingAllowed = true;
		UpdateChassis(Config, Sample, 1.f / FPS, State);
		for (int32 Index = 1; Index <= FPS; ++Index)
		{
			const double Time = double(Index) / FPS;
			Sample.Velocity = FVector(1800, -900, 0) * Time;
			Sample.Position = FVector(1800, -900, 0) * (.5 * Time * Time);
			UpdateChassis(Config, Sample, 1.f / FPS, State);
		}
		const float Factor = 1.f - FMath::Exp(-1.f / Config.ResponseSeconds);
		TestEqual(TEXT("Exponential acceleration filtering is stable at each FPS"), State.ForwardAcceleration, 1800 * Factor, .02f);
		TestTrue(TEXT("Forward acceleration produces rearward lean and lateral sign is preserved"), State.PitchDegrees > 3.9f && State.RollDegrees < -1.8f);
		TestTrue(TEXT("Mapped default pitch lifts the nose"), ChassisRotation(Config, State).RotateVector(FVector::ForwardVector).Z > 0);
		for (int32 Index = 1; Index <= FPS; ++Index)
		{
			Sample.Velocity -= FVector(1800, 0, 0) / FPS;
			Sample.Position += Sample.Velocity / FPS;
			UpdateChassis(Config, Sample, 1.f / FPS, State);
		}
		TestTrue(TEXT("Actual deceleration produces forward lean"), State.PitchDegrees < -3.9f);
		Sample.bDrivingAllowed = false;
		for (int32 Index = 0; Index < FPS; ++Index)
		{
			Sample.Position += Sample.Velocity / FPS;
			UpdateChassis(Config, Sample, 1.f / FPS, State);
		}
		TestTrue(TEXT("Airborne/inverted/recovery gate fades to neutral"), ChassisRotation(Config, State).IsNearlyZero(.02f));
		// A 60 Hz body sampled at 120 Hz must not lose half its lean to pre-filter saturation.
		State = {}; Sample = {}; Sample.bDrivingAllowed = true;
		UpdateChassis(Config, Sample, 1.f / FPS, State);
		for (int32 Index = 1; Index <= 2 * FPS; ++Index)
		{
			const double Time = double(Index) / FPS;
			const double PhysicsTime = double(FMath::FloorToInt(Time * 60 + .0001)) / 60;
			Sample.Velocity = FVector(1800 * PhysicsTime, 0, 0);
			Sample.Position = FVector(900 * Time * Time, 0, 0);
			UpdateChassis(Config, Sample, 1.f / FPS, State);
		}
		TestEqual(TEXT("Fixed-step body sampling retains full lean"), State.PitchDegrees, 4.f, .13f);
	}
	FHALChassisPresentationState State;
	FHALChassisMotionSample Sample;
	Sample.bDrivingAllowed = true; Sample.Velocity = FVector(1000, 0, 0);
	UpdateChassis(Config, Sample, .02f, State);
	for (int32 Index = 1; Index <= 40; ++Index)
	{
		Sample.Position += Sample.Velocity * .02f;
		Sample.Rotation = FRotator(0, Index * 2, 0).Quaternion();
		UpdateChassis(Config, Sample, .02f, State);
	}
	TestTrue(TEXT("Rotating local axes at constant world velocity does not invent acceleration"), ChassisRotation(Config, State).IsNearlyZero());
	State.PitchDegrees = 3; State.RollDegrees = 2;
	Config.PitchAxis = EHALBoneAxis::Z; Config.RollAxis = EHALBoneAxis::Y;
	Config.PitchSign = 1; Config.RollSign = -1;
	const FQuat Expected = AxisRotation(EHALBoneAxis::Y, -2).Quaternion() * AxisRotation(EHALBoneAxis::Z, 3).Quaternion();
	TestTrue(TEXT("Calibrated axes/signs compose bone-local offsets"), ChassisRotation(Config, State).Quaternion().Equals(Expected, .0001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALChassisSamplingTest, "HAL.FutureCreation.Presentation.ChassisSamplingAndGates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALChassisSamplingTest::RunTest(const FString& Parameters)
{
	using namespace HALVehiclePresentation;
	FHALChassisPresentationConfig Config; Config.bEnabled = true;
	FHALChassisPresentationState State;
	FHALChassisMotionSample Sample;
	Sample.bDrivingAllowed = true; Sample.Velocity = FVector(1800, 0, 0);
	UpdateChassis(Config, Sample, .02f, State);
	TestEqual(TEXT("First sample seeds without inventing start acceleration"), State.PitchDegrees, 0.f);
	for (int32 Index = 0; Index < 20; ++Index)
	{
		Sample.Velocity += FVector(100, 100, 0); Sample.Position += Sample.Velocity * .02f;
		UpdateChassis(Config, Sample, .02f, State);
	}
	TestEqual(TEXT("Strong sustained motion cannot exceed pitch cap"), State.PitchDegrees, 4.f);
	TestEqual(TEXT("Strong sustained motion cannot exceed roll cap"), State.RollDegrees, 4.f);
	Sample.Position += FVector(200, 0, 0);
	UpdateChassis(Config, Sample, .02f, State);
	TestTrue(TEXT("Sub-teleport network correction clears old lean"), ChassisRotation(Config, State).IsNearlyZero());
	Sample.Velocity += FVector(10000, 0, 0);
	UpdateChassis(Config, Sample, .02f, State);
	TestTrue(TEXT("Velocity spikes do not become an impact animation"), ChassisRotation(Config, State).IsNearlyZero());
	Sample.bDrivingAllowed = false;
	UpdateChassis(Config, Sample, .02f, State);
	Sample.Velocity += FVector(200, 0, 0); Sample.bDrivingAllowed = true;
	Sample.Position += Sample.Velocity * .02f;
	UpdateChassis(Config, Sample, .02f, State);
	TestTrue(TEXT("Landing/recovery exit seeds instead of differentiating airborne velocity"), ChassisRotation(Config, State).IsNearlyZero());
	Sample.Velocity.X = std::numeric_limits<double>::quiet_NaN();
	UpdateChassis(Config, Sample, .02f, State);
	TestFalse(TEXT("Invalid velocity drops history"), State.bHasSample);
	Sample = {}; Sample.bDrivingAllowed = true;
	UpdateChassis(Config, Sample, .02f, State);
	Sample.Velocity = FVector(30, 0, 0); Sample.Position += Sample.Velocity * .02f;
	UpdateChassis(Config, Sample, .02f, State);
	TestTrue(TEXT("Valid acceleration resumes after invalid data"), State.PitchDegrees > 0);
	Config.bEnabled = false;
	UpdateChassis(Config, Sample, .02f, State);
	TestTrue(TEXT("Disabling chassis clears offsets and history"), !State.bHasSample && ChassisRotation(Config, State).IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALChassisComponentTest, "HAL.FutureCreation.Presentation.ChassisSnapshotPreservesPhysics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALChassisComponentTest::RunTest(const FString& Parameters)
{
	FPresentationWorld F;
	if (!TestNotNull(TEXT("Saved vehicle"), F.Pawn)) { return false; }
	auto* Component = FHALPresentationTestAccess::EnableChassisProbe(F.Pawn);
	auto* Body = FHALPresentationTestAccess::Body(F.Pawn);
	FHALPresentationTestAccess::PublishSupport(F.Pawn, true);
	Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Component->TickComponent(.02f, LEVELTICK_All, nullptr);
	Body->SetPhysicsLinearVelocity(FVector(36, 18, 0));
	const auto Transform = Body->GetComponentTransform();
	const auto Velocity = Body->GetPhysicsLinearVelocity();
	const auto Angular = Body->GetPhysicsAngularVelocityInRadians();
	const float Mass = Body->GetMass();
	Component->TickComponent(.02f, LEVELTICK_All, nullptr);
	const auto Frame = Component->GetPresentationFrame();
	TestTrue(TEXT("Grounded snapshot exposes chassis response"), Frame.bDrivingLeanActive && Frame.ChassisPitchDegrees > 0 && Frame.ChassisRollDegrees > 0);
	TestTrue(TEXT("Chassis animation leaves root transform unchanged"), Body->GetComponentTransform().Equals(Transform));
	TestTrue(TEXT("Chassis animation leaves root velocities unchanged"), Body->GetPhysicsLinearVelocity().Equals(Velocity) && Body->GetPhysicsAngularVelocityInRadians().Equals(Angular));
	TestEqual(TEXT("Chassis animation leaves mass unchanged"), Body->GetMass(), Mass);
	auto* Anim = NewObject<UHALVehicleAnimInstance>(F.Pawn->FindComponentByClass<USkeletalMeshComponent>());
	Anim->NativeInitializeAnimation(); Anim->NativeUpdateAnimation(.02f);
	TestTrue(TEXT("AnimInstance copies the chassis offset"), Anim->Presentation.ChassisRotation.Equals(Frame.ChassisRotation));
	FHALPresentationTestAccess::PublishSupport(F.Pawn, true, true);
	for (int32 Index = 0; Index < 60; ++Index) { Component->TickComponent(.02f, LEVELTICK_All, nullptr); }
	TestTrue(TEXT("Executing recovery masks driving lean and decays the offset"), !Component->GetPresentationFrame().bDrivingLeanActive && Component->GetPresentationFrame().ChassisRotation.IsNearlyZero(.02f));
	TestTrue(TEXT("Animation frame copy stays immutable"), Anim->Presentation.ChassisRotation.Equals(Frame.ChassisRotation));
	Component->TickComponent(.5f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Long frame resets chassis together with wheel history"), !Component->GetPresentationFrame().bValid && Component->GetPresentationFrame().ChassisRotation.IsNearlyZero());
	FHALPresentationTestAccess::PublishSupport(F.Pawn, true);
	Component->TickComponent(.02f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Resume starts with neutral chassis"), Component->GetPresentationFrame().ChassisRotation.IsNearlyZero());
	FHALPresentationTestAccess::DisableChassis(Component);
	Component->TickComponent(.02f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Chassis toggle keeps wheel presentation active"), Component->GetPresentationFrame().bValid && Component->GetPresentationFrame().ChassisRotation.IsNearlyZero());
	return true;
}
#endif
