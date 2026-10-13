#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALHealthAttributeSet.h"
#include "../HALPlayerState.h"
#include "../PassiveTestVehicle.h"
#include "../TestVehiclePawn.h"
#include "../VehicleHealthComponent.h"
#include "../VehicleConfigurationTypes.h"
#include "../CombatResolver.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include <limits>

namespace
{
	struct FGASHealthTestWorld
	{
		UWorld* World;
		FGASHealthTestWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
				.ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
				true, ERHIFeatureLevel::Num, &Values);
		}
		~FGASHealthTestWorld() { if (World) { World->DestroyWorld(false); } }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALGASHealthBoundsTest,
	"HAL.FutureCreation.GAS.Health.BoundsAndIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALGASHealthBoundsTest::RunTest(const FString& Parameters)
{
	FGASHealthTestWorld Fixture;
	if (!TestNotNull(TEXT("World"), Fixture.World)) { return false; }
	APassiveTestVehicle* First = Fixture.World->SpawnActor<APassiveTestVehicle>();
	APassiveTestVehicle* Second = Fixture.World->SpawnActor<APassiveTestVehicle>();
	UVehicleHealthComponent* Health = First->FindComponentByClass<UVehicleHealthComponent>();
	FVehicleHealthConfig Config;
	Config.Health.MaxHP = 150;
	TestTrue(TEXT("Saved configuration accepted before gameplay"), Health->ApplyConfiguration(Config));
	First->DispatchBeginPlay();
	Second->DispatchBeginPlay();
	UHALAbilitySystemComponent* ASC = CastChecked<UHALAbilitySystemComponent>(First->GetAbilitySystemComponent());
	const UHALHealthAttributeSet* Attributes = ASC->GetSet<UHALHealthAttributeSet>();
	TestTrue(TEXT("Non-player GAS health is ready"), Health->IsHealthReady());
	TestEqual(TEXT("ASC Owner is the passive actor"), ASC->GetOwnerActor(), static_cast<AActor*>(First));
	TestEqual(TEXT("Saved seed initializes GAS, not a native 100 HP default"), Attributes->GetCurrentHP(), 150.0f);
	TestNull(TEXT("Facade no longer declares a replicated CurrentHP store"),
		FindFProperty<FProperty>(UVehicleHealthComponent::StaticClass(), TEXT("CurrentHP")));

	FVehicleHitContext Hit;
	Hit.SourceActor = Fixture.World->SpawnActor<AActor>();
	Hit.TargetActor = First;
	TestFalse(TEXT("NaN damage cannot poison attributes"), FCombatResolver::ResolveVehicleHit(
		Hit, std::numeric_limits<float>::quiet_NaN(), 1).bResolved);
	TestTrue(TEXT("Existing resolver damage reaches the instant GE"), FCombatResolver::ResolveVehicleHit(Hit, 25, 1).bResolved);
	TestEqual(TEXT("GAS receives damage exactly once"), Attributes->GetCurrentHP(), 125.0f);
	TestEqual(TEXT("Facade reads the same HP"), Health->GetCurrentHP(), Attributes->GetCurrentHP());
	TestEqual(TEXT("Other actor's attributes are independent"),
		Second->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(), 100.0f);
	TestTrue(TEXT("Repeated initialization succeeds without resetting"), ASC->InitializeHealth(999));
	TestEqual(TEXT("Repeated binding cannot heal a damaged owner"), Health->GetCurrentHP(), 125.0f);
	TestEqual(TEXT("Repeated initialization cannot replace MaxHP"), Health->GetMaxHP(), 150.0f);
	TestTrue(TEXT("Overkill is accepted"), FCombatResolver::ResolveVehicleHit(Hit, 1000, 1).bResolved);
	TestEqual(TEXT("HP is clamped at zero"), Attributes->GetCurrentHP(), 0.0f);
	ASC->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(), 999);
	TestEqual(TEXT("HP cannot exceed capacity"), Attributes->GetCurrentHP(), 150.0f);
	ASC->SetNumericAttributeBase(UHALHealthAttributeSet::GetMaxHPAttribute(), 80);
	TestEqual(TEXT("Capacity reduction clamps current HP"), Attributes->GetCurrentHP(), 80.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALGASPlayerAvatarTest,
	"HAL.FutureCreation.GAS.Health.PlayerStateAvatarLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALGASPlayerAvatarTest::RunTest(const FString& Parameters)
{
	FGASHealthTestWorld Fixture;
	if (!TestNotNull(TEXT("World"), Fixture.World)) { return false; }
	AHALPlayerState* State = Fixture.World->SpawnActor<AHALPlayerState>();
	UHALAbilitySystemComponent* ASC = State->GetHALAbilitySystemComponent();
	TestFalse(TEXT("ASC is not initialized with guessed health"), ASC->IsHealthInitialized());
	ATestVehiclePawn* First = Fixture.World->SpawnActor<ATestVehiclePawn>();
	ATestVehiclePawn* Replacement = Fixture.World->SpawnActor<ATestVehiclePawn>();
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/SM_CarCollisionProxy.SM_CarCollisionProxy"));
	if (!TestNotNull(TEXT("Saved convex test mesh"), Mesh)) { return false; }
	CastChecked<UStaticMeshComponent>(First->GetRootComponent())->SetStaticMesh(Mesh);
	CastChecked<UStaticMeshComponent>(Replacement->GetRootComponent())->SetStaticMesh(Mesh);
	First->SetPlayerState(State);
	First->DispatchBeginPlay();
	UVehicleHealthComponent* FirstHealth = First->FindComponentByClass<UVehicleHealthComponent>();
	TestTrue(TEXT("Player health binds after Pawn begin"), FirstHealth->IsHealthReady());
	TestEqual(TEXT("Player ASC belongs to PlayerState"), ASC->GetOwnerActor(), static_cast<AActor*>(State));
	TestEqual(TEXT("Current vehicle is Avatar"), ASC->GetAvatarActor(), static_cast<AActor*>(First));
	FVehicleHitContext Hit;
	Hit.SourceActor = Fixture.World->SpawnActor<AActor>();
	Hit.TargetActor = First;
	TestTrue(TEXT("Player damage reaches PlayerState attributes"), FCombatResolver::ResolveVehicleHit(Hit, 25, 1).bResolved);
	First->PawnClientRestart();
	TestEqual(TEXT("Repeated restart does not reset damaged HP"), FirstHealth->GetCurrentHP(), 75.0f);
	Replacement->SetPlayerState(State);
	Replacement->DispatchBeginPlay();
	UVehicleHealthComponent* ReplacementHealth = Replacement->FindComponentByClass<UVehicleHealthComponent>();
	TestEqual(TEXT("New Avatar retains owner health"), ReplacementHealth->GetCurrentHP(), 75.0f);
	First->UnPossessed();
	TestEqual(TEXT("Old Avatar cleanup cannot clear replacement"), ASC->GetAvatarActor(), static_cast<AActor*>(Replacement));
	TestNull(TEXT("Old facade unsubscribes from owner"), FirstHealth->GetBoundAbilitySystem());
	Replacement->UnPossessed();
	TestNull(TEXT("Unpossess clears Avatar"), ASC->GetAvatarActor());
	TestEqual(TEXT("Owner survives without Avatar"), ASC->GetOwnerActor(), static_cast<AActor*>(State));
	TestEqual(TEXT("Unpossess does not destroy owner HP"), ASC->GetSet<UHALHealthAttributeSet>()->GetCurrentHP(), 75.0f);
	TestFalse(TEXT("No-local-fallback player cannot report ready HP"), ReplacementHealth->IsHealthReady());
	return true;
}

#endif
