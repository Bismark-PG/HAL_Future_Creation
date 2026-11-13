#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "../HALDebugSettings.h"
#include "../HALExpandingSlipperyArea.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../BasicBallActor.h"
#include "../TestVehiclePawn.h"
#include "../VehicleDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"

namespace
{
	struct FDebugBallWorld
	{
		UWorld* World;
		UHALDebugSettings* Settings = GetMutableDefault<UHALDebugSettings>();
		TSoftClassPtr<ABasicBallActor> PreviousClass = Settings->DefaultBallClass;
		float PreviousDistance = Settings->BallForwardDistance;
		float PreviousHeight = Settings->BallHeightOffset;
		TSoftClassPtr<AHALExpandingSlipperyArea> PreviousAreaClass = Settings->DefaultSlipperyAreaClass;
		float PreviousAreaDistance = Settings->SlipperyAreaForwardDistance;
		FDebugBallWorld()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
				.ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		}
		~FDebugBallWorld()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
			Settings->DefaultBallClass = PreviousClass;
			Settings->BallForwardDistance = PreviousDistance;
			Settings->BallHeightOffset = PreviousHeight;
			Settings->DefaultSlipperyAreaClass = PreviousAreaClass;
			Settings->SlipperyAreaForwardDistance = PreviousAreaDistance;
		}
		TArray<ABasicBallActor*> Balls() const
		{
			TArray<ABasicBallActor*> Result;
			for (TActorIterator<ABasicBallActor> It(World); It; ++It) { Result.Add(*It); }
			return Result;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALDebugBallSpawnTest, "HAL.FutureCreation.Debug.BallSpawnAuthoredConfigurationAndBlocking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALDebugBallSpawnTest::RunTest(const FString& Parameters)
{
	FDebugBallWorld F;
	UClass* BallClass = F.Settings->DefaultBallClass.LoadSynchronous();
	if (!TestNotNull(TEXT("Configured default ball class loads"), BallClass)) { return false; }
	const auto* Property = FindFProperty<FObjectProperty>(BallClass, TEXT("Definition"));
	const UBallDefinition* Definition = Property ? Cast<UBallDefinition>(Property->GetObjectPropertyValue_InContainer(BallClass->GetDefaultObject())) : nullptr;
	if (!TestNotNull(TEXT("Default ball references the authored DA"), Definition)) { return false; }
	F.Settings->BallForwardDistance = 350;
	F.Settings->BallHeightOffset = 100;
	auto* Controller = F.World->SpawnActor<AHALPlayerController>();
	// An isolated world has no LocalPlayer/net driver; model the host intent that GameMode normally sets.
	Controller->SetAsLocalPlayerController();
	Controller->HALSpawnBall();
	TestEqual(TEXT("No possessed vehicle never spawns"), F.Balls().Num(), 0);
	Controller->SetPlayerState(F.World->SpawnActor<AHALPlayerState>());
	auto* Vehicle = F.World->SpawnActor<ATestVehiclePawn>();
	Vehicle->SetActorLocation(FVector(1000, 2000, 500));
	Vehicle->SetActorRotation(FRotator(0, 90, 0));
	Controller->Possess(Vehicle);
	const FVector Expected = Vehicle->GetActorLocation() + FVector(0, 350, 100);
	Controller->HALSpawnBall();
	TArray<ABasicBallActor*> Balls = F.Balls();
	if (!TestEqual(TEXT("One command spawns exactly one ball"), Balls.Num(), 1)) { return false; }
	ABasicBallActor* Ball = Balls[0];
	TestEqual(TEXT("Uses authored class, not native defaults or a live Actor template"), Ball->GetClass(), BallClass);
	TestTrue(TEXT("Uses current vehicle heading and world-up offset"), Ball->GetActorLocation().Equals(Expected, 0.1));
	TestTrue(TEXT("Server ball replicates movement and gameplay state"), Ball->GetIsReplicated() && Ball->IsReplicatingMovement());
	TestEqual(TEXT("Starts Free"), Ball->GetBallState(), EBasicBallState::Free);
	TestNull(TEXT("No inherited holder"), Ball->GetControlledBy());
	TestEqual(TEXT("No inherited attack attribution"), Ball->GetBallRepState().LastLauncherPlayerId, INDEX_NONE);
	// Isolated worlds have not initialized actors for play; execute the same configuration hook explicitly.
	Ball->PreInitializeComponents();
	TestTrue(TEXT("Current saved DA validates including impact migration"), Ball->IsConfigurationValid());
	TestEqual(TEXT("Authored radius retained"), Ball->GetPhysicsRoot()->GetUnscaledSphereRadius(), Definition->Radius);

	Vehicle->SetActorLocation(FVector(3000, 2000, 500));
	Vehicle->SetActorRotation(FRotator(0, 180, 0));
	const FVector BlockedLocation = Vehicle->GetActorLocation() + FVector(-350, 0, 100);
	AActor* Obstacle = F.World->SpawnActor<AActor>();
	UBoxComponent* Wall = NewObject<UBoxComponent>(Obstacle);
	Obstacle->AddInstanceComponent(Wall);
	Obstacle->SetRootComponent(Wall);
	Wall->SetBoxExtent(FVector(200));
	Wall->SetCollisionProfileName(TEXT("BlockAll"));
	Wall->SetWorldLocation(BlockedLocation);
	Wall->RegisterComponent();
	Controller->HALSpawnBall();
	TestEqual(TEXT("Blocked position does not embed or relocate a ball"), F.Balls().Num(), 1);
	Wall->DestroyComponent();
	F.Settings->DefaultBallClass.Reset();
	Controller->HALSpawnBall();
	TestEqual(TEXT("Missing authored class has no native fallback"), F.Balls().Num(), 1);
	F.Settings->DefaultBallClass = F.PreviousClass;
	Controller->UnPossess();
	Controller->Possess(F.World->SpawnActor<APawn>());
	Controller->HALSpawnBall();
	TestEqual(TEXT("Non-vehicle possession never spawns"), F.Balls().Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALDebugSlipperySpawnTest, "HAL.FutureCreation.Debug.SlipperyAreaSpawnAuthorityAndConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALDebugSlipperySpawnTest::RunTest(const FString& Parameters)
{
	FDebugBallWorld F;
	F.Settings->DefaultSlipperyAreaClass = AHALExpandingSlipperyArea::StaticClass();
	F.Settings->SlipperyAreaForwardDistance = 350;
	const auto Count = [&]() { int32 N = 0; for (TActorIterator<AHALExpandingSlipperyArea> It(F.World); It; ++It) { ++N; } return N; };
	auto* Controller = F.World->SpawnActor<AHALPlayerController>();
	Controller->SetAsLocalPlayerController();
	Controller->HALSpawnSlipperyArea();
	TestEqual(TEXT("No vehicle rejected"), Count(), 0);
	Controller->SetPlayerState(F.World->SpawnActor<AHALPlayerState>());
	auto* Vehicle = F.World->SpawnActor<ATestVehiclePawn>();
	Vehicle->SetActorLocation(FVector(1000, 2000, 500));
	Vehicle->SetActorRotation(FRotator(0, 90, 0));
	Controller->Possess(Vehicle);
	Controller->HALSpawnSlipperyArea();
	if (!TestEqual(TEXT("One request produces one area"), Count(), 1)) { return false; }
	TActorIterator<AHALExpandingSlipperyArea> It(F.World);
	const auto* Area = *It;
	TestTrue(TEXT("Position derived from authority vehicle heading"), Area->GetActorLocation().Equals(FVector(1000,2350,500), .1));
	TestTrue(TEXT("Replicated authority actor"), Area->GetIsReplicated());
	TestEqual(TEXT("Configured class retained"), Area->GetClass(), AHALExpandingSlipperyArea::StaticClass());
	TestEqual(TEXT("Actor defaults preserved"), Area->GetAreaConfig().MaximumRadius,
		GetDefault<AHALExpandingSlipperyArea>()->GetAreaConfig().MaximumRadius);
	F.Settings->DefaultSlipperyAreaClass.Reset();
	Controller->HALSpawnSlipperyArea();
	TestEqual(TEXT("Missing class does not fall back"), Count(), 1);
	F.Settings->DefaultSlipperyAreaClass = AHALExpandingSlipperyArea::StaticClass();
	F.Settings->SlipperyAreaForwardDistance = -1;
	Controller->HALSpawnSlipperyArea();
	TestEqual(TEXT("Invalid distance rejected"), Count(), 1);
	F.Settings->SlipperyAreaForwardDistance = 350;
	Controller->UnPossess();
	Controller->Possess(F.World->SpawnActor<APawn>());
	Controller->HALSpawnSlipperyArea();
	TestEqual(TEXT("Nonvehicle rejected"), Count(), 1);
	return true;
}
#endif
