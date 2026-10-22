#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALVehiclePresentationTestSupport.h"
#include "HALLegacyCombatTestRules.h"
#include "../HALVehicleAnimInstance.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include <limits>

namespace
{
	UBoxComponent* SupportBox(UWorld* World, const FVector& Center, const FVector& Extent, ECollisionChannel ObjectType = ECC_WorldStatic)
	{
		auto* Actor = World->SpawnActor<AActor>();
		auto* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box);
		Box->SetMobility(EComponentMobility::Movable);
		Box->SetBoxExtent(Extent);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionObjectType(ObjectType);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->RegisterComponent();
		Actor->SetActorLocation(Center);
		return Box;
	}
	struct FSuspensionWorld
	{
		FScopedHALLegacyCombatTestRules Rules;
		UWorld* World;
		FSuspensionWorld()
		{
			const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
				.ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		}
		~FSuspensionWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
		ATestVehiclePawn* Vehicle(const FVector& Position, bool bSynthetic = true)
		{
			auto* Class = LoadClass<ATestVehiclePawn>(nullptr, TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Pawn = Class ? World->SpawnActor<ATestVehiclePawn>(Class, Position, FRotator::ZeroRotator, Spawn) : nullptr;
			if (Pawn)
			{
				Pawn->PreInitializeComponents(); Pawn->InitializeComponents(); Pawn->PostInitializeComponents();
				FHALPresentationTestAccess::EnableSuspensionProbe(Pawn, bSynthetic);
			}
			return Pawn;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSuspensionMathTest, "HAL.FutureCreation.Presentation.Suspension.GeometryAndFilters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALSuspensionMathTest::RunTest(const FString&)
{
	using namespace HALVehiclePresentation;
	FHALVisualSuspensionConfig Config;
	FHALWheelSuspensionTravel Travel;
	const FVector Center(0,0,30), Up = FVector::UpVector;
	FVector Start, End;
	TestTrue(TEXT("Reference probe is valid"), BuildSuspensionProbe(Config, Travel, Center, Up, 30, Start, End));
	TestEqual(TEXT("Probe starts above maximum compression"), Start.Z, 42.0);
	TestEqual(TEXT("Probe includes sloped-plane radius and maximum extension"), End.Z, -47.0);
	FHALWheelSupportSample Support; Support.bHit = true;
	TestEqual(TEXT("Calibrated flat plane is neutral"), SuspensionTarget(Config, Travel, Center, Up, 30, Support).OffsetCm, 0.f);
	Support.Point.Z = 7;
	TestEqual(TEXT("One raised contact compresses by measured world cm"), SuspensionTarget(Config, Travel, Center, Up, 30, Support).OffsetCm, 7.f);
	Support.Point.Z = 50;
	TestEqual(TEXT("Large compression stays within configured travel"), SuspensionTarget(Config, Travel, Center, Up, 30, Support).OffsetCm, 10.f);
	Support.Point.Z = -9;
	TestEqual(TEXT("Lower support produces extension"), SuspensionTarget(Config, Travel, Center, Up, 30, Support).OffsetCm, -9.f);
	Support.Point.Z = -20;
	TestFalse(TEXT("Unreachable lower ground is not reported as supported"), SuspensionTarget(Config, Travel, Center, Up, 30, Support).bSupported);
	Support = {}; Support.bHit = true; Support.Normal = FVector(0,-.6,.8);
	TestEqual(TEXT("Slope uses radius tangent to plane, not vertical radius"), SuspensionTarget(Config, Travel, FVector(0,0,37.5), Up, 30, Support).OffsetCm, 0.f, .001f);
	Support.Normal = FVector::RightVector;
	TestFalse(TEXT("Wall normals cannot support wheels"), SuspensionTarget(Config, Travel, Center, Up, 30, Support).bSupported);
	Support.Normal = FVector(0,0,-1);
	TestFalse(TEXT("Undersides cannot support wheels"), SuspensionTarget(Config, Travel, Center, Up, 30, Support).bSupported);
	Support.Normal = FVector::UpVector; Support.bStartPenetrating = true;
	TestFalse(TEXT("Start penetration does not invent a compression"), SuspensionTarget(Config, Travel, Center, Up, 30, Support).bSupported);
	Support = {};
	TestEqual(TEXT("No hit extends to droop limit"), SuspensionTarget(Config, Travel, Center, Up, 30, Support).OffsetCm, -15.f);
	TestFalse(TEXT("Inverted pose does not build a probe"), BuildSuspensionProbe(Config, Travel, Center, -Up, 30, Start, End));
	TestEqual(TEXT("Inverted pose targets neutral"), SuspensionTarget(Config, Travel, Center, -Up, 30, Support).OffsetCm, 0.f);
	TestFalse(TEXT("Invalid radius cannot build a probe"), BuildSuspensionProbe(Config, Travel, Center, Up, 0, Start, End));
	Support.bHit = true; Support.Normal.X = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("Nonfinite normal cannot reach the graph"), SuspensionTarget(Config, Travel, Center, Up, 30, Support).bSupported);
	const FTransform Mesh(FRotator(0,90,0), FVector(13,27,-134.5), FVector(6));
	const FTransform Bone(FRotator(0,0,90), FVector(50,40,30), FVector(2));
	const FVector Translation = SuspensionBoneTranslation(Mesh, Bone, Up, 12);
	TestTrue(TEXT("Bone-space offset includes reference rotation and both scales"), Mesh.TransformVector(Bone.TransformVector(Translation)).Equals(Up * 12, .0001));
	// Unreal's positive FRotator.Roll maps local +Y toward -Z; the inverse offset is -Y.
	TestTrue(TEXT("Offset maps to calibrated local axis automatically"), Translation.Equals(FVector(0,-1,0), .0001));
	for (int32 FPS : {30,60,120})
	{
		float Offset = 0;
		for (int32 Frame = 0; Frame < FPS; ++Frame) { Offset = SmoothSteering(Offset, 7, Config.ResponseSeconds, 1.f / FPS); }
		TestEqual(TEXT("Suspension smoothing has the same duration at each FPS"), Offset, 7 * (1.f - FMath::Exp(-1.f / Config.ResponseSeconds)), .0001f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSuspensionConfigTest, "HAL.FutureCreation.Presentation.Suspension.ConfigurationCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALSuspensionConfigTest::RunTest(const FString&)
{
	auto* Config = NewObject<UHALVehiclePresentationConfig>();
	for (auto& Wheel : Config->Wheels) { Wheel.RadiusCm = 30; }
	FString Error;
	TestFalse(TEXT("Old saved assets opt in explicitly"), Config->Suspension.bEnabled);
	TestTrue(TEXT("New neutral defaults preserve valid old presentation settings"), Config->ValidateSettings(Error));
	Config->Suspension.ResponseSeconds = 0;
	TestTrue(TEXT("Immediate response is supported for calibration"), Config->ValidateSettings(Error));
	Config->Suspension.MinimumSupportNormalDot = 0;
	TestFalse(TEXT("Invalid plane divisor is rejected even when disabled"), Config->ValidateSettings(Error));
	Config->Suspension = {};
	Config->Wheels[2].Suspension.MaxCompressionCm = -1;
	TestFalse(TEXT("Negative per-wheel travel rejected"), Config->ValidateSettings(Error));
	Config->Wheels[2].Suspension = {};
	Config->Wheels[0].Suspension.MaxExtensionCm = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("Nonfinite per-wheel travel rejected"), Config->ValidateSettings(Error));
	Config->Wheels[0].Suspension = {};
	Config->Wheels[0].Suspension.MaxCompressionCm = Config->Wheels[0].Suspension.MaxExtensionCm = 0;
	TestTrue(TEXT("An individually locked wheel is allowed"), Config->ValidateSettings(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSuspensionWorldTest, "HAL.FutureCreation.Presentation.Suspension.SceneQueriesAndPhysicsBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALSuspensionWorldTest::RunTest(const FString&)
{
	FSuspensionWorld F;
	auto* Pawn = F.Vehicle(FVector(0,0,1000));
	if (!TestNotNull(TEXT("Saved vehicle class"), Pawn)) { return false; }
	auto* Component = Pawn->FindComponentByClass<UHALVehiclePresentationComponent>();
	auto* Body = FHALPresentationTestAccess::Body(Pawn);
	auto* Floor = SupportBox(F.World, FVector(0,0,960), FVector(1500,1500,10));
	const auto Transform = Body->GetComponentTransform();
	const auto Velocity = Body->GetPhysicsLinearVelocity(), Angular = Body->GetPhysicsAngularVelocityInRadians();
	const float Mass = Body->GetMass();
	const auto Collision = Body->GetCollisionEnabled();
	const bool bSimulates = Body->IsSimulatingPhysics();
	Component->TickComponent(.02f, LEVELTICK_All, nullptr);
	auto Frame = Component->GetPresentationFrame();
	TestEqual(TEXT("Exactly one simple trace per wheel"), Frame.SuspensionTraceCount, 4);
	TestTrue(TEXT("Movable WorldStatic floor supports all four wheels"), Frame.FL.bSuspensionSupported && Frame.FR.bSuspensionSupported && Frame.BL.bSuspensionSupported && Frame.BR.bSuspensionSupported);
	TestTrue(TEXT("Flat calibrated reference remains neutral"), Frame.FL.SuspensionTranslation.IsNearlyZero() && Frame.BR.SuspensionTranslation.IsNearlyZero());
	auto* Step = SupportBox(F.World, FVector(100,-100,973.5), FVector(25,25,3.5));
	SupportBox(F.World, FVector(100,100,980), FVector(25,25,5), ECC_PhysicsBody);
	SupportBox(F.World, FVector(-100,-100,980), FVector(25,25,5), ECC_WorldDynamic);
	for (int32 Index = 0; Index < 60; ++Index) { Component->TickComponent(.02f, LEVELTICK_All, nullptr); }
	Frame = Component->GetPresentationFrame();
	TestEqual(TEXT("Only FL responds to its raised surface, without reference feedback"), Frame.FL.SuspensionOffsetCm, 7.f, .001f);
	TestEqual(TEXT("PhysicsBody obstacle is excluded"), Frame.FR.SuspensionOffsetCm, 0.f, .001f);
	TestEqual(TEXT("WorldDynamic obstacle is excluded"), Frame.BL.SuspensionOffsetCm, 0.f, .001f);
	TestEqual(TEXT("Fourth wheel stays independent"), Frame.BR.SuspensionOffsetCm, 0.f, .001f);
	TestTrue(TEXT("Converted local Y offset drives a rotated suspension reference"), Frame.FL.SuspensionTranslation.Equals(FVector(0,-7,0), .001));
	TestTrue(TEXT("Queries never change root transform or velocities"), Body->GetComponentTransform().Equals(Transform)
		&& Body->GetPhysicsLinearVelocity().Equals(Velocity) && Body->GetPhysicsAngularVelocityInRadians().Equals(Angular));
	TestEqual(TEXT("Mass unchanged"), Body->GetMass(), Mass);
	TestEqual(TEXT("Collision unchanged"), Body->GetCollisionEnabled(), Collision);
	TestEqual(TEXT("Simulation unchanged"), Body->IsSimulatingPhysics(), bSimulates);
	auto* Anim = NewObject<UHALVehicleAnimInstance>(Pawn->FindComponentByClass<USkeletalMeshComponent>());
	Anim->NativeInitializeAnimation(); Anim->NativeUpdateAnimation(.02f);
	TestTrue(TEXT("AnimInstance receives translated wheel snapshot"), Anim->Presentation.FL.SuspensionTranslation.Equals(Frame.FL.SuspensionTranslation));
	Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision); Step->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FHALPresentationTestAccess::SetSuspension(Component, true, .08f);
	for (int32 Index = 0; Index < 60; ++Index) { Component->TickComponent(.02f, LEVELTICK_All, nullptr); }
	Frame = Component->GetPresentationFrame();
	TestFalse(TEXT("No scene support remains"), Frame.FL.bSuspensionSupported);
	TestEqual(TEXT("No-hit pose approaches the configured droop"), Frame.FL.SuspensionOffsetCm, -15.f, .001f);
	TestTrue(TEXT("Animation snapshot is immutable while component updates"), Anim->Presentation.FL.SuspensionTranslation.Equals(FVector(0,-7,0), .001));
	FHALPresentationTestAccess::SetRecoveryExecuting(Pawn, true);
	for (int32 Index = 0; Index < 60; ++Index) { Component->TickComponent(.02f, LEVELTICK_All, nullptr); }
	Frame = Component->GetPresentationFrame();
	TestTrue(TEXT("Active self-righting suppresses probes and fades neutral even while upright"), !Frame.bSuspensionActive
		&& Frame.SuspensionTraceCount == 0 && Frame.FL.SuspensionTranslation.IsNearlyZero(.001));
	FHALPresentationTestAccess::SetRecoveryExecuting(Pawn, false);
	Body->SetWorldRotation(FRotator(0,0,180), false, nullptr, ETeleportType::TeleportPhysics);
	for (int32 Index = 0; Index < 60; ++Index) { Component->TickComponent(.02f, LEVELTICK_All, nullptr); }
	Frame = Component->GetPresentationFrame();
	TestFalse(TEXT("Inverted pose disables probes"), Frame.bSuspensionActive);
	TestEqual(TEXT("Inverted pose makes no queries"), Frame.SuspensionTraceCount, 0);
	TestTrue(TEXT("Inverted pose fades neutral, without counter-rotating the car"), Frame.FL.SuspensionTranslation.IsNearlyZero(.001));
	FHALPresentationTestAccess::SetSuspension(Component, false);
	Component->TickComponent(.02f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Disabled feature costs no traces"), Component->GetPresentationFrame().SuspensionTraceCount, 0);
	Component->TickComponent(.5f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Long gap clears suspension with existing pose lifecycle"), Component->GetPresentationFrame().bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSuspensionActualRigTest, "HAL.FutureCreation.Presentation.Suspension.SavedRigAndFourVehicleQueries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALSuspensionActualRigTest::RunTest(const FString&)
{
	FSuspensionWorld F;
	TArray<UHALVehiclePresentationComponent*> Components;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		auto* Pawn = F.Vehicle(FVector(Index * 1000,0,1000), false);
		if (!TestNotNull(TEXT("Actual saved rig vehicle"), Pawn)) { return false; }
		Components.Add(Pawn->FindComponentByClass<UHALVehiclePresentationComponent>());
	}
	const FVector FirstCenter = FHALPresentationTestAccess::WheelCenter(Components[0], EHALVehicleWheel::FL);
	const float Radius = FHALPresentationTestAccess::WheelRadius(Components[0], EHALVehicleWheel::FL);
	SupportBox(F.World, FVector(1500,0,FirstCenter.Z - Radius - 10), FVector(5000,5000,10));
	double TotalUs = 0; float MaxVehicleUs = 0; int32 TotalTraces = 0;
	for (int32 Index = 0; Index < 120; ++Index)
	{
		for (auto* Component : Components)
		{
			Component->TickComponent(1.f/60, LEVELTICK_All, nullptr);
			const auto Frame = Component->GetPresentationFrame();
			TotalTraces += Frame.SuspensionTraceCount;
			TotalUs += Frame.SuspensionQueryMicroseconds;
			MaxVehicleUs = FMath::Max(MaxVehicleUs, Frame.SuspensionQueryMicroseconds);
		}
	}
	const auto Frame = Components[0]->GetPresentationFrame();
	TestTrue(TEXT("Actual mesh reference geometry finds its neutral support"), Frame.FL.bSuspensionSupported && FMath::Abs(Frame.FL.SuspensionOffsetCm) < .01f);
	TestEqual(TEXT("Four vehicles have a fixed 16-query update budget"), TotalTraces, 120 * 16);
	UE_LOG(LogTemp, Display, TEXT("HAL VP3 QueryBudget: Vehicles=4 Frames=120 Queries=%d MeanFourVehicleQueryUs=%.3f MaxSingleVehicleQueryUs=%.3f ActualFLRadiusCm=%.3f ActualCenter=%s NullRHIMicrobenchmark=1"),
		TotalTraces, TotalUs / 120, MaxVehicleUs, Radius, *FirstCenter.ToCompactString());
	return true;
}
#endif
