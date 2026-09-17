#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../VehicleWallEscapeMath.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleWallEscapeEligibilityTest,
	"HAL.FutureCreation.Vehicle.WallEscape.Eligibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleWallEscapeEligibilityTest::RunTest(const FString& Parameters)
{
	FVehicleInputCmd Input;
	Input.Throttle = 1.0f;
	Input.Steering = 1.0f;
	Input.bHandbrake = true;
	const auto Eligible = [&Input](float Speed, bool bActive = false)
	{
		return VehicleWallEscape::IsEligible(Input, Speed, bActive, 300.0f, 450.0f, 0.2f);
	};
	TestTrue(TEXT("Combo qualifies at rest"), Eligible(0.0f));
	TestTrue(TEXT("Entry speed boundary"), Eligible(300.0f));
	TestFalse(TEXT("Cannot enter above entry speed"), Eligible(301.0f));
	TestTrue(TEXT("Active assist survives hysteresis band"), Eligible(400.0f, true));
	TestFalse(TEXT("Active assist exits above exit speed"), Eligible(451.0f, true));
	TestFalse(TEXT("Fast lateral sliding must not qualify"), Eligible(1200.0f));
	Input.Brake = 1.0f;
	TestFalse(TEXT("Brake wins"), Eligible(0.0f));
	Input.Brake = 0.0f;
	Input.bHandbrake = false;
	TestFalse(TEXT("Requires handbrake"), Eligible(0.0f));
	Input.bHandbrake = true;
	Input.Steering = 0.1f;
	TestFalse(TEXT("Stick noise does not qualify"), Eligible(0.0f));
	Input.Steering = -1.0f;
	TestTrue(TEXT("Left steering qualifies"), Eligible(0.0f));
	Input.Throttle = 0.0f;
	TestFalse(TEXT("Requires throttle"), Eligible(0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleWallEscapeForceTest,
	"HAL.FutureCreation.Vehicle.WallEscape.ThrottleProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleWallEscapeForceTest::RunTest(const FString& Parameters)
{
	const TArray<FVector> Wall = { FVector(-1.0f, 0.0f, 0.0f) };
	const FVector Drive(1000.0f, 400.0f, 0.0f);
	const FVector Reduced = VehicleWallEscape::ConstrainThrottle(Drive, Wall, 0.05f, 1.0f);
	TestTrue(TEXT("Retains 5% inward drive and ALL tangent drive"), Reduced.Equals(FVector(50.0f, 400.0f, 0.0f)));
	TestTrue(TEXT("Zero assist preserves normal drive"),
		VehicleWallEscape::ConstrainThrottle(Drive, Wall, 0.05f, 0.0f).Equals(Drive));
	TestTrue(TEXT("Partial assist blends smoothly"),
		VehicleWallEscape::ConstrainThrottle(Drive, Wall, 0.05f, 0.5f).Equals(FVector(525.0f, 400.0f, 0.0f)));
	TestTrue(TEXT("Outward drive is unchanged"),
		VehicleWallEscape::ConstrainThrottle(-Drive, Wall, 0.05f, 1.0f).Equals(-Drive));
	TestTrue(TEXT("No wall preserves normal drive"),
		VehicleWallEscape::ConstrainThrottle(Drive, TArray<FVector>(), 0.05f, 1.0f).Equals(Drive));
	const TArray<FVector> Corner = { Wall[0], FVector(0.0f, -1.0f, 0.0f) };
	TestTrue(TEXT("Two walls cannot reintroduce inward drive into each other"),
		VehicleWallEscape::ConstrainThrottle(Drive, Corner, 0.0f, 1.0f).IsNearlyZero());
	const TArray<FVector> AngledCorner = { Wall[0], FVector(0.5f, -0.8660254f, 0.0f) };
	const FVector CornerResult = VehicleWallEscape::ConstrainThrottle(Drive, AngledCorner, 0.0f, 1.0f);
	TestTrue(TEXT("Angled corner satisfies both wall constraints"),
		FVector::DotProduct(CornerResult, AngledCorner[0]) >= -KINDA_SMALL_NUMBER
		&& FVector::DotProduct(CornerResult, AngledCorner[1]) >= -KINDA_SMALL_NUMBER);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleWallEscapeBlendTest,
	"HAL.FutureCreation.Vehicle.WallEscape.Blend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleWallEscapeBlendTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Half entry time gives half assist"),
		VehicleWallEscape::UpdateBlend(0.0f, true, 0.04f, 0.08f, 0.2f), 0.5f);
	TestEqual(TEXT("Half exit time retains half assist"),
		VehicleWallEscape::UpdateBlend(1.0f, false, 0.1f, 0.08f, 0.2f), 0.5f);
	TestEqual(TEXT("Long frame cannot overshoot"),
		VehicleWallEscape::UpdateBlend(0.0f, true, 1.0f, 0.08f, 0.2f), 1.0f);
	TestEqual(TEXT("Zero exit time disables immediately"),
		VehicleWallEscape::UpdateBlend(1.0f, false, 0.01f, 0.08f, 0.0f), 0.0f);
	float Blend = 0.0f;
	for (int32 Step = 0; Step < 8; ++Step)
	{
		Blend = VehicleWallEscape::UpdateBlend(Blend, true, 0.01f, 0.08f, 0.2f);
	}
	TestTrue(TEXT("Entry is frame-partition independent"), FMath::IsNearlyEqual(Blend, 1.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleWallEscapeYawTest,
	"HAL.FutureCreation.Vehicle.WallEscape.YawControl",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleWallEscapeYawTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Positive yaw acceleration is limited"),
		VehicleWallEscape::YawAcceleration(2.6f, 0.0f, 10.0f, 18.0f, 0.016f), 18.0f);
	TestEqual(TEXT("Negative yaw acceleration is limited"),
		VehicleWallEscape::YawAcceleration(-2.6f, 0.0f, 10.0f, 18.0f, 0.016f), -18.0f);
	TestEqual(TEXT("No correction at target yaw rate"),
		VehicleWallEscape::YawAcceleration(2.6f, 2.6f, 10.0f, 18.0f, 0.016f), 0.0f);
	const float LongFrameCorrection = VehicleWallEscape::YawAcceleration(2.6f, 0.0f, 10.0f, 18.0f, 0.5f);
	TestTrue(TEXT("Yaw control does not overshoot target on a long frame"),
		FMath::IsNearlyEqual(LongFrameCorrection * 0.5f, 2.6f));
	float Rate = 0.0f;
	for (int32 Step = 0; Step < 120; ++Step)
	{
		Rate += VehicleWallEscape::YawAcceleration(2.6f, Rate, 10.0f, 18.0f, 1.0f / 60.0f) / 60.0f;
	}
	TestTrue(TEXT("Rate converges to target"), FMath::IsNearlyEqual(Rate, 2.6f, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleWallEscapeContactTest,
	"HAL.FutureCreation.Vehicle.WallEscape.ComponentContact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleWallEscapeContactTest::RunTest(const FString& Parameters)
{
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Read-only engine mesh for transient test body"), Mesh))
	{
		return false;
	}
	const FBoolProperty* ActiveProperty = FindFProperty<FBoolProperty>(
		UArcadeVehicleMovementComponent::StaticClass(), TEXT("bWallEscapeActive"));
	const FFloatProperty* BlendProperty = FindFProperty<FFloatProperty>(
		UArcadeVehicleMovementComponent::StaticClass(), TEXT("WallEscapeBlend"));
	const FBoolProperty* EnableProperty = FindFProperty<FBoolProperty>(
		UArcadeVehicleMovementComponent::StaticClass(), TEXT("bEnableWallEscape"));
	if (!TestNotNull(TEXT("Runtime active debug property"), ActiveProperty)
		|| !TestNotNull(TEXT("Runtime blend debug property"), BlendProperty)
		|| !TestNotNull(TEXT("Feature enable property"), EnableProperty))
	{
		return false;
	}

	const UWorld::InitializationValues Values = UWorld::InitializationValues()
		.AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
		.ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Transient collision world"), World))
	{
		return false;
	}
	const auto AddStaticBox = [World](const FVector& Center, const FVector& Extent)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
		Actor->AddInstanceComponent(Box);
		Actor->SetRootComponent(Box);
		Box->SetBoxExtent(Extent);
		Box->SetMobility(EComponentMobility::Static);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->SetWorldLocation(Center);
		Box->RegisterComponent();
		return Box;
	};
	AddStaticBox(FVector(0.0f, 0.0f, -25.0f), FVector(2000.0f, 2000.0f, 25.0f));
	UBoxComponent* Wall = AddStaticBox(FVector(120.0f, 0.0f, 100.0f), FVector(20.0f, 1000.0f, 100.0f));
	AActor* Vehicle = World->SpawnActor<AActor>();
	UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>(Vehicle);
	Vehicle->AddInstanceComponent(Body);
	Vehicle->SetRootComponent(Body);
	Body->SetStaticMesh(Mesh);
	Body->SetWorldScale3D(FVector(2.0f, 1.4f, 0.7f));
	Body->SetWorldLocation(FVector(0.0f, 0.0f, 35.0f));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Body->SetCollisionObjectType(ECC_PhysicsBody);
	Body->SetCollisionResponseToAllChannels(ECR_Block);
	Body->SetSimulatePhysics(true);
	Body->SetEnableGravity(false);
	Body->RegisterComponent();
	UArcadeVehicleMovementComponent* Movement = NewObject<UArcadeVehicleMovementComponent>(Vehicle);
	Vehicle->AddInstanceComponent(Movement);
	Movement->SetUpdatedPrimitive(Body);
	Movement->RegisterComponent();
	FVehicleInputCmd Input;
	Input.Throttle = 1.0f;
	Input.Steering = 1.0f;
	Input.bHandbrake = true;
	Movement->SetInputCommand(Input);
	const auto Tick = [Movement]() { Movement->TickComponent(0.02f, LEVELTICK_All, nullptr); };
	const auto Active = [Movement, ActiveProperty]() { return ActiveProperty->GetPropertyValue_InContainer(Movement); };
	Tick();
	TestTrue(TEXT("Body touching floor is grounded"), Movement->IsGrounded());
	TestTrue(TEXT("Static wall activates mesh geometry probe despite supporting floor"), Active());
	TestTrue(TEXT("Entry starts with partial rather than instant assist"),
		FMath::IsNearlyEqual(BlendProperty->GetPropertyValue_InContainer(Movement), 0.25f));
	Input.bHandbrake = false;
	Movement->SetInputCommand(Input);
	Tick();
	TestFalse(TEXT("Releasing handbrake exits active state"), Active());
	TestTrue(TEXT("Releasing combo retains a fading blend"),
		BlendProperty->GetPropertyValue_InContainer(Movement) > 0.0f);
	Input.bHandbrake = true;
	Movement->SetInputCommand(Input);
	Body->SetPhysicsLinearVelocity(FVector(0.0f, 1000.0f, 0.0f));
	Tick();
	TestFalse(TEXT("Runtime high lateral speed blocks assist"), Active());
	Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Movement->ResetInputCommand();
	Movement->SetInputCommand(Input);
	Wall->SetCollisionObjectType(ECC_PhysicsBody);
	Tick();
	TestFalse(TEXT("A blocking dynamic vehicle is not a static wall"), Active());
	Wall->SetCollisionObjectType(ECC_WorldStatic);
	Movement->ResetInputCommand();
	Movement->SetInputCommand(Input);
	Tick();
	TestTrue(TEXT("Static wall reactivates assist"), Active());
	Body->SetWorldRotation(FRotator(0.0f, 0.0f, 180.0f), false, nullptr, ETeleportType::TeleportPhysics);
	Tick();
	TestFalse(TEXT("Inverted body does not receive a flip assist"), Active());
	TestEqual(TEXT("Inverted body clears blend immediately"),
		BlendProperty->GetPropertyValue_InContainer(Movement), 0.0f);
	Body->SetWorldRotation(FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
	Tick();
	TestTrue(TEXT("Upright body can reactivate"), Active());
	EnableProperty->SetPropertyValue_InContainer(Movement, false);
	Tick();
	TestFalse(TEXT("Disabled feature does not activate"), Active());
	TestEqual(TEXT("Disabled feature clears blend immediately"),
		BlendProperty->GetPropertyValue_InContainer(Movement), 0.0f);
	EnableProperty->SetPropertyValue_InContainer(Movement, true);
	Body->SetWorldLocation(FVector(-500.0f, 0.0f, 35.0f), false, nullptr, ETeleportType::TeleportPhysics);
	Tick();
	TestTrue(TEXT("Open ground remains grounded"), Movement->IsGrounded());
	TestFalse(TEXT("Open ground alone never counts as a wall"), Active());
	Body->SetWorldLocation(FVector(0.0f, 0.0f, 250.0f), false, nullptr, ETeleportType::TeleportPhysics);
	Tick();
	TestFalse(TEXT("Airborne body is not grounded"), Movement->IsGrounded());
	TestFalse(TEXT("Airborne body cannot activate assist"), Active());
	TestEqual(TEXT("Airborne body clears assist immediately"),
		BlendProperty->GetPropertyValue_InContainer(Movement), 0.0f);
	World->DestroyWorld(false);
	return true;
}

#endif
