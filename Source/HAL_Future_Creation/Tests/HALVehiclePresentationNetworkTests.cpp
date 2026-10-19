#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALVehiclePresentationTestSupport.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

namespace
{
	class FPresentationLiveTest : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline = FPlatformTime::Seconds() + 90, Complete = -1, NeutralAt = -1;
		bool bStarted = false, bActive = false, bNeutral = false;
	public:
		explicit FPresentationLiveTest(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			if (Complete > 0) { return FPlatformTime::Seconds() - Complete > 4; }
			if (FPlatformTime::Seconds() > Deadline)
			{ Test->AddError(TEXT("Presentation network test requires matching Listen Server/client processes; timed out.")); return true; }
			UWorld* World = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
			{
				auto* Candidate = Context.World();
				if (Candidate && Candidate->IsGameWorld() && (Candidate->GetNetMode() == NM_Client || Candidate->GetNetMode() == NM_ListenServer))
				{ World = Candidate; break; }
			}
			if (!World) { return false; }
			const bool bHost = World->GetNetMode() == NM_ListenServer;
			AHALPlayerController* PC = nullptr;
			for (auto It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (auto* Candidate = Cast<AHALPlayerController>(It->Get()); Candidate && Candidate->IsLocalController()) { PC = Candidate; break; }
			}
			auto* Own = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			ATestVehiclePawn* Other = nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World); It; ++It) { if (*It != Own && It->GetPlayerState()) { Other = *It; break; } }
			auto* Match = World->GetGameState<AHALMatchGameState>();
			if (!Own || !Other || !Own->GetPlayerState() || !Other->GetPlayerState() || !Match) { return false; }
			auto* Local = FHALPresentationTestAccess::EnableProbe(Own);
			auto* Remote = FHALPresentationTestAccess::EnableProbe(Other);
			if (Match->GetMatchPhase() == EHALMatchPhase::WaitingForPlayers)
			{
				if (bHost) { PC->HALStartMatch(); }
				return false;
			}
			const float Desired = bHost ? 0.65f : -0.65f;
			if (!bStarted) { FHALPresentationTestAccess::SetInput(Own, Desired); bStarted = true; }
			const auto LocalFrame = Local->GetPresentationFrame();
			const auto RemoteFrame = Remote->GetPresentationFrame();
			if (!bActive && LocalFrame.bValid && RemoteFrame.bValid
				&& FMath::Abs(LocalFrame.SteeringDegrees - Desired * 25) < 0.6f
				&& FMath::Abs(RemoteFrame.SteeringDegrees + Desired * 25) < 0.8f)
			{
				bActive = true;
				Test->TestTrue(TEXT("Rear wheels remain unsteered for both roles"), LocalFrame.BL.SteeringRotation.IsNearlyZero() && RemoteFrame.BR.SteeringRotation.IsNearlyZero());
				if (!bHost)
				{
					Test->TestTrue(TEXT("Remote observer uses replicated accepted steer"), Remote->GetObserverSteering() > 0.6f);
				}
				else
				{
					const auto Input = Other->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetPresentationInput();
					Test->TestTrue(TEXT("Server remote animation reads consumed client physics input"), Input.bValid && Input.Steering < -0.6f);
				}
				PC->SetName(bHost ? TEXT("HAL_VP_HostActive") : TEXT("HAL_VP_ClientActive"));
				UE_LOG(LogTemp, Display, TEXT("HAL Presentation smoke: Host=%d Active=1 Local=%.3f Remote=%.3f Observer=%.3f HeadlessProbe=1"),
					bHost ? 1 : 0, LocalFrame.SteeringDegrees, RemoteFrame.SteeringDegrees, Remote->GetObserverSteering());
			}
			if (bActive && !bNeutral && Other->GetPlayerState()->GetPlayerName().Contains(TEXT("Active")))
			{
				FHALPresentationTestAccess::SetInput(Own, 0); bNeutral = true; NeutralAt = World->GetTimeSeconds();
			}
			if (bNeutral && World->GetTimeSeconds() - NeutralAt > 1
				&& FMath::Abs(LocalFrame.SteeringDegrees) < 0.2f && FMath::Abs(RemoteFrame.SteeringDegrees) < 0.2f)
			{
				Test->TestTrue(TEXT("Both local and observer steering return to neutral"), true);
				UE_LOG(LogTemp, Display, TEXT("HAL Presentation smoke: Host=%d Complete=1 Local=%.3f Remote=%.3f"), bHost ? 1 : 0, LocalFrame.SteeringDegrees, RemoteFrame.SteeringDegrees);
				Complete = FPlatformTime::Seconds();
			}
			return false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALPresentationNetworkTest, "HAL.NetworkSmoke.VehiclePresentation",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALPresentationNetworkTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FPresentationLiveTest>(this)); return true;
}
#endif
