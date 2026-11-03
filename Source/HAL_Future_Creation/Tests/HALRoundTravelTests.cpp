#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALRoomTestSupport.h"
#include "../HALRoundTravelTypes.h"
#include "../HALPlayerState.h"
#include "../HALPlayerController.h"
#include "../HALMatchGameState.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALGameFlowPermissions.h"
#include "../HALPracticeRoomGameMode.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

struct FHALPracticeRoomTestAccess
{
	static void Setup(AHALPracticeRoomGameMode* Mode,UWorld* World,bool Round,bool OptIn)
	{ Mode->GameState=World->GetGameState(); Mode->bRoundWorld=Round; Mode->bAllowTestRoundEnd=OptIn; }
};

namespace
{
	struct FTravelWorld
	{
		UWorld* W; UHALRoomComponent* Room; TArray<AHALPlayerController*> PCs;
		FTravelWorld(int32 Players=2,EHALRoomProfile Profile=EHALRoomProfile::Development)
		{
			const auto V=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&V);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
			auto* S=W->SpawnActor<AHALMatchGameState>(); W->SetGameState(S); Room=S->GetRoom();
			Room->Initialize(FHALRoomTestAccess::Config(Room,Profile)); Room->EnablePracticeRoom();
			for (int32 I=0;I<Players;++I)
			{
				auto* PC=W->SpawnActor<AHALPlayerController>(); PC->SetAsLocalPlayerController();
				auto* PS=W->SpawnActor<AHALPlayerState>(); PS->SetOwner(PC); PC->SetPlayerState(PS);
				PS->PreInitializeComponents(); PS->InitializeComponents(); PS->PostInitializeComponents(); PS->DispatchBeginPlay();
				Room->AddMember(PC); Room->Request(PC,EHALRoomAction::SelectVehicle,TEXT("Car_01"),PS->GetRoomData().SelectionRevision); PCs.Add(PC);
			}
		}
		FHALRoundTravel Ticket()
		{
			FHALRoundTravel T; T.Config=const_cast<UHALRoomConfig*>(Room->GetServerConfig()); T.FlowId=Room->GetData().FlowId;
			T.RoundId=1; T.HostKey=PCs[0]->GetPlayerState<AHALPlayerState>()->GetRoomData().PlayerKey;
			for (auto* PC:PCs) { FHALTravelPlayer P; P.Identity=PC->GetPlayerState<AHALPlayerState>()->GetRoomData(); T.Players.Add(P); }
			return T;
		}
		ATestVehiclePawn* Spawn(AHALPlayerController* PC)
		{
			auto* Class=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Pawn=W->SpawnActor<ATestVehiclePawn>(Class,FVector(0,0,1000),FRotator::ZeroRotator,P);
			Pawn->PreInitializeComponents(); Pawn->InitializeComponents(); Pawn->PostInitializeComponents();
			auto* PS=PC->GetPlayerState<AHALPlayerState>(); PS->BeginNewRoundAvatar(Pawn,1,1); Pawn->DispatchBeginPlay();
			Room->CompleteAvatar(PC,PS->GetRoomData().SelectionRevision,1); return Pawn;
		}
		~FTravelWorld() { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); }
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoundReturnGateTest,"HAL.FutureCreation.GameFlow.GF6.RoundReturnAuthorityAndOptIn",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALRoundReturnGateTest::RunTest(const FString&)
{
	for (const auto Profile:{EHALRoomProfile::Development,EHALRoomProfile::Exhibition})
	{
		FTravelWorld F(4,Profile); const auto T=F.Ticket(); F.Room->RestoreTravel(T);
		for (int32 I=0;I<4;++I) { F.Room->ImportTravelMember(F.PCs[I],T.Players[I],T.HostKey); }
		auto* Mode=F.W->SpawnActor<AHALPracticeRoomGameMode>(); FHALPracticeRoomTestAccess::Setup(Mode,F.W,true,true);
		TestFalse(TEXT("Loading cannot be ended by test UI"),Mode->CanRequestRoundReturn());
		TestEqual(TEXT("Trusted server call also respects phase"),Mode->ReturnActiveRoundToRoom(),EHALRoomResult::WrongPhase);
		F.Room->AdvanceRound(1,EHALRoomPhase::Countdown); F.Room->AdvanceRound(1,EHALRoomPhase::Playing);
		FHALPracticeRoomTestAccess::Setup(Mode,F.W,true,false);
		TestEqual(TEXT("Exhibition defaults closed, Development retains existing control"),Mode->CanRequestRoundReturn(),Profile==EHALRoomProfile::Development);
		auto* Host=F.PCs[0]; const uint32 Revision=Host->GetPlayerState<AHALPlayerState>()->GetRoomData().SelectionRevision;
		if (Profile==EHALRoomProfile::Exhibition)
		{ TestEqual(TEXT("Unopted host request rejected"),Mode->HandleRoomAction(Host,EHALRoomAction::ReturnToRoom,NAME_None,Revision),EHALRoomResult::WrongPhase); }
		FHALPracticeRoomTestAccess::Setup(Mode,F.W,true,true);
		TestTrue(TEXT("Non-Shipping server opt-in enables test entry"),Mode->CanRequestRoundReturn());
		TestEqual(TEXT("Client cannot end another player's round"),Mode->HandleRoomAction(F.PCs[1],EHALRoomAction::ReturnToRoom,NAME_None,Revision),EHALRoomResult::NotHost);
		TestEqual(TEXT("Null requester rejected"),Mode->HandleRoomAction(nullptr,EHALRoomAction::ReturnToRoom,NAME_None,Revision),EHALRoomResult::NotHost);
		TestEqual(TEXT("No arbitrary vehicle/result payload accepted"),Mode->HandleRoomAction(Host,EHALRoomAction::ReturnToRoom,TEXT("Car_01"),Revision),EHALRoomResult::InvalidRequest);
		TestEqual(TEXT("Old selection request rejected"),Mode->HandleRoomAction(Host,EHALRoomAction::ReturnToRoom,NAME_None,Revision+1),EHALRoomResult::StaleSelection);
		TestEqual(TEXT("Missing handoff configuration fails without closing room"),Mode->ReturnActiveRoundToRoom(),EHALRoomResult::InvalidConfiguration);
		TestEqual(TEXT("Failed request leaves active phase intact"),F.Room->GetData().Phase,EHALRoomPhase::Playing);
		FHALPracticeRoomTestAccess::Setup(Mode,F.W,false,true);
		TestFalse(TEXT("Practice world cannot masquerade as a round"),Mode->CanRequestRoundReturn());
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoundConfigTest,"HAL.FutureCreation.GameFlow.GF4.ConfigAndReferenceFreeEnvelope",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALRoundConfigTest::RunTest(const FString&)
{
	FTravelWorld F; auto T=F.Ticket(); FString E;
	TestTrue(TEXT("Copied values and saved config form a valid handoff"),T.IsValid());
	const auto Player=T.Players[0]; T.Players.Add(Player); TestFalse(TEXT("Duplicate identity/seat rejected"),T.IsValid()); T.Players.Pop();
	T.HostKey=FGuid::NewGuid(); TestFalse(TEXT("No replacement host or missing original host"),T.IsValid()); T.HostKey=Player.Identity.PlayerKey;
	T.Players[0].Identity.LoadToken=FGuid::NewGuid(); TestFalse(TEXT("Old load token never crosses worlds"),T.IsValid()); T.Players[0].Identity.LoadToken.Invalidate();
	T.Config->bEnableRoundTravel=true; TestFalse(TEXT("Enabled travel requires target map"),T.Config->Validate(E));
	T.Config->MatchMap=TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/MVPB_NetTest.MVPB_NetTest")));
	TestTrue(TEXT("Explicit defaults valid"),T.Config->Validate(E));
	T.Config->CountdownSeconds=20; TestFalse(TEXT("Countdown range validated in code"),T.Config->Validate(E)); T.Config->CountdownSeconds=3;
	T.Config->LoadTimeoutSeconds=0; TestFalse(TEXT("No unbounded zero timeout"),T.Config->Validate(E));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoundBarrierTest,"HAL.FutureCreation.GameFlow.GF4.BarrierIdentityAndCountdown",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALRoundBarrierTest::RunTest(const FString&)
{
	FTravelWorld F; auto T=F.Ticket(); F.Room->RestoreTravel(T);
	TArray<ATestVehiclePawn*> Pawns;
	for (int32 I=0;I<2;++I)
	{
		TestTrue(TEXT("Matching copied player imports"),F.Room->ImportTravelMember(F.PCs[I],T.Players[I],T.HostKey));
		Pawns.Add(F.Spawn(F.PCs[I]));
		TestFalse(TEXT("Loading blocks driving"),HALGameFlow::Permissions(F.W,Pawns.Last()).bDrive);
	}
	auto* Host=F.PCs[0]; auto* PS=Host->GetPlayerState<AHALPlayerState>(); const auto P=PS->GetRoomData();
	TestTrue(TEXT("New round produces a per-Avatar load token"),P.LoadToken.IsValid());
	TestEqual(TEXT("Old flow rejected"),F.Room->AcknowledgeLoaded(Host,FGuid::NewGuid(),1,P.LoadToken,Pawns[0]),EHALRoomResult::WrongPhase);
	TestEqual(TEXT("Old round rejected"),F.Room->AcknowledgeLoaded(Host,T.FlowId,0,P.LoadToken,Pawns[0]),EHALRoomResult::WrongPhase);
	TestEqual(TEXT("Wrong Avatar rejected"),F.Room->AcknowledgeLoaded(Host,T.FlowId,1,P.LoadToken,Pawns[1]),EHALRoomResult::NotReady);
	TestEqual(TEXT("Wrong token rejected"),F.Room->AcknowledgeLoaded(Host,T.FlowId,1,FGuid::NewGuid(),Pawns[0]),EHALRoomResult::NotReady);
	TestEqual(TEXT("Actual host initialization accepted"),F.Room->AcknowledgeLoaded(Host,T.FlowId,1,P.LoadToken,Pawns[0]),EHALRoomResult::Accepted);
	TestFalse(TEXT("Host alone cannot start while client not ready"),F.Room->AllClientsLoaded());
	F.Room->SetCountdown(3); TestEqual(TEXT("Early countdown denied"),F.Room->GetData().Phase,EHALRoomPhase::Loading);
	auto* Other=F.PCs[1];
	F.Room->AcknowledgeLoaded(Other,T.FlowId,1,Other->GetPlayerState<AHALPlayerState>()->GetRoomData().LoadToken,Pawns[1]);
	TestTrue(TEXT("All members and actual Avatars checked"),F.Room->AllClientsLoaded());
	F.Room->SetCountdown(3); TestEqual(TEXT("One shared countdown end time"),F.Room->GetData().CountdownEndServerTime,3.0);
	TestFalse(TEXT("Countdown keeps controls blocked"),HALGameFlow::Permissions(F.W,Pawns[0]).bDrive);
	TestEqual(TEXT("Late duplicate ready cannot alter countdown"),F.Room->AcknowledgeLoaded(Host,T.FlowId,1,P.LoadToken,Pawns[0]),EHALRoomResult::WrongPhase);
	F.Room->AdvanceRound(1,EHALRoomPhase::Playing); TestTrue(TEXT("Playing opens driving and HP damage"),HALGameFlow::Permissions(F.W,Pawns[0]).bDrive && HALGameFlow::Permissions(F.W,Pawns[0]).bDamage);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoundCopyTest,"HAL.FutureCreation.GameFlow.GF4.CopyIdentityAndReturnUnconfirmed",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALRoundCopyTest::RunTest(const FString&)
{
	FTravelWorld F; auto* PS=F.PCs[0]->GetPlayerState<AHALPlayerState>(); PS->RestoreTravelCounters(8,9,3);
	auto* Copy=F.W->SpawnActor<AHALPlayerState>(); PS->CopyProperties(Copy);
	TestEqual(TEXT("Engine PlayerState replacement preserves room identity"),Copy->GetRoomData().PlayerKey,PS->GetRoomData().PlayerKey);
	TestEqual(TEXT("Serial preserved across Worlds"),Copy->GetVehicleLifecycle().ChangeSerial,8u);
	TestEqual(TEXT("Round preserved across Worlds"),Copy->GetVehicleLifecycle().RoundId,3u);
	TestNull(TEXT("Old Avatar never copied"),Copy->GetVehicleLifecycle().Avatar.Get());
	TestNull(TEXT("New ASC starts explicitly owner-only with no old Avatar"),Copy->GetHALAbilitySystemComponent()->GetAvatarActor());
	auto T=F.Ticket(); T.bToPractice=true; T.RoundId=3; T.Players[0].ChangeSerial=8; T.Players[0].LifecycleRevision=9; T.Players[0].PreviousRound=3;
	F.Room->RestoreTravel(T); F.Room->ImportTravelMember(F.PCs[0],T.Players[0],T.HostKey);
	TestEqual(TEXT("Return clears confirmation"),PS->GetRoomData().Phase,EHALRoomPlayerPhase::Selecting);
	TestEqual(TEXT("Next start will increment from previous round"),F.Room->GetData().RoundId,3u);
	TestTrue(TEXT("Old selection messages become stale"),PS->GetRoomData().SelectionRevision>T.Players[0].Identity.SelectionRevision);
	return true;
}
#endif
