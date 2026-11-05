#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALRejoinTestController.h"
#include "../HALPracticeRoomGameMode.h"
#include "../HALRoomComponent.h"
#include "../HALRoomConfig.h"
#include "../HALPlayerState.h"
#include "../HALPlayerStart.h"
#include "../HALMatchGameState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

struct FHALRejoinTestAccess
{
	static void Returned(AHALPracticeRoomGameMode* M,const FHALRoundTravel& T)
	{ M->Travel=T; M->bRestoringTravel=true; M->bRoundWorld=false; M->GetGameState<AHALMatchGameState>()->GetRoom()->RestoreTravel(T); }
};
namespace
{
	struct FReturnedRoom
	{
		UWorld* W; AHALPracticeRoomGameMode* Mode; UHALRoomComponent* R;
		TArray<AHALRejoinTestController*> Old;
		FHALRoundTravel Ticket;
		explicit FReturnedRoom(int32 Count)
		{
			const auto V=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&V);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W); W->SetGameInstance(NewObject<UGameInstance>(GEngine));
			auto* Saved=LoadObject<UHALRoomConfig>(nullptr,TEXT("/Game/Data/Gameflow/DA_Room_Development.DA_Room_Development"));
			auto* Config=DuplicateObject<UHALRoomConfig>(Saved,W->GetGameInstance()); Config->bEnableRoundTravel=false;
			FURL URL; URL.AddOption(TEXT("game=/Script/HAL_Future_Creation.HALPracticeRoomGameMode"));
			URL.AddOption(*FString::Printf(TEXT("RoomConfig=%s"),*Config->GetPathName()));
			W->SetGameMode(URL); W->InitializeActorsForPlay(URL);
			Mode=W->GetAuthGameMode<AHALPracticeRoomGameMode>(); R=W->GetGameState<AHALMatchGameState>()->GetRoom();
			for (int32 I=0;I<4;++I) { auto* S=W->SpawnActor<AHALPlayerStart>(FVector(I*1800,0,1000),FRotator::ZeroRotator); S->SpawnSlot=I; }
			Ticket.Config=Config; Ticket.FlowId=R->GetData().FlowId; Ticket.RoundId=1; Ticket.bToPractice=true;
			for (int32 I=0;I<Count;++I)
			{
				auto* PC=Fresh(); R->AddMember(PC); Old.Add(PC);
				FHALTravelPlayer P; P.Identity=PC->GetPlayerState<AHALPlayerState>()->GetRoomData(); Ticket.Players.Add(P);
			}
			Ticket.HostKey=Ticket.Players[0].Identity.PlayerKey; FHALRejoinTestAccess::Returned(Mode,Ticket);
		}
		AHALRejoinTestController* Fresh()
		{ auto* PC=W->SpawnActor<AHALRejoinTestController>(); PC->SetAsLocalPlayerController(); return PC; }
		void Carry(AHALRejoinTestController* PC)
		{ AController* C=PC; Mode->HandleSeamlessTravelPlayer(C); }
		~FReturnedRoom() { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); }
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALReturnedRoomAdmissionTest,"HAL.FutureCreation.GameFlow.GF7.ReturnedRoomOrdinaryJoinAndDelayedCarry",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALReturnedRoomAdmissionTest::RunTest(const FString&)
{
	FReturnedRoom F(2); F.Carry(F.Old[0]);
	TestTrue(TEXT("Actual seamless callback restores the host"),F.R->GetData().Host==F.Old[0]->GetPlayerState<AHALPlayerState>());
	auto* New=F.Fresh();
	// Exactly the engine event that ordinary Super::PostLogin fires before AddMember.
	F.Mode->HandleStartingNewPlayer_Implementation(New);
	TestTrue(TEXT("Fresh login is not sent an old-identity rejection"),New->ReturnReason.IsEmpty());
	TestEqual(TEXT("Returned Open room accepts ordinary registration"),F.R->AddMember(New),EHALRoomResult::Accepted);
	auto* PS=New->GetPlayerState<AHALPlayerState>(); const auto NewKey=PS->GetRoomData().PlayerKey;
	TestEqual(TEXT("New player cannot steal the loading old player's seat"),PS->GetRoomData().Seat,2);
	F.Carry(F.Old[1]);
	TestTrue(TEXT("Late genuine seamless callback still restores identity"),F.Old[1]->ReturnReason.IsEmpty());
	TestEqual(TEXT("Late player retains seat"),F.Old[1]->GetPlayerState<AHALPlayerState>()->GetRoomData().Seat,1);
	TestEqual(TEXT("Both carried players and new player coexist"),F.R->GetData().Members.Num(),3);
	F.R->RemoveMember(New); New->Destroy();
	auto* Again=F.Fresh(); F.Mode->HandleStartingNewPlayer_Implementation(Again);
	TestTrue(TEXT("Repeated normal login does not re-enter restore context"),Again->ReturnReason.IsEmpty());
	TestEqual(TEXT("New registration remains available"),F.R->AddMember(Again),EHALRoomResult::Accepted);
	TestTrue(TEXT("Rejoin allocates a fresh identity"),Again->GetPlayerState<AHALPlayerState>()->GetRoomData().PlayerKey!=NewKey);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALReturnedRoomCapacityTest,"HAL.FutureCreation.GameFlow.GF7.ReturnedRoomPendingSeatsAndAdmissionGates",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALReturnedRoomCapacityTest::RunTest(const FString&)
{
	FReturnedRoom F(4); F.Carry(F.Old[0]);
	TestFalse(TEXT("Three loading peers count toward room capacity"),F.R->CanAdmitPlayer());
	F.R->RemoveMember(F.Old[1]); F.Old[1]->Destroy();
	TestTrue(TEXT("Disconnect releases a not-yet-imported reservation"),F.R->CanAdmitPlayer());
	auto* New=F.Fresh(); F.Mode->HandleStartingNewPlayer_Implementation(New);
	TestEqual(TEXT("Freed seat accepts a new player"),F.R->AddMember(New),EHALRoomResult::Accepted);
	TestEqual(TEXT("Only the departed player's seat is reused"),New->GetPlayerState<AHALPlayerState>()->GetRoomData().Seat,1);
	F.Old[3]->Destroy(); F.R->PruneTravelReservations();
	TestTrue(TEXT("Snapshot departure without an import cannot strand capacity"),F.R->CanAdmitPlayer());
	F.Carry(F.Old[2]);
	F.R->CloseForReturn();
	FString Error; F.Mode->PreLogin(TEXT(""),TEXT("127.0.0.1"),FUniqueNetIdRepl(),Error);
	TestFalse(TEXT("Admission remains closed after phase lock"),Error.IsEmpty());
	return true;
}
#endif
