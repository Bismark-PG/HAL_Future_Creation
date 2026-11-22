#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "../HALMatchBallLifecycle.h"
#include "../HALMatchGameState.h"
#include "../BasicBallActor.h"
#include "../BallControlComponent.h"
#include "../TestVehiclePawn.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "Components/BoxComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "AssetCompilingManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	const FHALMatchBallArena* ProductionBallArena()
	{
		return GetDefault<UHALMatchBallSettings>()->Arenas.FindByPredicate([](const FHALMatchBallArena& Arena)
		{ return Arena.MatchMap.ToSoftObjectPath().GetLongPackageName() == TEXT("/Game/Maps/MVPB_Match"); });
	}

	struct FMatchBallWorld
	{
		UWorld* World;
		FMatchBallWorld()
		{
			const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL(), false);
			World->SetBegunPlay(true);
		}
		~FMatchBallWorld()
		{
			World->EndPlay(EEndPlayReason::Quit);
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
		AActor* Block(FVector Center, FVector Extent)
		{
			auto* Actor = World->SpawnActor<AActor>();
			auto* Body = NewObject<UBoxComponent>(Actor);
			Actor->AddInstanceComponent(Body);
			Actor->SetRootComponent(Body);
			Body->SetBoxExtent(Extent);
			Body->SetCollisionProfileName(TEXT("BlockAll"));
			Body->SetWorldLocation(Center);
			Body->RegisterComponent();
			return Actor;
		}
		int32 PrimaryCount() const
		{
			int32 Count = 0;
			for (TActorIterator<ABasicBallActor> It(World); It; ++It) { Count += It->IsPrimaryMatchBall(); }
			return Count;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMatchBallMapTest, "HAL.FutureCreation.MatchBall.ProductionMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALMatchBallMapTest::RunTest(const FString&)
{
	const auto* Rules = ProductionBallArena();
	if (!TestNotNull(TEXT("Competition map is configured"), Rules) || !TestTrue(TEXT("Arena rules valid"), Rules->IsValid())) { return false; }
	auto* Map = Rules->MatchMap.LoadSynchronous();
	UClass* BallClass = Rules->PrimaryBallClass.LoadSynchronous();
	if (!TestNotNull(TEXT("Saved map loads"), Map) || !TestNotNull(TEXT("Production ball class loads"), BallClass)) { return false; }
	int32 Count = 0;
	for (AActor* Actor : Map->PersistentLevel->Actors)
	{
		auto* Ball = Cast<ABasicBallActor>(Actor);
		if (!IsValid(Ball) || !Ball->IsPrimaryMatchBall()) { continue; }
		++Count;
		TestTrue(TEXT("Saved primary matches respawn Blueprint"), Ball->GetClass() == BallClass);
		TestTrue(TEXT("Original kickoff point inside bounds"), Rules->Contains(Ball->GetActorLocation()));
		TestEqual(TEXT("Respawn matches authored primary radius"), Ball->GetConfiguredSphereRadius(), BallClass->GetDefaultObject<ABasicBallActor>()->GetConfiguredSphereRadius());
		const auto* Definition = FindFProperty<FObjectProperty>(BallClass, TEXT("Definition"));
		TestTrue(TEXT("Primary uses its Blueprint's ball DA"), Definition && Definition->Identical_InContainer(Ball, BallClass->GetDefaultObject()));
	}
	TestEqual(TEXT("Exactly one authored primary in the competition map"), Count, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMatchBallLifecycleTest, "HAL.FutureCreation.MatchBall.RecoveryAndSafeReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALMatchBallLifecycleTest::RunTest(const FString&)
{
	const auto* Production = ProductionBallArena();
	if (!TestNotNull(TEXT("Production defaults available"), Production)) { return false; }
	FHALMatchBallArena Rules = *Production;
	Rules.FloorCenter = FVector::ZeroVector;
	Rules.PlayableRadius = 2000;
	UClass* Class = Rules.PrimaryBallClass.LoadSynchronous();
	if (!TestNotNull(TEXT("Primary class"), Class)) { return false; }
	FMatchBallWorld F;
	auto* W = F.World;
	auto* State = W->SpawnActor<AHALMatchGameState>();
	W->SetGameState(State);
	auto* Life = State->FindComponentByClass<UHALMatchBallLifecycle>();
	auto* Floor = F.Block(FVector(0, 0, -20), FVector(2000, 2000, 20));
	const FVector Home(200, 300, 90);
	auto* Original = W->SpawnActor<ABasicBallActor>(Class, Home, FRotator::ZeroRotator);
	auto* Extra = W->SpawnActor<ABasicBallActor>(Class, FVector(1200, 0, 60), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Ordinary ball"), Original) || !TestNotNull(TEXT("Extra ball"), Extra)) { return false; }
	Original->SetPrimaryMatchBall(true);
	Life->PrepareArena(Rules);
	State->SetMatchPhase(EHALMatchPhase::Playing);
	auto Pump = [&](float Seconds) { W->TimeSeconds += Seconds; Life->CheckPrimaryBall(); };
	Pump(4.f);
	TestTrue(TEXT("Stationary ball remains the original"), Life->PrimaryBall.Get() == Original);
	auto* DriverClass = LoadClass<ATestVehiclePawn>(nullptr, TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
	if (!TestNotNull(TEXT("Saved vehicle class"), DriverClass)) { return false; }
	auto* PC = W->SpawnActor<AHALPlayerController>();
	PC->SetPlayerState(W->SpawnActor<AHALPlayerState>());
	auto* Driver = W->SpawnActor<ATestVehiclePawn>(DriverClass, FVector(-850, 0, 200), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Vehicle spawned"), Driver)) { return false; }
	PC->Possess(Driver);
	auto* Control = Driver->FindComponentByClass<UBallControlComponent>();
	if (!TestTrue(TEXT("Normal constraint acquires primary"), Control->AcquireBall(Original))) { return false; }
	Pump(2.f);
	TestTrue(TEXT("Holding preserves ball and constraint"), Original->GetControlledBy() == Driver && Control->GetHeldBall() == Original);
	FVector Recoil;
	if (!TestTrue(TEXT("Normal active launch"), Control->LaunchHeldBall(Recoil))) { return false; }
	Original->SetActorLocation(FVector(0, 0, 5000), false, nullptr, ETeleportType::TeleportPhysics);
	Pump(2.f);
	TestTrue(TEXT("In-bounds airborne launch keeps its attack"), Original->GetBallState() == EBasicBallState::Launched && Life->PrimaryBall.Get() == Original);
	Original->SetActorLocation(FVector(2300, 0, 100), false, nullptr, ETeleportType::TeleportPhysics);
	Pump(.1f);
	Original->SetActorLocation(Home, false, nullptr, ETeleportType::TeleportPhysics);
	Pump(.1f);
	TestTrue(TEXT("Brief excursion clears grace"), Life->PrimaryBall.Get() == Original && !Life->IsReturnAnnounced());
	Original->SetActorLocation(FVector(0, 0, -500), false, nullptr, ETeleportType::TeleportPhysics);
	Pump(.1f); Pump(Rules.OutsideGraceSeconds + .1f);
	if (!TestTrue(TEXT("Confirmed fall announces a return"), Life->IsReturnAnnounced() && F.PrimaryCount() == 0)) { return false; }
	TestTrue(TEXT("Visible ground ring has a visible owner and no collision"), Life->WarningRing && Life->WarningRing->IsVisible()
		&& !Life->WarningRing->GetOwner()->IsHidden() && Life->WarningRing->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	const FVector FirstGround = Life->GetReturnGround();
	TestTrue(TEXT("Uses original authored XY"), FVector::Dist2D(Home, FirstGround) < 1.);
	auto* Obstacle = F.Block(FirstGround + FVector(0, 0, 90), FVector(130, 130, 90));
	Pump(Rules.ReturnWarningSeconds + .1f);
	TestTrue(TEXT("Newly occupied landing site moves and repeats its warning"), Life->IsReturnAnnounced()
		&& FVector::Dist2D(FirstGround, Life->GetReturnGround()) > 100 && F.PrimaryCount() == 0);
	Pump(Rules.ReturnWarningSeconds * .5f);
	TestEqual(TEXT("No early substitute"), F.PrimaryCount(), 0);
	Pump(Rules.ReturnWarningSeconds);
	auto* Replacement = Life->PrimaryBall.Get();
	if (!TestNotNull(TEXT("Replacement created"), Replacement)) { return false; }
	TestTrue(TEXT("Fresh Free ball without stale attack, focus restored"), Replacement->GetClass() == Class
		&& Replacement->GetBallState() == EBasicBallState::Free && !Replacement->GetControlledBy() && !Replacement->GetLaunchedBy()
		&& !Replacement->GetBallRepState().AttackSnapshot.AttackId.IsValid() && Replacement->IsPrimaryCameraTarget());
	Pump(3.f);
	TestEqual(TEXT("Repeated polls keep exactly one primary"), F.PrimaryCount(), 1);
	TestTrue(TEXT("Extra ball stays independent"), IsValid(Extra) && !Extra->IsPrimaryMatchBall());
	if (!TestTrue(TEXT("Replacement capturable"), Control->AcquireBall(Replacement))) { return false; }
	Replacement->Destroy();
	TestTrue(TEXT("External destruction immediately releases holder and constraint"), !Control->GetHeldBall() && !Control->bConstraintActive);
	Obstacle->Destroy();
	Floor->Destroy();
	Pump(.1f); Pump(3.f);
	TestTrue(TEXT("Unavailable floor defers safely"), !Life->IsReturnAnnounced() && F.PrimaryCount() == 0);
	F.Block(FVector(0, 0, -20), FVector(2000, 2000, 20));
	Pump(.6f);
	TestTrue(TEXT("Recovery retries once ground is available"), Life->IsReturnAnnounced());
	State->SetMatchPhase(EHALMatchPhase::Finished);
	Pump(5.f);
	TestTrue(TEXT("Results cancel outstanding return"), !Life->IsReturnAnnounced() && F.PrimaryCount() == 0 && IsValid(Extra));
	// An arena missing its kickoff actor must recover even with another ball and a permanently blocked center.
	FMatchBallWorld Empty;
	auto* EmptyState = Empty.World->SpawnActor<AHALMatchGameState>();
	Empty.World->SetGameState(EmptyState);
	Empty.Block(FVector(0, 0, -20), FVector(2000, 2000, 20));
	Empty.Block(FVector(0, 0, 200), FVector(920, 920, 200));
	auto* Unmarked = Empty.World->SpawnActor<ABasicBallActor>(Class, FVector(1900, 0, 70), FRotator::ZeroRotator);
	auto* Fresh = EmptyState->FindComponentByClass<UHALMatchBallLifecycle>();
	Fresh->PrepareArena(Rules);
	TestFalse(TEXT("Waiting does not generate a primary"), Fresh->IsReturnAnnounced());
	EmptyState->SetMatchPhase(EHALMatchPhase::Playing);
	TestTrue(TEXT("Remote open floor gets a warning beyond the blocked center"), Fresh->IsReturnAnnounced()
		&& FVector::Dist2D(FVector::ZeroVector, Fresh->GetReturnGround()) > 920.);
	Empty.World->TimeSeconds += 2.;
	Fresh->CheckPrimaryBall();
	TestTrue(TEXT("Exactly one primary while the extra remains unmarked"), Empty.PrimaryCount() == 1
		&& IsValid(Unmarked) && !Unmarked->IsPrimaryMatchBall() && Fresh->PrimaryBall.Get() != Unmarked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMatchBallCueRenderTest, "HAL.RenderSmoke.MatchBall.GroundWarning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALMatchBallCueRenderTest::RunTest(const FString&)
{
	if (!FApp::CanEverRender()) { AddError(TEXT("Actual RHI required.")); return false; }
	FMatchBallWorld Fixture;
	auto* W = Fixture.World;
	auto* State = W->SpawnActor<AHALMatchGameState>();
	auto* Life = State->FindComponentByClass<UHALMatchBallLifecycle>();
	auto* GroundActor = W->SpawnActor<AActor>();
	auto* Deck = NewObject<UStaticMeshComponent>(GroundActor);
	GroundActor->SetRootComponent(Deck);
	Deck->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Deck->SetWorldScale3D(FVector(8, 8, .1));
	Deck->SetWorldLocation(FVector(0, 0, -5));
	Deck->RegisterComponent();
	auto* LightActor = W->SpawnActor<AActor>();
	auto* Light = NewObject<UDirectionalLightComponent>(LightActor);
	LightActor->SetRootComponent(Light);
	Light->SetIntensity(6.f);
	Light->SetWorldRotation(FRotator(-70, -25, 0));
	Light->RegisterComponent();
	auto* Camera = W->SpawnActor<ASceneCapture2D>()->GetCaptureComponent2D();
	Camera->bCaptureEveryFrame = false;
	Camera->ProjectionType = ECameraProjectionMode::Orthographic;
	Camera->OrthoWidth = 420.f;
	Camera->CaptureSource = SCS_FinalColorLDR;
	Camera->ShowFlags.SetTemporalAA(false);
	Camera->ShowFlags.SetBloom(false);
	Camera->ShowFlags.SetLensFlares(false);
	Camera->ShowFlags.SetMotionBlur(false);
	Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	Camera->SetWorldLocation(FVector(0, 0, 700));
	Camera->SetWorldRotation(FRotator(-90, 0, 0));
	auto* Target = NewObject<UTextureRenderTarget2D>();
	Target->InitCustomFormat(256, 256, PF_B8G8R8A8, false);
	Camera->TextureTarget = Target;
	auto Sample = [&]()
	{
		W->SendAllEndOfFrameUpdates();
		FlushRenderingCommands();
		Camera->CaptureScene();
		TArray<FColor> Pixels;
		Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
		return Pixels;
	};
	Life->SetReturnCue(true, FVector::ZeroVector);
	FAssetCompilingManager::Get().FinishAllCompilation();
	const auto Lit = Sample();
	// Check the silhouette against its actual material pixels, independently of the post-process chain.
	Camera->CaptureSource = SCS_BaseColor;
	const auto Visible = Sample();
	Life->SetReturnCue(false);
	const auto Hidden = Sample();
	if (!TestEqual(TEXT("Visible pixels read back"), Visible.Num(), 256 * 256) || !TestEqual(TEXT("Hidden readback"), Hidden.Num(), Visible.Num())) { return false; }
	const FString Proof = FPaths::ProjectSavedDir() / TEXT("MatchBall");
	if (Lit.Num() == Visible.Num()) { FFileHelper::CreateBitmap(*(Proof / TEXT("GroundWarning_Lit.bmp")), 256, 256, Lit.GetData()); }
	FFileHelper::CreateBitmap(*(Proof / TEXT("GroundWarning_BaseColor.bmp")), 256, 256, Visible.GetData());
	FFileHelper::CreateBitmap(*(Proof / TEXT("GroundWarning_Hidden.bmp")), 256, 256, Hidden.GetData());
	int32 RingPixels = 0, InteriorPixels = 0;
	for (int32 I = 0; I < Visible.Num(); ++I)
	{
		const int32 CyanContrast = int32(Visible[I].G) - Visible[I].R;
		const int32 GroundContrast = int32(Hidden[I].G) - Hidden[I].R;
		if (CyanContrast < 24 || CyanContrast - GroundContrast < 16 || Visible[I].B <= Visible[I].R) { continue; }
		++RingPixels;
		if (FVector2D(I % 256 - 128, I / 256 - 128).SizeSquared() < 55 * 55) { ++InteriorPixels; }
	}
	TestTrue(TEXT("Cyan annulus visible from the top; center stays clear"), RingPixels > 750 && InteriorPixels < 40);
	AddInfo(FString::Printf(TEXT("Visible cyan pixels=%d inside center=%d"), RingPixels, InteriorPixels));
	return true;
}
#endif
