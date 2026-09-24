#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../VehicleDefinition.h"
#include "../VehicleKnockbackSettings.h"
#include "../PassiveTestVehicle.h"
#include "../BasicBallActor.h"
#include "../TestVehiclePawn.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../BallControlComponent.h"
#include "../VehicleHealthComponent.h"
#include "../CombatResolver.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Camera/CameraComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace
{
	struct FConfigurationTestWorld
	{
		UWorld* World;
		FConfigurationTestWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
				.ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
				true, ERHIFeatureLevel::Num, &Values);
		}
		~FConfigurationTestWorld() { if (World) { World->DestroyWorld(false); } }
	};

	void SetDefinition(AActor* Actor, UObject* Definition)
	{
		const FEnumProperty* Source = FindFProperty<FEnumProperty>(Actor->GetClass(), TEXT("ConfigurationSource"));
		check(Source);
		Source->GetUnderlyingProperty()->SetIntPropertyValue(Source->ContainerPtrToValuePtr<void>(Actor),
			static_cast<uint64>(EVehicleConfigurationSource::Definition));
		const FObjectProperty* Reference = FindFProperty<FObjectProperty>(Actor->GetClass(), TEXT("Definition"));
		check(Reference);
		Reference->SetObjectPropertyValue_InContainer(Actor, Definition);
	}

	UVehicleDefinition* MakePassiveDefinition()
	{
		UVehicleDefinition* Definition = NewObject<UVehicleDefinition>();
		Definition->CollisionShape = EVehicleCollisionShape::Box;
		Definition->Visual.Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		Definition->bReadyForUse = true;
		return Definition;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfigurationValidationTest,
	"HAL.FutureCreation.Configuration.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FConfigurationValidationTest::RunTest(const FString& Parameters)
{
	FString Error;
	UVehicleDefinition* Definition = MakePassiveDefinition();
	TestTrue(TEXT("A complete passive box configuration is valid"), Definition->Validate(Error));
	Definition->bReadyForUse = false;
	TestFalse(TEXT("Unreviewed seed values cannot enter gameplay"), Definition->Validate(Error));
	Definition->bReadyForUse = true;
	Definition->Physics.Body.LinearDamping = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN cannot bypass editor ClampMin metadata"), Definition->Validate(Error));
	FArcadeVehicleConfig Movement;
	Movement.WallEscape.WallEscapeExitSpeed = Movement.WallEscape.WallEscapeEnterSpeed - 1;
	TestFalse(TEXT("Invalid wall escape hysteresis is rejected"), Movement.Validate(Error));
	FBallControlConfig Control;
	Control.Launch.MaxRecoilDeltaSpeed = Control.Launch.BaseRecoilDeltaSpeed - 1;
	TestFalse(TEXT("Recoil limits must agree"), Control.Validate(Error));
	FVehicleKnockbackConfig Rules;
	Rules.HeavyImpactSpeedThreshold = Rules.MediumImpactSpeedThreshold - 1;
	TestFalse(TEXT("Invalid combat thresholds are rejected"), Rules.Validate(Error));
	UVehicleLocalConfig* Local = NewObject<UVehicleLocalConfig>();
	Local->bReadyForUse = true;
	TestFalse(TEXT("Ready alone cannot substitute for real input references"), Local->Validate(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfigurationLifecycleTest,
	"HAL.FutureCreation.Configuration.InitializationAndIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FConfigurationLifecycleTest::RunTest(const FString& Parameters)
{
	FConfigurationTestWorld Fixture;
	if (!TestNotNull(TEXT("Transient world"), Fixture.World)) { return false; }
	UVehicleDefinition* Definition = MakePassiveDefinition();
	Definition->Health.Health.MaxHP = 150;
	Definition->Physics.Body.SetMassOverride(321, true);
	APassiveTestVehicle* First = Fixture.World->SpawnActor<APassiveTestVehicle>();
	APassiveTestVehicle* Second = Fixture.World->SpawnActor<APassiveTestVehicle>();
	SetDefinition(First, Definition);
	SetDefinition(Second, Definition);
	First->PreInitializeComponents();
	Second->PreInitializeComponents();
	First->DispatchBeginPlay();
	Second->DispatchBeginPlay();
	UVehicleHealthComponent* FirstHealth = First->FindComponentByClass<UVehicleHealthComponent>();
	UVehicleHealthComponent* SecondHealth = Second->FindComponentByClass<UVehicleHealthComponent>();
	TestTrue(TEXT("Valid definition is selected before gameplay"), First->IsConfigurationValid());
	TestEqual(TEXT("Health BeginPlay sees configured starting HP"), FirstHealth->GetCurrentHP(), 150.0f);
	UBoxComponent* Body = CastChecked<UBoxComponent>(First->GetRootComponent());
	TestTrue(TEXT("Actual Chaos body mass agrees with configuration"), FMath::IsNearlyEqual(Body->GetMass(), 321.0f, 0.1f));
	AActor* Source = Fixture.World->SpawnActor<AActor>();
	FVehicleHitContext Hit;
	Hit.SourceActor = Source;
	Hit.TargetActor = First;
	TestTrue(TEXT("Existing resolver applies damage"), FCombatResolver::ResolveVehicleHit(Hit, 25, 1).bResolved);
	TestEqual(TEXT("Damage changes only first instance"), FirstHealth->GetCurrentHP(), 125.0f);
	TestEqual(TEXT("Other vehicle retains independent HP"), SecondHealth->GetCurrentHP(), 150.0f);
	TestEqual(TEXT("Shared asset remains immutable"), Definition->Health.Health.MaxHP, 150.0f);
	TestFalse(TEXT("Applying base configuration after BeginPlay is forbidden"), FirstHealth->ApplyConfiguration(Definition->Health));
	TestEqual(TEXT("Rejected reapply does not reset damaged HP"), FirstHealth->GetCurrentHP(), 125.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfigurationFailureTest,
	"HAL.FutureCreation.Configuration.NoFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FConfigurationFailureTest::RunTest(const FString& Parameters)
{
	FConfigurationTestWorld Fixture;
	if (!TestNotNull(TEXT("Transient world"), Fixture.World)) { return false; }
	APassiveTestVehicle* Invalid = Fixture.World->SpawnActor<APassiveTestVehicle>();
	UVehicleDefinition* Definition = MakePassiveDefinition();
	Definition->BoxExtent = FVector(999);
	Definition->Health.Health.MaxHP = -1;
	SetDefinition(Invalid, Definition);
	AddExpectedError(TEXT("invalid passive VehicleDefinition"), EAutomationExpectedErrorFlags::Contains, 1);
	Invalid->PreInitializeComponents();
	TestFalse(TEXT("Invalid definition blocks gameplay"), Invalid->IsConfigurationValid());
	UBoxComponent* Body = CastChecked<UBoxComponent>(Invalid->GetRootComponent());
	TestTrue(TEXT("Whole definition validated before geometry changes"), Body->GetUnscaledBoxExtent().Equals(FVector(100, 70, 35)));
	TestFalse(TEXT("Invalid actor cannot simulate with fallback values"), Body->IsSimulatingPhysics());
	TestEqual(TEXT("Invalid actor has no gameplay collision"), Body->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	ABasicBallActor* Ball = Fixture.World->SpawnActor<ABasicBallActor>();
	TestFalse(TEXT("A free ball cannot be acquired by an invalid vehicle"), Ball->BeginControl(Invalid));
	APassiveTestVehicle* Target = Fixture.World->SpawnActor<APassiveTestVehicle>();
	FVehicleHitContext Hit;
	Hit.SourceActor = Invalid;
	Hit.TargetActor = Target;
	TestFalse(TEXT("Invalid configuration cannot cause a damage result"), FCombatResolver::ResolveVehicleHit(Hit, 25, 1).bResolved);
	TestEqual(TEXT("Target HP is unchanged"), Target->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(), 100.0f);
	SetDefinition(Ball, nullptr);
	AddExpectedError(TEXT("invalid BallDefinition"), EAutomationExpectedErrorFlags::Contains, 1);
	Ball->PreInitializeComponents();
	TestFalse(TEXT("Missing ball reference does not silently fall back"), Ball->IsConfigurationValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfigurationRulesTest,
	"HAL.FutureCreation.Configuration.CombatRulesSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FConfigurationRulesTest::RunTest(const FString& Parameters)
{
	FConfigurationTestWorld Fixture;
	UVehicleKnockbackSettings* Settings = NewObject<UVehicleKnockbackSettings>();
	Settings->ConfigurationSource = EVehicleConfigurationSource::Legacy;
	Settings->MediumImpactSpeedThreshold = 777;
	FVehicleKnockbackConfig Rules;
	TestTrue(TEXT("Legacy settings remain effective"), Settings->GetRuntimeRules(Rules));
	TestEqual(TEXT("Legacy custom value is preserved"), Rules.MediumImpactSpeedThreshold, 777.0f);
	Settings->ConfigurationSource = EVehicleConfigurationSource::Definition;
	Settings->CombatRules = nullptr;
	TestFalse(TEXT("Definition rules cannot be lazily loaded in the hit path"), Settings->GetRuntimeRules(Rules));
	AddExpectedError(TEXT("Invalid CombatRules"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Missing global rules block initialization"), Settings->InitializeRules());
	TestFalse(TEXT("Legacy thresholds cannot substitute for missing definition"), Settings->GetRuntimeRules(Rules));
	UCombatRulesDefinition* Definition = NewObject<UCombatRulesDefinition>();
	Definition->bReadyForUse = true;
	Definition->BallImpacts.MediumImpactSpeedThreshold = 888;
	Settings = NewObject<UVehicleKnockbackSettings>();
	Settings->ConfigurationSource = EVehicleConfigurationSource::Definition;
	Settings->CombatRules = Definition;
	TestTrue(TEXT("Reviewed global rules are cached before gameplay"), Settings->InitializeRules());
	Definition->BallImpacts.MediumImpactSpeedThreshold = 999;
	TestTrue(TEXT("Cached rules available"), Settings->GetRuntimeRules(Rules));
	TestEqual(TEXT("Editing shared template does not mutate active match rules"), Rules.MediumImpactSpeedThreshold, 888.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfigurationPlayerBallTest,
	"HAL.FutureCreation.Configuration.PlayerAndBallInitialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FConfigurationPlayerBallTest::RunTest(const FString& Parameters)
{
	FConfigurationTestWorld Fixture;
	if (!TestNotNull(TEXT("Transient world"), Fixture.World)) { return false; }
	UVehicleDefinition* Definition = NewObject<UVehicleDefinition>();
	Definition->CollisionMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/SM_CarCollisionProxy.SM_CarCollisionProxy"));
	if (!TestNotNull(TEXT("Existing read-only convex mesh"), Definition->CollisionMesh.Get())) { return false; }
	Definition->Visual.Mesh = Definition->CollisionMesh;
	Definition->bReadyForUse = true;
	Definition->LocalConfig = NewObject<UVehicleLocalConfig>();
	UVehicleLocalConfig* Local = Definition->LocalConfig;
	Local->bReadyForUse = true;
	Local->DefaultMappingContext = NewObject<UInputMappingContext>();
	Local->SteeringAction = NewObject<UInputAction>();
	Local->ThrottleAction = NewObject<UInputAction>();
	Local->BrakeAction = NewObject<UInputAction>();
	Local->HandbrakeAction = NewObject<UInputAction>();
	Local->LaunchAction = NewObject<UInputAction>();
	Local->Camera.FieldOfView = 100;
	Definition->Movement.Steering.SteeringAngularAcceleration = 12;
	Definition->BallControl.Launch.LaunchSpeedIncrement = 4567;
	Definition->Health.Health.MaxHP = 180;
	ATestVehiclePawn* Player = Fixture.World->SpawnActor<ATestVehiclePawn>();
	SetDefinition(Player, Definition);
	Player->PreInitializeComponents();
	Player->DispatchBeginPlay();
	TestTrue(TEXT("Convex player configuration becomes valid"), Player->IsConfigurationValid());
	TestEqual(TEXT("Player HP initializes from definition"), Player->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(), 180.0f);
	TestEqual(TEXT("Local camera configuration is applied"), Player->FindComponentByClass<UCameraComponent>()->FieldOfView, 100.0f);
	const FObjectProperty* Input = FindFProperty<FObjectProperty>(Player->GetClass(), TEXT("DefaultMappingContext"));
	TestTrue(TEXT("Input reference is available before possession consumers"), Input->GetObjectPropertyValue_InContainer(Player) == Local->DefaultMappingContext);
	UArcadeVehicleMovementComponent* Movement = Player->FindComponentByClass<UArcadeVehicleMovementComponent>();
	const FFloatProperty* Steering = FindFProperty<FFloatProperty>(Movement->GetClass(), TEXT("SteeringAngularAcceleration"));
	TestEqual(TEXT("Movement uses definition instead of native seed"), Steering->GetPropertyValue_InContainer(Movement), 12.0f);
	UBallControlComponent* Control = Player->FindComponentByClass<UBallControlComponent>();
	const FFloatProperty* LaunchSpeed = FindFProperty<FFloatProperty>(Control->GetClass(), TEXT("LaunchSpeedIncrement"));
	TestEqual(TEXT("Control sees configured values before timer startup"), LaunchSpeed->GetPropertyValue_InContainer(Control), 4567.0f);
	TestFalse(TEXT("Control rejects live base reload"), Control->ApplyConfiguration(Definition->BallControl));
	TestFalse(TEXT("Movement rejects live base reload"), Movement->ApplyConfiguration(Definition->Movement));
	UBallDefinition* BallDefinition = NewObject<UBallDefinition>();
	BallDefinition->bReadyForUse = true;
	BallDefinition->Visual.Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	BallDefinition->Physics.Body.LinearDamping = 1;
	BallDefinition->Gameplay.Damage.KnockbackStrengthMultiplier = 5;
	ABasicBallActor* Ball = Fixture.World->SpawnActor<ABasicBallActor>();
	SetDefinition(Ball, BallDefinition);
	Ball->PreInitializeComponents();
	Ball->DispatchBeginPlay();
	TestTrue(TEXT("Ball definition becomes valid"), Ball->IsConfigurationValid());
	TestTrue(TEXT("Ball actual mass matches configured body"), FMath::IsNearlyEqual(Ball->GetPhysicsRoot()->GetMass(), 35.0f, 0.1f));
	TestEqual(TEXT("Ball authored damping replaces native seed"), Ball->GetPhysicsRoot()->GetLinearDamping(), 1.0f);
	TestTrue(TEXT("Configured free ball supports existing control transition"), Ball->BeginControl(Player));
	TestEqual(TEXT("Ball state remains instance-owned"), Ball->GetBallState(), EBasicBallState::Controlled);
	TestTrue(TEXT("Existing launch transition is preserved"), Ball->LaunchFromControl(Player, Player, FVector(1000, 0, 0)));
	TestEqual(TEXT("Launched ball retains instigator"), Ball->GetLaunchedBy(), static_cast<AActor*>(Player));
	TestEqual(TEXT("Ball template is not mutated by gameplay"), BallDefinition->Gameplay.Damage.KnockbackStrengthMultiplier, 5.0f);
	return true;
}

#endif
