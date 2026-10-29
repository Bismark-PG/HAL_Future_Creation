#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALCameraTestSupport.h"
#include "../BasicBallActor.h"
#include "../HALPlayerController.h"
#include "../HALMatchGameState.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	// Requires a real rendering viewport in separate listen-server and client processes.
	// Profiles and primary test ball are transient; no saved DA, BP or map is edited.
	class FCameraNetworkProbe : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline = FPlatformTime::Seconds()+90, ReadyAt = -1, CompleteAt = -1;
		TWeakObjectPtr<ATestVehiclePawn> Configured;
		bool bBallCreated = false;
	public:
		explicit FCameraNetworkProbe(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			if (CompleteAt > 0) { return FPlatformTime::Seconds()-CompleteAt > 4; }
			if (FPlatformTime::Seconds() > Deadline) { Test->AddError(TEXT("Camera smoke requires real viewport plus listen-server/client; timed out.")); return true; }
			UWorld* World = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
			{
				auto* W = Context.World();
				if (W && W->IsGameWorld() && (W->GetNetMode() == NM_ListenServer || W->GetNetMode() == NM_Client)) { World = W; break; }
			}
			if (!World) { return false; }
			const bool bHost = World->GetNetMode() == NM_ListenServer;
			AHALPlayerController* PC = nullptr;
			for (auto It = World->GetPlayerControllerIterator(); It; ++It)
			{ if (auto* P = Cast<AHALPlayerController>(It->Get()); P && P->IsLocalController()) { PC = P; break; } }
			auto* Own = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			ATestVehiclePawn* Other = nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World); It; ++It) { if (*It != Own && It->GetPlayerState()) { Other = *It; break; } }
			auto* Match = World->GetGameState<AHALMatchGameState>();
			if (!Own || !Other || !Own->GetPlayerState() || !Other->GetPlayerState() || !Match) { return false; }
			if (Match->GetMatchPhase() == EHALMatchPhase::WaitingForPlayers) { if (bHost) { PC->HALStartMatch(); } return false; }
			if (Configured.Get() != Own)
			{
				FHALCameraTestAccess::Enable(Own,bHost ? 65 : 100,bHost ? 1200 : 1800); Configured = Own;
				ReadyAt = -1;
			}
			if (bHost && !bBallCreated)
			{
				for (TActorIterator<ABasicBallActor> It(World); It; ++It) { It->SetPrimaryCameraTarget(false); }
				FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				auto* BallClass = LoadClass<ABasicBallActor>(nullptr,TEXT("/Game/Blueprints/Balls/BP_BasicBall.BP_BasicBall_C"));
				auto* Ball = BallClass ? World->SpawnActor<ABasicBallActor>(BallClass,Own->GetActorLocation()+FVector(500,600,200),FRotator::ZeroRotator,Spawn) : nullptr;
				if (!Ball) { Test->AddError(TEXT("Cannot create camera test ball")); return true; }
				if (auto* Body = Cast<UPrimitiveComponent>(Ball->GetRootComponent())) { Body->SetSimulatePhysics(false); Body->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
				Ball->SetPrimaryCameraTarget(true); bBallCreated = true;
			}
			auto* C = Own->FindComponentByClass<UHALDynamicCameraComponent>();
			const auto& F = C->GetDebugFrame();
			int32 X = 0,Y = 0; PC->GetViewportSize(X,Y);
			if (X < 1 || Y < 1 || !F.bValid || F.MainBallCandidates != 1 || FMath::Abs(F.View.FOV-(bHost ? 65 : 100)) > .01) { return false; }
			PC->SetName(bHost ? TEXT("HAL_CAM_HostReady") : TEXT("HAL_CAM_ClientReady"));
			if (!Other->GetPlayerState()->GetPlayerName().Contains(TEXT("Ready"))) { return false; }
			if (ReadyAt < 0) { ReadyAt = FPlatformTime::Seconds(); }
			if (FPlatformTime::Seconds()-ReadyAt < 2) { return false; }
			Test->TestTrue(TEXT("Real viewport used"),F.Viewport.X > 0 && F.Viewport.Y > 0);
			Test->TestEqual(TEXT("PlayerCameraManager consumes local CalcCamera output"),PC->PlayerCameraManager->GetCameraCacheView().FOV,bHost ? 65.f : 100.f);
			Test->TestTrue(TEXT("Own inside final hard screen frame"),FMath::Abs(F.OwnScreen.X-.5) <= .2801 && FMath::Abs(F.OwnScreen.Y-.5) <= .2801);
			Test->TestFalse(TEXT("Remote Pawn has no local camera state"),Other->FindComponentByClass<UHALDynamicCameraComponent>()->GetDebugFrame().bValid);
			Test->TestEqual(TEXT("Explicit server primary marker reaches both ends"),F.MainBallCandidates,1);
			UE_LOG(LogTemp,Display,TEXT("HAL Camera real viewport: Host=%d FOV=%.1f CachedFOV=%.1f Distance=%.1f Size=%.0f/%.0f Own=%.4f/%.4f Primary=%d"),
				bHost,F.View.FOV,PC->PlayerCameraManager->GetCameraCacheView().FOV,F.Distance,F.Viewport.X,F.Viewport.Y,F.OwnScreen.X,F.OwnScreen.Y,F.MainBallCandidates);
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("DynamicCamera")/(bHost ? TEXT("Camera_Host.png") : TEXT("Camera_Client.png")),true,false);
			CompleteAt = FPlatformTime::Seconds(); return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCameraNetworkTest,"HAL.NetworkSmoke.DynamicCamera.RealViewport",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALCameraNetworkTest::RunTest(const FString&)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCameraNetworkProbe>(this)); return true;
}
#endif
