#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALVehiclePresentationTestSupport.h"
#include "../HALPlayerController.h"
#include "../HALMatchGameState.h"
#include "../HALDebugSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

namespace
{
	class FLaunchVisualLiveTest : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline = FPlatformTime::Seconds() + 90, Complete = -1;
		float AlignedAt = -1, ShotAt = -1, LocalPeak = 0, RemotePeak = 0;
		bool bPrepared = false, bAligned = false, bHostShot = false, bClientShot = false, bRejectedRequest = false;
		bool bImmediatePrediction = false;
	public:
		explicit FLaunchVisualLiveTest(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			if (Complete > 0) { return FPlatformTime::Seconds() - Complete > 4; }
			if (FPlatformTime::Seconds() > Deadline) { Test->AddError(TEXT("Launch visual smoke requires matching host/client; timed out.")); return true; }
			UWorld* World = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
			{ auto* Candidate = Context.World(); if (Candidate && Candidate->IsGameWorld() && (Candidate->GetNetMode() == NM_Client || Candidate->GetNetMode() == NM_ListenServer)) { World = Candidate; break; } }
			if (!World) { return false; }
			const bool bHost = World->GetNetMode() == NM_ListenServer;
			AHALPlayerController* PC = nullptr;
			for (auto It = World->GetPlayerControllerIterator(); It; ++It) { if (auto* C = Cast<AHALPlayerController>(It->Get()); C && C->IsLocalController()) { PC = C; break; } }
			auto* Own = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			ATestVehiclePawn* Other = nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World); It; ++It) { if (*It != Own && It->GetPlayerState()) { Other = *It; break; } }
			auto* Match = World->GetGameState<AHALMatchGameState>();
			if (!Own || !Other || !Own->GetPlayerState() || !Other->GetPlayerState() || !Match) { return false; }
			if (!bPrepared)
			{
				FHALPresentationTestAccess::EnableLaunchProbe(Own); FHALPresentationTestAccess::EnableLaunchProbe(Other);
				FHALPresentationTestAccess::SetInput(Own, 0);
				PC->SetName(bHost ? TEXT("HAL_VP4AReadyH") : TEXT("HAL_VP4AReadyC")); bPrepared = true;
			}
			for (auto* Pawn : { Own, Other }) { FHALPresentationTestAccess::Body(Pawn)->SetEnableGravity(false); }
			const FString OtherName = Other->GetPlayerState()->GetPlayerName();
			if (Match->GetMatchPhase() == EHALMatchPhase::WaitingForPlayers) { if (bHost && OtherName.StartsWith(TEXT("HAL_VP4A"))) { PC->HALStartMatch(); } return false; }
			if (!OtherName.StartsWith(TEXT("HAL_VP4A"))) { return false; }
			if (bHost && !bAligned)
			{
				int32 Index = 0;
				for (auto* Pawn : { Own, Other })
				{
					auto* Body = FHALPresentationTestAccess::Body(Pawn);
					Body->SetWorldLocationAndRotation(FVector(Index++ ? 1200 : -1200,0,10000), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
					Body->SetPhysicsLinearVelocity(FVector::ZeroVector); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector); Pawn->ForceNetUpdate();
				}
				bAligned = true; AlignedAt = World->GetTimeSeconds(); PC->SetName(TEXT("HAL_VP4AAlignedH"));
			}
			auto* Local = Own->FindComponentByClass<UHALVehiclePresentationComponent>();
			auto* Remote = Other->FindComponentByClass<UHALVehiclePresentationComponent>();
			if (!bHost && !bAligned && Own->GetActorLocation().Z > 9000 && Other->GetActorLocation().Z > 9000
				&& Local->GetPresentationFrame().bValid && Remote->GetPresentationFrame().bValid)
			{ bAligned = true; PC->SetName(TEXT("HAL_VP4AAlignedC")); }
			LocalPeak = FMath::Max(LocalPeak, FMath::Abs(Local->GetPresentationFrame().LaunchPitchDegrees));
			RemotePeak = FMath::Max(RemotePeak, FMath::Abs(Remote->GetPresentationFrame().LaunchPitchDegrees));
			if (bHost && bAligned && !bHostShot && OtherName.Contains(TEXT("Aligned")) && World->GetTimeSeconds() - AlignedAt > .5f)
			{
				auto* BallClass = GetDefault<UHALDebugSettings>()->DefaultBallClass.LoadSynchronous();
				if (!BallClass) { Test->AddError(TEXT("Missing saved ball class.")); return true; }
				for (auto* Pawn : { Own, Other })
				{
					FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					auto* Ball = World->SpawnActor<ABasicBallActor>(BallClass, Pawn->GetActorLocation() + FVector(250,0,0), FRotator::ZeroRotator, Spawn);
					if (!FHALPresentationTestAccess::GiveBall(Pawn, Ball)) { Test->AddError(TEXT("Native controlled-ball fixture failed.")); return true; }
				}
				FHALPresentationTestAccess::StartLaunch(Own); bHostShot = true; ShotAt = World->GetTimeSeconds(); PC->SetName(TEXT("HAL_VP4AFiringH"));
			}
			if (!bHost && bAligned && !bClientShot)
			{
				for (TActorIterator<ABasicBallActor> It(World); It; ++It)
				{
					if (It->GetBallState() == EBasicBallState::Controlled && It->GetControlledBy() == Own)
					{
						FHALPresentationTestAccess::StartLaunch(Own); bClientShot = true;
						const auto& Stats = FHALPresentationTestAccess::LaunchState(Local);
						bImmediatePrediction = Stats.PredictedCount == 1 && Stats.StartedCount == 1 && Stats.ConfirmedCount == 0;
						PC->SetName(TEXT("HAL_VP4AFiringC")); break;
					}
				}
			}
			const auto& LocalStats = FHALPresentationTestAccess::LaunchState(Local);
			const auto& RemoteStats = FHALPresentationTestAccess::LaunchState(Remote);
			if (!bHost && bClientShot && !bRejectedRequest && LocalStats.ConfirmedCount == 1 && LocalPeak > 1)
			{
				for (TActorIterator<ABasicBallActor> It(World); It; ++It)
				{
					if (It->GetBallState() == EBasicBallState::Launched && It->GetLaunchedBy() == Own)
					{
						const auto Previous = FHALPresentationTestAccess::SetStaleControlledView(*It, Own);
						FHALPresentationTestAccess::StartLaunch(Own); // Real input path, server has already launched this ball.
						FHALPresentationTestAccess::RestoreBallView(*It, Previous);
						bRejectedRequest = true; ShotAt = World->GetTimeSeconds(); PC->SetName(TEXT("HAL_VP4ARejectC")); break;
					}
				}
			}
			const bool bOtherDone = OtherName.Contains(TEXT("Done"));
			const bool bLocalDone = bHost ? bHostShot && bOtherDone : bRejectedRequest && LocalStats.RejectedCount == 1;
			if (bLocalDone && ShotAt >= 0 && World->GetTimeSeconds() - ShotAt > .7f && LocalPeak > 1 && RemotePeak > 1
				&& LocalStats.Pulses.IsEmpty() && RemoteStats.Pulses.IsEmpty())
			{
				Test->TestEqual(TEXT("Each accepted observer shot starts exactly once"), RemoteStats.StartedCount, 1u);
				Test->TestEqual(TEXT("One accepted local shot plus client-only rejected prediction"), LocalStats.StartedCount, bHost ? 1u : 2u);
				Test->TestEqual(TEXT("No duplicate accepted confirmations"), LocalStats.ConfirmedCount, 1u);
				if (!bHost) { Test->TestTrue(TEXT("Prediction began before the server response"), bImmediatePrediction); Test->TestEqual(TEXT("Rejected shot faded via reliable owner result"), LocalStats.RejectedCount, 1u); }
				Test->TestTrue(TEXT("Both snapshots return to neutral"), Local->GetPresentationFrame().ChassisRotation.IsNearlyZero(.01f) && Remote->GetPresentationFrame().ChassisRotation.IsNearlyZero(.01f));
				UE_LOG(LogTemp, Display, TEXT("HAL VP4A smoke: Host=%d Complete=1 LocalStarts=%u RemoteStarts=%u LocalConfirms=%u Predictions=%u Rejects=%u ImmediatePrediction=%d LocalPeak=%.3f RemotePeak=%.3f Resimulations=%d TransientConfig=1 NativeCaptureFixture=1 StaleClientViewFixture=1"),
					bHost ? 1 : 0, LocalStats.StartedCount, RemoteStats.StartedCount, LocalStats.ConfirmedCount, LocalStats.PredictedCount, LocalStats.RejectedCount,
					bImmediatePrediction ? 1 : 0, LocalPeak, RemotePeak, Own->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetResimulationRunCount());
				PC->SetName(bHost ? TEXT("HAL_VP4ADoneH") : TEXT("HAL_VP4ADoneC")); Complete = FPlatformTime::Seconds();
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALLaunchVisualNetworkTest, "HAL.NetworkSmoke.VehicleLaunchPresentation",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALLaunchVisualNetworkTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FLaunchVisualLiveTest>(this)); return true; }
#endif
