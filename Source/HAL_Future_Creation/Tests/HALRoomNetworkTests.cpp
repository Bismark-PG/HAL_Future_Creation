#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALRoomTestSupport.h"
#include "../HALGameFlowPermissions.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	class FRoomNetworkProbe : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline = FPlatformTime::Seconds()+90, NextRequest = 0, Complete = -1;
		bool bInitialized = false, bDenied = false, bSawReadyPhysics = false;
	public:
		explicit FRoomNetworkProbe(FAutomationTestBase* T) : Test(T) {}
		virtual bool Update() override
		{
			if (Complete > 0) { return FPlatformTime::Seconds()-Complete > 4; }
			if (FPlatformTime::Seconds() > Deadline) { Test->AddError(TEXT("GF-1 network probe timed out; needs matching Listen Server and client.")); return true; }
			UWorld* World = nullptr;
			for (const auto& C : GEngine->GetWorldContexts())
			{ if (auto* W = C.World(); W && W->IsGameWorld() && (W->GetNetMode()==NM_Client || W->GetNetMode()==NM_ListenServer)) { World = W; break; } }
			if (!World) { return false; }
			const bool Host = World->GetNetMode()==NM_ListenServer;
			AHALPlayerController* PC = nullptr; AHALPlayerController* RemotePC = nullptr;
			for (auto It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (auto* P = Cast<AHALPlayerController>(It->Get())) { if (P->IsLocalController()) { PC=P; } else { RemotePC=P; } }
			}
			auto* Pawn = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			auto* PS = PC ? PC->GetPlayerState<AHALPlayerState>() : nullptr;
			auto* State = World->GetGameState<AHALMatchGameState>();
			AHALPlayerState* Other = nullptr;
			if (State) { for (const auto& P : State->PlayerArray) { if (auto* S = Cast<AHALPlayerState>(P); S && S!=PS) { Other=S; break; } } }
			if (!Pawn || !PS || !Other || !State) { return false; }
			if (State->GetMatchPhase()!=EHALMatchPhase::Playing) { if (Host) { PC->HALStartMatch(); } return false; }
			auto* Room = State->GetRoom();
			if (Host && !bInitialized)
			{
				if (!RemotePC || !RemotePC->GetPawn()) { return false; }
				auto* Config = FHALRoomTestAccess::Config(Room,EHALRoomProfile::Development);
				Config->Catalog->Vehicles[0].VehicleId = TEXT("ProbeVehicle");
				Config->Catalog->Vehicles[0].VehicleClass = Pawn->GetClass();
				const auto Result = Room->Initialize(Config);
				Test->TestEqual(TEXT("Server-only transient room opt-in"),Result,EHALRoomResult::Accepted);
				if (Result!=EHALRoomResult::Accepted) { return true; }
				Room->AddMember(PC); Room->AddMember(RemotePC); bInitialized=true;
			}
			if (!Room->IsEnabled() || PS->GetRoomData().Phase==EHALRoomPlayerPhase::Unassigned) { return false; }
			const auto Data = PS->GetRoomData();
			auto* Movement = Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>();
			const auto Physics = Movement ? Movement->GetMovementEffectStatus() : FHALMovementStatus();
			if (Data.Phase==EHALRoomPlayerPhase::Ready && Room->GetData().Phase==EHALRoomPhase::Open
				&& Physics.bReady && !Physics.Value.bControlsBlocked) { bSawReadyPhysics=true; }
			const double Now = FPlatformTime::Seconds();
			if (Room->GetData().Phase==EHALRoomPhase::Open && Now>=NextRequest)
			{
				NextRequest=Now+.25;
				if (Data.Phase==EHALRoomPlayerPhase::Selecting)
				{
					if (Data.VehicleId.IsNone()) { PC->SelectRoomVehicle(TEXT("ProbeVehicle")); }
					else { PC->ConfirmRoomVehicle(); }
				}
				if (!Host && Data.Phase==EHALRoomPlayerPhase::Ready && bSawReadyPhysics && !bDenied)
				{
					if (PC->GetLastRoomResult()==EHALRoomResult::NotHost)
					{ bDenied=true; PC->SetName(TEXT("GF1_ClientDenied")); }
					else { PC->RequestStartMatch(); }
				}
			}
			if (Host && Room->GetData().Phase==EHALRoomPhase::Open)
			{
				for (auto* Controller : { PC,RemotePC })
				{
					if (Controller && Controller->GetPlayerState<AHALPlayerState>()->GetRoomData().Phase==EHALRoomPlayerPhase::PreparingAvatar)
					{ Room->CompleteAvatar(Controller,Controller->GetPlayerState<AHALPlayerState>()->GetRoomData().SelectionRevision); }
				}
				if (Other->GetPlayerName()==TEXT("GF1_ClientDenied") && bSawReadyPhysics && Room->CheckStart(PC)==EHALRoomResult::Accepted) { PC->RequestStartMatch(); }
			}
			if (Room->GetData().Phase!=EHALRoomPhase::Locked || Room->GetData().LockedRoster.Num()!=2) { return false; }
			if (!Movement || !Movement->GetMovementEffectStatus().bReady || !Movement->GetMovementEffectStatus().Value.bControlsBlocked) { return false; }
			Test->TestTrue(TEXT("Ready actually unblocked historical physics before lock"),bSawReadyPhysics);
			Test->TestEqual(TEXT("Round identity replicated"),Room->GetData().RoundId,1u);
			Test->TestTrue(TEXT("Same vehicle, distinct identities"),Data.VehicleId==Other->GetRoomData().VehicleId && Data.Seat!=Other->GetRoomData().Seat && Data.Color!=Other->GetRoomData().Color);
			Test->TestFalse(TEXT("Final locked permissions deny control"),HALGameFlow::Permissions(World,Pawn).bDrive);
			Test->TestTrue(TEXT("No extra client Ready button/request"),Data.Phase==EHALRoomPlayerPhase::Ready);
			if (!Host)
			{
				Test->TestTrue(TEXT("Nonhost start rejected through real owned RPC"),bDenied);
				Test->TestEqual(TEXT("Client cannot directly change room"),Room->Request(PC,EHALRoomAction::CancelConfirmation,NAME_None,Data.SelectionRevision),EHALRoomResult::NotAuthority);
				Test->TestEqual(TEXT("Client cannot advance phase"),Room->AdvanceRound(1,EHALRoomPhase::Playing),EHALRoomResult::NotAuthority);
			}
			UE_LOG(LogTemp,Display,TEXT("HAL GF1 probe: Host=%d Members=%d Round=%u Phase=%d OwnSeat=%d OtherSeat=%d SameVehicle=1 ControlsBlocked=%d Revision=%u"),
				Host,Room->GetData().Members.Num(),Room->GetData().RoundId,int32(Room->GetData().Phase),Data.Seat,Other->GetRoomData().Seat,
				Movement->GetMovementEffectStatus().Value.bControlsBlocked,Movement->GetMovementEffectStatus().Value.Revision);
			Complete=Now; return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoomNetworkTest,"HAL.NetworkSmoke.GameFlow.GF1",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALRoomNetworkTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRoomNetworkProbe>(this)); return true; }
#endif
