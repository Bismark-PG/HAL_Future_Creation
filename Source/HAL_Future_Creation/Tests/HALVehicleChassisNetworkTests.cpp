#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALVehiclePresentationTestSupport.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

namespace
{
	/** Actual input/physics/replication path; only cosmetic settings are transient copies. */
	class FChassisLiveTest : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline = FPlatformTime::Seconds() + 90, Complete = -1, AlignedAt = -1, StartedAt = -1, MaskedAt = -1;
		bool bPrepared = false, bAligned = false, bStopped = false, bMasked = false, bLocalResponse = false, bRemoteResponse = false;
		float LocalPeak = 0, RemotePeak = 0;
	public:
		explicit FChassisLiveTest(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			if (Complete > 0) { return FPlatformTime::Seconds() - Complete > 4; }
			if (FPlatformTime::Seconds() > Deadline)
			{ Test->AddError(TEXT("Chassis smoke requires matching Listen Server/client processes; timed out.")); return true; }
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
			if (!bPrepared)
			{
				FHALPresentationTestAccess::EnableChassisProbe(Own);
				FHALPresentationTestAccess::EnableChassisProbe(Other);
				FHALPresentationTestAccess::SetInput(Own, 0);
				const auto Initial = Own->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetStabilityObservation();
				UE_LOG(LogTemp, Display, TEXT("HAL Chassis fixture: Host=%d InitialPosition=%s InitialUp=%s Driving=%d Ground=%d"),
					bHost ? 1 : 0, *Own->GetActorLocation().ToCompactString(), *Own->GetActorUpVector().ToCompactString(), Initial.bDrivingAllowed ? 1 : 0, Initial.bNearGround ? 1 : 0);
				PC->SetName(bHost ? TEXT("HAL_VP2ReadyHost") : TEXT("HAL_VP2ReadyClient"));
				bPrepared = true;
			}
			if (Match->GetMatchPhase() == EHALMatchPhase::WaitingForPlayers)
			{
				if (bHost) { PC->HALStartMatch(); }
				return false;
			}
			if (!Other->GetPlayerState()->GetPlayerName().StartsWith(TEXT("HAL_VP2"))) { return false; }
			if (bHost && !bAligned)
			{
				// The saved testing map may leave a car inverted during connection/setup.
				// Start this driving test from known, separated, upright physical poses.
				int32 Index = 0;
				for (auto* Pawn : { Own, Other })
				{
					auto* Body = FHALPresentationTestAccess::Body(Pawn);
					Body->SetEnableGravity(true);
					Body->SetWorldLocationAndRotation(FVector(Index++ ? 1000 : -1000, -2500, 150), FRotator(0, 90, 0), false, nullptr, ETeleportType::TeleportPhysics);
					Body->SetPhysicsLinearVelocity(FVector::ZeroVector); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
					Pawn->ForceNetUpdate();
				}
				PC->SetName(TEXT("HAL_VP2AlignedHost")); bAligned = true; AlignedAt = World->GetTimeSeconds();
			}
			if (!bHost && !bAligned && !Other->GetPlayerState()->GetPlayerName().Contains(TEXT("Ready")))
			{ bAligned = true; AlignedAt = World->GetTimeSeconds(); }
			if (!bAligned) { return false; }
			if (StartedAt < 0)
			{
				if (World->GetTimeSeconds() - AlignedAt < .5 || Own->GetActorUpVector().Z < .98
					|| !Own->FindComponentByClass<UHALVehiclePresentationComponent>()->GetPresentationFrame().bDrivingLeanActive) { return false; }
				StartedAt = World->GetTimeSeconds();
				PC->SetName(bHost ? TEXT("HAL_VP2DrivingHost") : TEXT("HAL_VP2DrivingClient"));
				UE_LOG(LogTemp, Display, TEXT("HAL Chassis fixture: Host=%d DrivingStarted=1 Position=%s Up=%s"),
					bHost ? 1 : 0, *Own->GetActorLocation().ToCompactString(), *Own->GetActorUpVector().ToCompactString());
				FHALPresentationTestAccess::SetInput(Own, bHost ? .4f : -.4f, .55f);
			}
			const auto Local = Own->FindComponentByClass<UHALVehiclePresentationComponent>()->GetPresentationFrame();
			const auto Remote = Other->FindComponentByClass<UHALVehiclePresentationComponent>()->GetPresentationFrame();
			if (Local.bValid && Local.bDrivingLeanActive && FMath::Abs(Local.ChassisPitchDegrees) > .05f) { bLocalResponse = true; }
			if (Remote.bValid && Remote.bDrivingLeanActive && FMath::Abs(Remote.ChassisPitchDegrees) > .05f) { bRemoteResponse = true; }
			LocalPeak = FMath::Max(LocalPeak, FMath::Abs(Local.ChassisPitchDegrees));
			RemotePeak = FMath::Max(RemotePeak, FMath::Abs(Remote.ChassisPitchDegrees));
			if (!bStopped && World->GetTimeSeconds() - StartedAt > 1.5)
			{
				FHALPresentationTestAccess::SetInput(Own, 0);
				PC->SetName(bHost ? TEXT("HAL_VP2StoppedHost") : TEXT("HAL_VP2StoppedClient"));
				bStopped = true;
			}
			if (bHost && bStopped && !bMasked && Other->GetPlayerState()->GetPlayerName().Contains(TEXT("Stopped")))
			{
				int32 Index = 0;
				for (auto* Pawn : { Own, Other })
				{
					auto* Body = FHALPresentationTestAccess::Body(Pawn);
					Body->SetEnableGravity(false);
					Body->SetWorldLocationAndRotation(FVector(Index++ ? 1000 : -1000, -2500, 1500), FRotator(0, 0, 100), false, nullptr, ETeleportType::TeleportPhysics);
					Body->SetPhysicsLinearVelocity(FVector::ZeroVector); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
					Pawn->ForceNetUpdate();
				}
				PC->SetName(TEXT("HAL_VP2MaskedHost")); bMasked = true;
			}
			if (bHost ? bMasked : Other->GetPlayerState()->GetPlayerName().Contains(TEXT("Masked")))
			{
				if (MaskedAt < 0) { MaskedAt = World->GetTimeSeconds(); }
				if (World->GetTimeSeconds() - MaskedAt > 1.2 && Local.bValid && Remote.bValid
					&& !Local.bDrivingLeanActive && !Remote.bDrivingLeanActive
					&& Local.ChassisRotation.IsNearlyZero(.05f) && Remote.ChassisRotation.IsNearlyZero(.05f))
				{
					Test->TestTrue(TEXT("Own vehicle derives lean from actual driving"), bLocalResponse);
					Test->TestTrue(TEXT("Other vehicle derives lean from replicated motion/support permission"), bRemoteResponse);
					Test->TestTrue(TEXT("Both pitch channels stay bounded"), LocalPeak <= 4.001f && RemotePeak <= 4.001f);
					Test->TestTrue(TEXT("Inverted/airborne state fades both native snapshots"), true);
					const auto* Movement = Own->FindComponentByClass<UArcadeVehicleMovementComponent>();
					UE_LOG(LogTemp, Display, TEXT("HAL Chassis smoke: Host=%d Complete=1 OwnResponse=%d OtherResponse=%d LocalPeak=%.3f RemotePeak=%.3f LocalActive=%d RemoteActive=%d Resimulations=%d TransientConfig=1"),
						bHost ? 1 : 0, bLocalResponse ? 1 : 0, bRemoteResponse ? 1 : 0, LocalPeak, RemotePeak,
						Local.bDrivingLeanActive ? 1 : 0, Remote.bDrivingLeanActive ? 1 : 0, Movement->GetResimulationRunCount());
					Complete = FPlatformTime::Seconds();
				}
			}
			return false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALChassisNetworkTest, "HAL.NetworkSmoke.VehicleChassisPresentation",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALChassisNetworkTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FChassisLiveTest>(this)); return true;
}
#endif
