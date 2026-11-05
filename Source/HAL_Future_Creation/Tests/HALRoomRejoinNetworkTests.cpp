#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../HALPracticeRoomGameMode.h"
#include "../HALRoomComponent.h"
#include "../HALRoomConfig.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerState.h"
#include "../HALPlayerController.h"
#include "../HALNetworkGameInstance.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALHealthAttributeSet.h"
#include "../TestVehiclePawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "Misc/CommandLine.h"
#include "UObject/UnrealType.h"

namespace
{
	/** Same process leaves Results and uses the production JoinMatch again. A second
	 * client stays across travel; the runner introduces a fresh fourth player on return. */
	class FRoomRejoinProbe : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		FString Role,SentName;
		bool Host=false,Returning=false,Fresh=false,Joined=false,LeftResults=false,Rejoined=false,Carried=false,Returned=false,Marked=false,HostSawExit=false;
		uint32 ConfiguredWorld=0,SeenPlaying=0,SeenResults=0;
		FGuid InitialKey; int32 InitialSeat=INDEX_NONE;
		double Deadline=FPlatformTime::Seconds()+190,Next=0,NameAt=-1,RejoinAt=0,CompleteAt=-1;
		void Name(AHALPlayerController* PC,const FString& Value)
		{
			const double Now=FPlatformTime::Seconds();
			if (Value!=SentName || Now-NameAt>.5) { SentName=Value; NameAt=Now; PC->SetName(Value); }
		}
	public:
		explicit FRoomRejoinProbe(FAutomationTestBase* T):Test(T)
		{
			FParse::Value(FCommandLine::Get(),TEXT("GF7RejoinRole="),Role);
			Host=Role==TEXT("Host"); Returning=Role==TEXT("Returning"); Fresh=Role==TEXT("Fresh");
		}
		virtual bool Update() override
		{
			const double Now=FPlatformTime::Seconds();
			if (CompleteAt>0) { return Now>=CompleteAt; }
			if (Now>Deadline) { Test->AddError(FString::Printf(TEXT("Rejoin timeout Role=%s Played=%u Results=%u Left=%d Joined=%d Rejoined=%d Returned=%d"),*Role,SeenPlaying,SeenResults,LeftResults,Joined,Rejoined,Returned)); return true; }
			UWorld* W=nullptr;
			for (const auto& C:GEngine->GetWorldContexts()) { if (C.World() && C.World()->IsGameWorld()) { W=C.World(); break; } }
			if (!W) { return false; }
			auto* GI=W->GetGameInstance<UHALNetworkGameInstance>();
			if (W->GetMapName().Contains(TEXT("MVPB_NetEntry")))
			{
				if (Returned)
				{
					Test->TestTrue(TEXT("Final real match and return observed"),SeenPlaying==2 && SeenResults==2);
					if (Returning) { Test->TestTrue(TEXT("Original process rejoined as a new identity"),LeftResults && Rejoined); }
					if (Role==TEXT("Staying")) { Test->TestTrue(TEXT("Old peer retained identity through both maps"),Carried); }
					UE_LOG(LogTemp,Display,TEXT("GF7 rejoin complete Role=%s Played=%u Results=%u Rejoined=%d Carried=%d Entry=1"),*Role,SeenPlaying,SeenResults,Rejoined,Carried);
					CompleteAt=Now+1; return false;
				}
				if (Joined && GI && !GI->IsConnecting())
				{ Test->AddError(FString::Printf(TEXT("Production rejoin returned to Entry: %s"),*GI->GetConnectionMessage().ToString())); return true; }
				if ((Fresh || (Returning && LeftResults && Now>=RejoinAt)) && !Joined && GI && GI->GetFirstLocalPlayerController())
				{ Test->TestTrue(TEXT("Production IPv4 Join accepted locally"),GI->JoinMatch(TEXT("127.0.0.1"))); Joined=true; }
				return false;
			}
			auto* S=W->GetGameState<AHALMatchGameState>(); auto* R=S?S->GetRoom():nullptr;
			if (!R || !R->GetData().bPracticeRoom) { return false; }
			AHALPlayerController* PC=nullptr;
			for (auto It=W->GetPlayerControllerIterator();It;++It) { if (auto* P=Cast<AHALPlayerController>(It->Get());P && P->IsLocalController()) { PC=P; break; } }
			auto* PS=PC?PC->GetPlayerState<AHALPlayerState>():nullptr;
			if (!PS || (W->GetNetMode()==NM_Client && (!PC->GetLocalPlayer() || !PC->GetNetConnection()))) { return false; }
			const auto& D=R->GetData(); const auto I=PS->GetRoomData();
			if (I.Phase==EHALRoomPlayerPhase::Unassigned) { return false; }
			if (!InitialKey.IsValid()) { InitialKey=I.PlayerKey; InitialSeat=I.Seat; }
			if (Host && ConfiguredWorld!=W->GetUniqueID())
			{
				auto* M=W->GetAuthGameMode<AHALPracticeRoomGameMode>();
				auto* C=DuplicateObject<UHALRoomConfig>(R->GetServerConfig(),M); C->SetFlags(RF_Transient); C->ClearFlags(RF_Public|RF_Standalone);
				C->MatchRules.DurationSeconds=5; C->MatchRules.ResultsSeconds=4;
				FindFProperty<FObjectProperty>(M->GetClass(),TEXT("RoomConfig"))->SetObjectPropertyValue_InContainer(M,C);
				FPacketSimulationSettings Lag; Lag.PktLag=80; W->GetNetDriver()->SetPacketSimulationSettings(Lag);
				ConfiguredWorld=W->GetUniqueID();
			}
			const auto OthersNamed=[&](const FString& Value)
			{ for (const auto& P:D.Members) { if (P && P!=PS && P->GetPlayerName()!=Value) { return false; } } return true; };
			if (D.Phase==EHALRoomPhase::Open)
			{
				if (D.RoundId==1 && Host && !Marked)
				{ Test->TestTrue(TEXT("Host saw Results departure before automatic return"),HostSawExit); Marked=true;
					UE_LOG(LogTemp,Display,TEXT("GF7 rejoin returned room open Round=1 FreshPeerMayJoin=1")); }
				if (D.RoundId==1 && Returning && !Rejoined)
				{
					Test->TestTrue(TEXT("Rejoin produces a fresh server PlayerKey"),I.PlayerKey!=InitialKey);
					Test->TestEqual(TEXT("New connection must select rather than restore old readiness"),I.Phase,EHALRoomPlayerPhase::Selecting);
					Test->TestNull(TEXT("No old Avatar carried over an explicit disconnect"),PC->GetPawn());
					Rejoined=true;
				}
				if (D.RoundId && !Fresh && !Returning)
				{ Test->TestEqual(TEXT("Carried player's key preserved"),I.PlayerKey,InitialKey); Test->TestEqual(TEXT("Carried player's seat preserved"),I.Seat,InitialSeat); Carried=true; }
				if (D.RoundId==2)
				{
					// The roster and PlayerState identity can arrive in separate replication updates.
					if (D.Members.Num()!=4) { return false; }
					Test->TestEqual(TEXT("Return clears confirmation again"),I.Phase,EHALRoomPlayerPhase::Selecting);
					Test->TestEqual(TEXT("All four peers survive second real return"),D.Members.Num(),4);
					Returned=true;
				}
				if (Returned)
				{
					if (!Host) { Name(PC,TEXT("RJ_DONE")); }
					else if (D.Members.Num()==4 && OthersNamed(TEXT("RJ_DONE"))) { PC->LeaveRoom(); }
					return false;
				}
				if (Now<Next || PC->IsRoomRequestPending()) { return false; } Next=Now+.15;
				if (I.Phase==EHALRoomPlayerPhase::Selecting)
				{ if (I.VehicleId.IsNone()) { PC->SelectRoomVehicle(Returning || Fresh?TEXT("Car_02"):TEXT("Car_01")); } else { PC->ConfirmRoomVehicle(); } }
				else if (I.Phase==EHALRoomPlayerPhase::Ready && PC->GetPawn() && PS->AreVehicleAbilitiesReady(Cast<ATestVehiclePawn>(PC->GetPawn())))
				{
					const auto Ready=FString::Printf(TEXT("RJ_READY_%u"),D.RoundId); if (!Host) { Name(PC,Ready); }
					if (Host && D.Members.Num()==(D.RoundId==0?3:4) && OthersNamed(Ready) && R->CheckStart(PC)==EHALRoomResult::Accepted) { PC->RequestStartMatch(); }
				}
				return false;
			}
			if (D.Phase==EHALRoomPhase::Playing && SeenPlaying!=D.RoundId)
			{
				const auto* H=PS->GetHALAbilitySystemComponent()->GetSet<UHALHealthAttributeSet>();
				if (!H || !PC->GetPawn() || !FMath::IsNearlyEqual(H->GetCurrentHP(),H->GetMaxHP())) { return false; }
				Test->TestTrue(TEXT("Correct new-round source abilities"),PS->AreVehicleAbilitiesReady(Cast<ATestVehiclePawn>(PC->GetPawn())));
				SeenPlaying=D.RoundId;
			}
			if (D.Phase==EHALRoomPhase::Results)
			{
				if (SeenResults!=D.RoundId)
				{ Test->TestEqual(TEXT("Timed match ends normally"),D.Match.Reason,EHALMatchEndReason::TimeExpired); SeenResults=D.RoundId; }
				if (D.RoundId==1 && Host && D.Members.Num()==2) { HostSawExit=true; }
				if (D.RoundId==1 && Returning && !LeftResults)
				{
					RejoinAt=Now+FMath::Max(0.,D.Match.ReturnServerTime-S->GetServerWorldTimeSeconds())+1.;
					LeftResults=true; PC->LeaveRoom();
				}
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoomRejoinNetworkTest,"HAL.NetworkSmoke.GameFlow.GF7.ResultsLeaveRejoinAndNewPeer",
	EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALRoomRejoinNetworkTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRoomRejoinProbe>(this)); return true; }
#endif
