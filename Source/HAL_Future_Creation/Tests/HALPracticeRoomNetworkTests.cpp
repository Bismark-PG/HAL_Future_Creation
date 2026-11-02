#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../HALPracticeRoomGameMode.h"
#include "../HALRoomComponent.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../HALNetworkGameInstance.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALGameplayTags.h"
#include "../HALVehicleCatalog.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../TestVehiclePawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	/** Real server-configured GF-3 GameMode on existing map; never substitutes direct Possess/Room initialization. */
	class FPracticeNetworkProbe : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Deadline=FPlatformTime::Seconds()+120, NextRequest=0, Complete=-1, ExitAfter=-1;
		int32 Step=0;
		bool bHost=false,bDenied=false,bInvalidDenied=false,bVerified=false,bSkill=false,bClientExit=false,bSawOther=false;
		FGuid FirstEpoch;
		TWeakObjectPtr<APawn> FirstPawn;
	public:
		explicit FPracticeNetworkProbe(FAutomationTestBase* T,bool ClientExit=false):Test(T),bClientExit(ClientExit) {}
		virtual bool Update() override
		{
			const double Now=FPlatformTime::Seconds();
			if (Complete>0) { return Now-Complete>2; }
			if (Now>Deadline) { Test->AddError(FString::Printf(TEXT("GF-3 network timeout: Host=%d Step=%d Verified=%d"),bHost,Step,bVerified)); return true; }
			UWorld* W=nullptr;
			for (const auto& C:GEngine->GetWorldContexts()) { if (auto* V=C.World(); V && V->IsGameWorld()) { W=V; break; } }
			if (!W) { return false; }
			if (bVerified && W->GetNetMode()==NM_Standalone && W->GetMapName().Contains(TEXT("MVPB_NetEntry")))
			{
				const auto* GI=W->GetGameInstance<UHALNetworkGameInstance>();
				const TCHAR* Reason=bClientExit && !bHost ? TEXT("Left the room") : TEXT("Host closed");
				Test->TestTrue(TEXT("Exit returned to entry with correct reason"),GI && GI->GetConnectionMessage().ToString().Contains(Reason));
				UE_LOG(LogTemp,Display,TEXT("HAL GF3 probe: Host=%d ClientExit=%d Verified=1 Entry=1"),bHost,bClientExit);
				Complete=Now; return false;
			}
			if (W->GetNetMode()!=NM_ListenServer && W->GetNetMode()!=NM_Client) { return false; }
			bHost=W->GetNetMode()==NM_ListenServer;
			auto* S=W->GetGameState<AHALMatchGameState>(); auto* R=S ? S->GetRoom() : nullptr;
			AHALPlayerController* PC=nullptr;
			for (auto It=W->GetPlayerControllerIterator(); It; ++It) { if (auto* P=Cast<AHALPlayerController>(It->Get()); P && P->IsLocalController()) { PC=P; break; } }
			auto* PS=PC ? PC->GetPlayerState<AHALPlayerState>() : nullptr;
			if (!PS || !R || !R->GetData().bPracticeRoom || PS->GetRoomData().Phase==EHALRoomPlayerPhase::Unassigned) { return false; }
			AHALPlayerState* Other=nullptr;
			for (const auto& P:R->GetData().Members) { if (P && P!=PS) { Other=P; break; } }
			if (Other) { bSawOther=true; }
			const auto P=PS->GetRoomData(); auto* Pawn=Cast<ATestVehiclePawn>(PC->GetPawn()); auto* ASC=PS->GetHALAbilitySystemComponent();
			if (P.AvatarResult==EHALRoomResult::SpawnBlocked || P.AvatarResult==EHALRoomResult::SpawnFailed || P.AvatarResult==EHALRoomResult::MissingSpawn)
			{ Test->AddError(FString::Printf(TEXT("GF-3 actual authored spawn rejected: Seat=%d Result=%d"),P.Seat,int32(P.AvatarResult))); return true; }
			auto* M=Pawn ? Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>() : nullptr;
			const auto Physics=M ? M->GetMovementEffectStatus() : FHALMovementStatus();
			const bool Ready=Pawn && P.ReadyAvatar==Pawn && ASC->GetAvatarActor()==Pawn && Pawn->GetPlayerState()==PS
				&& P.Phase==EHALRoomPlayerPhase::Ready && Physics.bReady && !Physics.Value.bControlsBlocked;
			if (!Step)
			{
				Test->TestNull(TEXT("Dedicated mode does not auto-spawn a default car"),PC->GetPawn()); Step=1;
			}
			if (bVerified)
			{
				if (bHost && ExitAfter>0 && Now>=ExitAfter) { PC->LeaveRoom(); ExitAfter=-1; }
				return false;
			}
			if (R->GetData().Phase==EHALRoomPhase::Locked)
			{
				if (!Pawn || !Physics.bReady || !Physics.Value.bControlsBlocked || !Other) { return false; }
				Test->TestTrue(TEXT("Final mixed models retain independent ownership"),P.VehicleId!=Other->GetRoomData().VehicleId && P.Seat!=Other->GetRoomData().Seat && P.Color!=Other->GetRoomData().Color);
				Test->TestEqual(TEXT("One frozen two-player roster"),R->GetData().LockedRoster.Num(),2);
				Test->TestTrue(TEXT("Changed Avatar has fresh physics epoch"),FirstEpoch.IsValid() && Physics.Value.Epoch!=FirstEpoch && Pawn!=FirstPawn.Get());
				Test->TestEqual(TEXT("No default spawn or duplicate lifecycle commits"),PS->GetVehicleLifecycle().ChangeSerial,bHost?3u:2u);
				if (!bHost) { Test->TestTrue(TEXT("Server rejected foreign ID and nonhost Start"),bInvalidDenied && bDenied); PC->SetName(TEXT("GF3_LOCKED")); }
				bVerified=true; if (bHost) { ExitAfter=Now+2; } return false;
			}
			if (Now<NextRequest || PC->IsRoomRequestPending()) { return false; }
			NextRequest=Now+.15;
			if (Step==1)
			{
				if (!bHost && !bInvalidDenied)
				{
					if (PC->GetLastRoomResult()==EHALRoomResult::InvalidVehicle) { bInvalidDenied=true; }
					else { PC->SelectRoomVehicle(TEXT("NotInCatalog")); return false; }
				}
				if (P.VehicleId.IsNone()) { PC->SelectRoomVehicle(TEXT("Car_01")); }
				else if (P.Phase==EHALRoomPlayerPhase::Selecting) { PC->ConfirmRoomVehicle(); PC->ConfirmRoomVehicle(); }
				else if (Ready)
				{
					FirstPawn=Pawn; FirstEpoch=Physics.Value.Epoch;
					if (!bHost && !bDenied)
					{
						if (PC->GetLastRoomResult()==EHALRoomResult::NotHost) { bDenied=true; }
						else { PC->RequestStartMatch(); return false; }
					}
					PC->SetName(TEXT("GF3_READY_A")); Step=2;
				}
				return false;
			}
			if (Step==2)
			{
				if (bClientExit)
				{
					if (!bHost && Other && Other->GetPlayerName()==TEXT("GF3_READY_A") && Ready)
					{ Test->TestTrue(TEXT("Client leaves while both are practicing"),P.VehicleId==Other->GetRoomData().VehicleId); bVerified=true; PC->LeaveRoom(); }
					if (bHost && bSawOther && !Other && R->GetData().Members.Num()==1 && Ready)
					{
						Test->TestTrue(TEXT("Client exit frees its seat and keeps room open"),R->CanAdmitPlayer() && R->CheckStart(PC)==EHALRoomResult::Accepted);
						bVerified=true; PC->LeaveRoom();
					}
					return false;
				}
				if (!Other) { return false; }
				if (bHost && Other->GetPlayerName()!=TEXT("GF3_READY_A")) { return false; }
				if (!bHost && Other->GetRoomData().VehicleId!=TEXT("Car_02")) { return false; }
				if (P.Phase==EHALRoomPlayerPhase::Ready) { PC->CancelRoomConfirmation(); }
				else if (P.Phase==EHALRoomPlayerPhase::Selecting && !PC->GetPawn())
				{
					Test->TestNull(TEXT("Cancelled Avatar unbound from local ASC"),ASC->GetAvatarActor());
					if (P.VehicleId!=TEXT("Car_02")) { PC->SelectRoomVehicle(TEXT("Car_02")); }
					else { PC->ConfirmRoomVehicle(); Step=3; }
				}
				return false;
			}
			if (Step==3 && Ready)
			{
				Test->TestTrue(TEXT("Car_02 is actual catalog class, not just a UI ID"),Pawn->GetClass()->GetName().Contains(TEXT("Car_02")));
				if (!bHost && !bSkill)
				{
					if (ASC->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune)) { bSkill=true; }
					else { PC->HALUseSkill(); return false; }
				}
				PC->SetName(TEXT("GF3_READY_B")); Step=4; return false;
			}
			if (Step==4)
			{
				if (!bHost) { return false; }
				if (!Other || Other->GetPlayerName()!=TEXT("GF3_READY_B")) { return false; }
				if (P.Phase==EHALRoomPlayerPhase::Ready && P.VehicleId==TEXT("Car_02")) { PC->CancelRoomConfirmation(); }
				else if (P.Phase==EHALRoomPlayerPhase::Selecting)
				{
					if (P.VehicleId!=TEXT("Car_01")) { PC->SelectRoomVehicle(TEXT("Car_01")); }
					else { PC->ConfirmRoomVehicle(); }
				}
				else if (Ready && P.VehicleId==TEXT("Car_01") && R->CheckStart(PC)==EHALRoomResult::Accepted) { PC->RequestStartMatch(); }
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALPracticeRoomNetworkTest,"HAL.NetworkSmoke.GameFlow.GF3.Flow",
	EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALPracticeRoomNetworkTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FPracticeNetworkProbe>(this)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALPracticeRoomClientExitTest,"HAL.NetworkSmoke.GameFlow.GF3.ClientExit",
	EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALPracticeRoomClientExitTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FPracticeNetworkProbe>(this,true)); return true; }
#endif
