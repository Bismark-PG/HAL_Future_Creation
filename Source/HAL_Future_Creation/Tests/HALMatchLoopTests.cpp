#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include <limits>
#include "HALRoomTestSupport.h"
#include "../HALPracticeRoomGameMode.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALHealthAttributeSet.h"
#include "../HALGameFlowPermissions.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

struct FHALMatchLoopTestAccess
{
	static void Begin(AHALPracticeRoomGameMode* M,UWorld* W,UHALRoomConfig* C)
	{ M->GameState=W->GetGameState(); M->RoomConfig=C; M->bRoundWorld=true; M->BeginMatchRules(); }
	static void Evaluate(AHALPracticeRoomGameMode* M) { M->PollMatchRules(); }
	static void Expire(AHALPracticeRoomGameMode* M)
	{ M->ActiveMatch.EndServerTime=M->GetGameState<AHALMatchGameState>()->GetServerWorldTimeSeconds(); M->GetGameState<AHALMatchGameState>()->GetRoom()->SetMatchData(M->ActiveMatch); }
	static void Forfeit(AHALPracticeRoomGameMode* M,AHALPlayerState* P) { M->ForfeitMatchPlayer(P); }
	static void Stop(AHALPracticeRoomGameMode* M) { M->StopMatchRules(); }
};
namespace
{
	struct FMatchWorld
	{
		UWorld* W; UHALRoomComponent* R; AHALPracticeRoomGameMode* Mode; UHALRoomConfig* Config;
		TArray<AHALPlayerController*> PCs;
		FMatchWorld(int32 Count=4)
		{
			const auto V=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&V);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
			auto* S=W->SpawnActor<AHALMatchGameState>(); W->SetGameState(S); R=S->GetRoom();
			Config=FHALRoomTestAccess::Config(R,EHALRoomProfile::Development); R->Initialize(Config); R->EnablePracticeRoom();
			FHALRoundTravel T; T.Config=Config; T.FlowId=R->GetData().FlowId; T.RoundId=1;
			for (int32 I=0;I<Count;++I)
			{
				auto* PC=W->SpawnActor<AHALPlayerController>(); PC->SetAsLocalPlayerController(); PCs.Add(PC);
				auto* PS=W->SpawnActor<AHALPlayerState>(); PS->SetOwner(PC); PC->SetPlayerState(PS);
				PS->PreInitializeComponents(); PS->InitializeComponents(); PS->PostInitializeComponents(); PS->DispatchBeginPlay();
				R->AddMember(PC); R->Request(PC,EHALRoomAction::SelectVehicle,TEXT("Car_01"),PS->GetRoomData().SelectionRevision);
				FHALTravelPlayer P; P.Identity=PS->GetRoomData(); T.Players.Add(P);
			}
			T.HostKey=T.Players[0].Identity.PlayerKey; R->RestoreTravel(T);
			for (int32 I=0;I<Count;++I)
			{
				auto* PC=PCs[I]; auto* PS=PC->GetPlayerState<AHALPlayerState>(); R->ImportTravelMember(PC,T.Players[I],T.HostKey);
				auto* Class=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
				FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				auto* Pawn=W->SpawnActor<ATestVehiclePawn>(Class,FVector(I*500,0,1000),FRotator::ZeroRotator,P);
				Pawn->PreInitializeComponents(); Pawn->InitializeComponents(); Pawn->PostInitializeComponents();
				PS->BeginNewRoundAvatar(Pawn,1,1); Pawn->DispatchBeginPlay(); R->CompleteAvatar(PC,PS->GetRoomData().SelectionRevision,1);
			}
			R->AdvanceRound(1,EHALRoomPhase::Countdown); R->AdvanceRound(1,EHALRoomPhase::Playing); S->SetMatchPhase(EHALMatchPhase::Playing);
			Mode=W->SpawnActor<AHALPracticeRoomGameMode>(); FHALMatchLoopTestAccess::Begin(Mode,W,Config);
		}
		AHALPlayerState* Player(int32 I) const { return PCs[I]->GetPlayerState<AHALPlayerState>(); }
		void HP(int32 I,float Value) { Player(I)->GetHALAbilitySystemComponent()->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(),Value); }
		~FMatchWorld() { FHALMatchLoopTestAccess::Stop(Mode); W->DestroyWorld(false); GEngine->DestroyWorldContext(W); }
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMatchRulesTest,"HAL.FutureCreation.GameFlow.GF7.RulesAndDeterministicDecisions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALMatchRulesTest::RunTest(const FString&)
{
	FHALMatchRules Rules; FString Error; TestTrue(TEXT("Defaults valid"),Rules.Validate(Error));
	Rules.DurationSeconds=0; TestFalse(TEXT("Zero duration rejected"),Rules.Validate(Error)); Rules.DurationSeconds=300;
	Rules.ResultsSeconds=std::numeric_limits<float>::infinity(); TestFalse(TEXT("Nonfinite duration rejected"),Rules.Validate(Error));
	FHALRoundMatchData D; D.bActive=true;
	for (int32 I=0;I<3;++I) { auto& P=D.Players.AddDefaulted_GetRef(); P.Number=I+1; P.Life=EHALRoundLife::Alive; P.HP=I<2?75:20; P.MaxHP=I==0?300:100; }
	TestEqual(TEXT("No premature result"),HALRoundMatch::Decide(D,false),EHALMatchEndReason::None);
	TestEqual(TEXT("Timeout compares absolute HP"),HALRoundMatch::Decide(D,true),EHALMatchEndReason::TimeExpired);
	TestTrue(TEXT("Equal absolute HP shared despite different MaxHP"),D.Players[0].bWinner && D.Players[1].bWinner && !D.Players[2].bWinner);
	TestEqual(TEXT("Duplicate result cannot rerun"),HALRoundMatch::Decide(D,true),EHALMatchEndReason::None);
	D.bComplete=false; for (auto& P:D.Players) { P.Life=EHALRoundLife::Eliminated; P.HP=0; }
	TestEqual(TEXT("All last players out together is a draw"),HALRoundMatch::Decide(D,false),EHALMatchEndReason::NoSurvivors);
	TestFalse(TEXT("Draw clears former winner flags"),D.Players.ContainsByPredicate([](const auto& P){return P.bWinner;}));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMatchBatchTest,"HAL.FutureCreation.GameFlow.GF7.BatchDeathsAndImmutableResults",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALMatchBatchTest::RunTest(const FString&)
{
	FMatchWorld F; TestTrue(TEXT("Real round initialized"),F.R->GetData().Match.bActive && F.R->GetData().Match.Players.Num()==4);
	TestEqual(TEXT("Missing travel context rejects manual return"),F.Mode->ReturnActiveRoundToRoom(),EHALRoomResult::InvalidConfiguration);
	TestFalse(TEXT("Failed manual return does not poison the active match"),F.R->GetData().Match.bComplete);
	F.HP(0,0); F.HP(1,0); TestEqual(TEXT("No decision inside attribute callback"),F.R->GetData().Phase,EHALRoomPhase::Playing);
	F.HP(0,100); // A heal in the same callback batch must not revive a driver who already reached zero.
	FHALMatchLoopTestAccess::Evaluate(F.Mode);
	TestNull(TEXT("Eliminated host vehicle retired"),F.PCs[0]->GetPawn()); TestNull(TEXT("Second eliminated vehicle retired"),F.PCs[1]->GetPawn());
	TestEqual(TEXT("Lethal snapshot remains zero despite same-batch healing"),F.R->GetData().Match.Players[0].HP,0.f);
	TestEqual(TEXT("Eliminated source skills removed"),F.Player(0)->GetHALAbilitySystemComponent()->GetActivatableAbilities().Num(),0);
	TestEqual(TEXT("Two survivors continue"),F.R->GetData().Phase,EHALRoomPhase::Playing);
	TestTrue(TEXT("Survivor controls permitted"),HALGameFlow::Permissions(F.W,F.PCs[2]->GetPawn()).bDrive);
	F.HP(2,0); F.HP(3,0); FHALMatchLoopTestAccess::Evaluate(F.Mode);
	TestEqual(TEXT("Simultaneous final batch has no spurious winner"),F.R->GetData().Match.Reason,EHALMatchEndReason::NoSurvivors);
	TestEqual(TEXT("Results entered"),F.R->GetData().Phase,EHALRoomPhase::Results);
	TestFalse(TEXT("Results block world hit interactions"),HALGameFlow::Permissions(F.W).bWorldInteractions);
	const auto Result=F.R->GetData().Match; F.HP(0,100); FHALMatchLoopTestAccess::Evaluate(F.Mode);
	TestTrue(TEXT("Late health changes cannot rewrite result"),FHALRoundMatchData::StaticStruct()->CompareScriptStruct(&Result,&F.R->GetData().Match,0));
	TestFalse(TEXT("Public API cannot republish a frozen result"),F.R->SetMatchData(Result));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMatchCutoffTest,"HAL.FutureCreation.GameFlow.GF7.DeadlineSnapshotAndSinglePlayer",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALMatchCutoffTest::RunTest(const FString&)
{
	FMatchWorld F(2); F.HP(0,60); F.HP(1,40); FHALMatchLoopTestAccess::Expire(F.Mode);
	TestFalse(TEXT("Actions close at deadline even before timer poll"),HALGameFlow::Permissions(F.W,F.PCs[0]->GetPawn()).bDamage);
	F.HP(1,99); FHALMatchLoopTestAccess::Evaluate(F.Mode);
	TestTrue(TEXT("Post-cutoff modifier cannot steal timed victory"),F.R->GetData().Match.Players[0].bWinner && !F.R->GetData().Match.Players[1].bWinner);
	FMatchWorld Solo(1); FHALMatchLoopTestAccess::Evaluate(Solo.Mode);
	TestEqual(TEXT("Solo development does not immediately win"),Solo.R->GetData().Phase,EHALRoomPhase::Playing);
	FHALMatchLoopTestAccess::Expire(Solo.Mode); FHALMatchLoopTestAccess::Evaluate(Solo.Mode);
	TestEqual(TEXT("Solo can still test timed result"),Solo.R->GetData().Match.Reason,EHALMatchEndReason::TimeExpired);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMatchForfeitTest,"HAL.FutureCreation.GameFlow.GF7.ForfeitAndHostElimination",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALMatchForfeitTest::RunTest(const FString&)
{
	FMatchWorld F; FHALMatchLoopTestAccess::Forfeit(F.Mode,F.Player(2)); F.R->RemoveMember(F.PCs[2]); FHALMatchLoopTestAccess::Evaluate(F.Mode);
	TestEqual(TEXT("Quitter does not abort active round"),F.R->GetData().Phase,EHALRoomPhase::Playing);
	TestEqual(TEXT("Departed identity retained in value result"),F.R->GetData().Match.Players[2].Life,EHALRoundLife::Forfeited);
	F.HP(0,0); FHALMatchLoopTestAccess::Evaluate(F.Mode);
	TestEqual(TEXT("Host KO keeps two survivors playing"),F.R->GetData().Phase,EHALRoomPhase::Playing);
	TestTrue(TEXT("Host remains room owner after KO"),F.R->GetData().Host==F.Player(0));
	F.HP(1,0); FHALMatchLoopTestAccess::Evaluate(F.Mode);
	TestEqual(TEXT("One survivor ends round"),F.R->GetData().Match.Reason,EHALMatchEndReason::LastSurvivor);
	TestTrue(TEXT("Only surviving fourth player wins"),F.R->GetData().Match.Players[3].bWinner);
	return true;
}
#endif
