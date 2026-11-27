#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include <limits>
#include "HALRoomTestSupport.h"
#include "../HALPracticeRoomGameMode.h"
#include "../HALMatchGameState.h"
#include "../HALMatchHUDWidget.h"
#include "../HALMatchFeverDefinition.h"
#include "../SpringPunchGimmick.h"
#include "../HALPracticeRoomWidget.h"
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
	static void SetRemaining(AHALPracticeRoomGameMode* M, double Seconds)
	{ M->ActiveMatch.EndServerTime = M->GetGameState<AHALMatchGameState>()->GetServerWorldTimeSeconds() + Seconds; }
	static void Expire(AHALPracticeRoomGameMode* M)
	{ M->ActiveMatch.EndServerTime=M->GetGameState<AHALMatchGameState>()->GetServerWorldTimeSeconds(); M->GetGameState<AHALMatchGameState>()->GetRoom()->SetMatchData(M->ActiveMatch); }
	static void Forfeit(AHALPracticeRoomGameMode* M,AHALPlayerState* P) { M->ForfeitMatchPlayer(P); }
	static void ExpireResults(AHALPracticeRoomGameMode* M)
	{ M->ActiveMatch.ReturnServerTime=M->GetGameState<AHALMatchGameState>()->GetServerWorldTimeSeconds()-1.; }
	static void Stop(AHALPracticeRoomGameMode* M) { M->StopMatchRules(); }
};
namespace
{
	struct FMatchWorld
	{
		UWorld* W; UHALRoomComponent* R; AHALPracticeRoomGameMode* Mode; UHALRoomConfig* Config;
		TArray<AHALPlayerController*> PCs;
		FMatchWorld(int32 Count=4, float StartingEnergy=0.f, UHALMatchFeverDefinition* Fever=nullptr)
		{
			const auto V=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&V);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
			auto* S=W->SpawnActor<AHALMatchGameState>(); W->SetGameState(S); R=S->GetRoom();
			Config=FHALRoomTestAccess::Config(R,EHALRoomProfile::Development);
			Config->StartingUltimateEnergy=StartingEnergy;
			Config->MatchFever=Fever;
			R->Initialize(Config); R->EnablePracticeRoom();
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMatchFeverRuleTest,"HAL.FutureCreation.MatchFever.RulesEnergyAndReset",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALMatchFeverRuleTest::RunTest(const FString&)
{
	auto* Fever = NewObject<UHALMatchFeverDefinition>();
	Fever->NaturalEnergyRateMultiplier = 2.f;
	int32 Starts = 0;
	FMatchWorld Duel(2, 20.f, Fever);
	auto* ASC = Duel.Player(0)->GetHALAbilitySystemComponent();
	const float BaseSeconds = ASC->GetUltimateEnergyView().FullRechargeSeconds;
	Duel.R->OnFeverChanged.AddLambda([&Starts](const auto* Value){ Starts += Value != nullptr; });
	FHALMatchLoopTestAccess::SetRemaining(Duel.Mode, 90.01);
	FHALMatchLoopTestAccess::Evaluate(Duel.Mode);
	TestNull(TEXT("A two-driver start and >90s stays normal"), Duel.R->GetActiveFever());
	FHALMatchLoopTestAccess::SetRemaining(Duel.Mode, 90.);
	FHALMatchLoopTestAccess::Evaluate(Duel.Mode);
	const auto& Match = Duel.R->GetData().Match;
	TestTrue(TEXT("Exact cutoff starts one banner window"), Match.IsFeverBannerVisible(Match.FeverStartedAt)
		&& !Match.IsFeverBannerVisible(Match.FeverStartedAt + Fever->BannerSeconds));
	TestEqual(TEXT("Owner HUD gets the effective recharge time"), ASC->GetUltimateEnergyView().FullRechargeSeconds, BaseSeconds / 2.f);
	Duel.W->TimeSeconds += 1.;
	const float Before = ASC->GetUltimateEnergyView().Energy;
	TestEqual(TEXT("Pickup amount remains independent of natural charge"), ASC->GrantUltimateEnergy(3.f), 3.f);
	TestEqual(TEXT("One second at Fever rate plus one ordinary pickup"), ASC->GetUltimateEnergyView().Energy, Before + 200.f / BaseSeconds + 3.f, .01f);
	FHALMatchLoopTestAccess::Evaluate(Duel.Mode);
	TestEqual(TEXT("Fever begins once despite later evaluations"), Starts, 1);
	Duel.HP(1,0); FHALMatchLoopTestAccess::Evaluate(Duel.Mode);
	TestNull(TEXT("Results retire the active phase"), Duel.R->GetActiveFever());
	FMatchWorld Four(4, 0.f, Fever);
	Four.HP(1,0); FHALMatchLoopTestAccess::Evaluate(Four.Mode);
	TestNull(TEXT("Three remaining keep the normal pace"), Four.R->GetActiveFever());
	Four.HP(2,0); FHALMatchLoopTestAccess::Evaluate(Four.Mode);
	TestTrue(TEXT("Two finalists enter Fever ahead of the clock"), Four.R->GetData().Match.IsFeverActive());
	FMatchWorld Batch(3, 0.f, Fever);
	Batch.HP(1,0); Batch.HP(2,0); FHALMatchLoopTestAccess::SetRemaining(Batch.Mode, 1.);
	FHALMatchLoopTestAccess::Evaluate(Batch.Mode);
	TestTrue(TEXT("Terminal batch wins over both triggers"), Batch.R->GetData().Match.bComplete && Batch.R->GetData().Match.FeverStartedAt < 0.);
#if !UE_BUILD_SHIPPING
	FMatchWorld PracticeDuel(2, 0.f, Fever);
	PracticeDuel.Mode->SetFeverDebugOverride(true);
	TestNotNull(TEXT("Host can audition before either trigger"), PracticeDuel.R->GetActiveFever());
	PracticeDuel.Mode->SetFeverDebugOverride(false);
	FHALMatchLoopTestAccess::SetRemaining(PracticeDuel.Mode, 50.);
	FHALMatchLoopTestAccess::Evaluate(PracticeDuel.Mode);
	TestNull(TEXT("Manual normal pace persists for comparison"), PracticeDuel.R->GetActiveFever());
	PracticeDuel.Mode->SetFeverDebugOverride({});
	TestNotNull(TEXT("Restoring automatic rules responds to the actual clock"), PracticeDuel.R->GetActiveFever());
#endif
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSpringFeverTest, "HAL.FutureCreation.MatchFever.SpringCooldownAndMotion", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALSpringFeverTest::RunTest(const FString&)
{
	auto* Fever = NewObject<UHALMatchFeverDefinition>();
	Fever->GimmickWaitMultiplier = .5f;
	FMatchWorld Match(2, 0.f, Fever);
	auto* Punch = Match.W->SpawnActor<ASpringPunchGimmick>();
	Punch->InitialDelay = 2; Punch->CooldownDuration = 4; Punch->DispatchBeginPlay();
	const double FirstStrike = Punch->Cycle.Motion.StartsAt;
	Punch->EnterState(ESpringPunchState::Windup);
	Punch->EnterState(ESpringPunchState::Extending);
	FHALMatchLoopTestAccess::SetRemaining(Match.Mode, 80);
	FHALMatchLoopTestAccess::Evaluate(Match.Mode);
	TestEqual(TEXT("Entering Fever preserves announced strike"), Punch->Cycle.Motion.StartsAt, FirstStrike);
	TestEqual(TEXT("Actual strike duration unchanged"), Punch->Phase.Duration, Punch->ExtendDuration);
	Punch->EnterState(ESpringPunchState::Retracting);
	Punch->EnterState(ESpringPunchState::Cooldown);
	TestEqual(TEXT("Next cooldown halved"), Punch->Phase.Duration, 2.f);
	Match.W->TimeSeconds = Punch->Phase.StartedAt + .5;
	Punch->ApplyFever(nullptr);
	TestEqual(TEXT("Three quarters of a four-second cooldown remain"), Match.W->GetTimerManager().GetTimerRemaining(Punch->StateTimerHandle), 3.f, .001f);
	const double NextStrike = Match.W->TimeSeconds + 3 + Punch->WindupDuration;
	Punch->EnterState(ESpringPunchState::Windup);
	TestEqual(TEXT("Observer schedule agrees with next authority strike"), Punch->Phase.Motion.StartsAt, NextStrike, .001);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALRoundEnergyTest,"HAL.FutureCreation.UltimateEnergy.ConfiguredRoundStartAndResults",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALRoundEnergyTest::RunTest(const FString&)
{
	FMatchWorld Fixture(2, 37.f);
	auto* Owner = Fixture.Player(0)->GetHALAbilitySystemComponent();
	TestEqual(TEXT("Round uses configured starting balance"), Owner->GetUltimateEnergyView().Energy, 37.f);
	TestEqual(TEXT("Playing allows a partial grant"), Owner->GrantUltimateEnergy(25.f), 25.f);
	Fixture.HP(1, 0);
	FHALMatchLoopTestAccess::Evaluate(Fixture.Mode);
	TestEqual(TEXT("Last survivor enters results"), Fixture.R->GetData().Phase, EHALRoomPhase::Results);
	TestEqual(TEXT("Results reject pickup"), Owner->GrantUltimateEnergy(25.f), 0.f);
	TestFalse(TEXT("Results stop the recovery clock"), Owner->GetUltimateEnergyView().bRecharging);
	TestEqual(TEXT("Scoreboard entry preserves survivor balance"), Owner->GetUltimateEnergyView().Energy, 62.f);
	return true;
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
	TestEqual(TEXT("All last players out together share victory"),HALRoundMatch::Decide(D,false),EHALMatchEndReason::SharedVictory);
	TestFalse(TEXT("Every final-batch player is a winner"),D.Players.ContainsByPredicate([](const auto& P){return !P.bWinner || P.FinalRank!=1;}));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMatchBatchTest,"HAL.FutureCreation.GameFlow.GF7.BatchDeathsAndImmutableResults",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALMatchBatchTest::RunTest(const FString&)
{
	FMatchWorld F; TestTrue(TEXT("Real round initialized"),F.R->GetData().Match.bActive && F.R->GetData().Match.Players.Num()==4);
	TestTrue(TEXT("Ranked snapshot is absent before completion"),F.R->GetData().Match.RankedPlayers.IsEmpty());
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
	TestEqual(TEXT("Simultaneous final batch shares victory"),F.R->GetData().Match.Reason,EHALMatchEndReason::SharedVictory);
	const auto& Ranked=F.R->GetData().Match.RankedPlayers;
	TestEqual(TEXT("All four identities retained in result"),Ranked.Num(),4);
	for (int32 I=0;I<4;++I)
	{
		TestEqual(TEXT("Later eliminated pair ranks first"),Ranked[I].Number,I<2?I+3:I-1);
		TestEqual(TEXT("Competition ranks skip the shared places"),Ranked[I].FinalRank,I<2?1:3);
		TestEqual(TEXT("Only the final pair wins"),Ranked[I].bWinner,I<2);
		TestEqual(TEXT("Batch comes from the authoritative poll"),Ranked[I].EliminationBatch,I<2?2:1);
		TestEqual(TEXT("Winning never resurrects a dead driver"),Ranked[I].HP,0.f);
	}
	TestEqual(TEXT("Results entered"),F.R->GetData().Phase,EHALRoomPhase::Results);
	TestFalse(TEXT("Results block world hit interactions"),HALGameFlow::Permissions(F.W).bWorldInteractions);
	const auto Result=F.R->GetData().Match; F.HP(0,100); FHALMatchLoopTestAccess::Evaluate(F.Mode);
	TestTrue(TEXT("Late health changes cannot rewrite result"),FHALRoundMatchData::StaticStruct()->CompareScriptStruct(&Result,&F.R->GetData().Match,0));
	TestFalse(TEXT("Public API cannot republish a frozen result"),F.R->SetMatchData(Result));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALResultsManualReturnTest,"HAL.FutureCreation.GameFlow.GF7.ResultsWaitForHostReturn",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALResultsManualReturnTest::RunTest(const FString&)
{
	FMatchWorld F(2); F.HP(1,0); FHALMatchLoopTestAccess::Evaluate(F.Mode);
	TestEqual(TEXT("Normal match completion enters Results"),F.R->GetData().Phase,EHALRoomPhase::Results);
	const auto Snapshot=F.R->GetData().Match; FHALMatchLoopTestAccess::ExpireResults(F.Mode);
	for (int32 I=0;I<3;++I) { FHALMatchLoopTestAccess::Evaluate(F.Mode); }
	TestEqual(TEXT("Expired former return deadline never leaves Results"),F.R->GetData().Phase,EHALRoomPhase::Results);
	TestTrue(TEXT("Waiting preserves the frozen result"),FHALRoundMatchData::StaticStruct()->CompareScriptStruct(&Snapshot,&F.R->GetData().Match,0));
	TestTrue(TEXT("Host's manual return remains available after the deadline"),F.Mode->CanRequestRoundReturn());
	const auto Revision=F.Player(0)->GetRoomData().SelectionRevision;
	TestEqual(TEXT("Other player cannot submit the host return"),F.Mode->HandleRoomAction(F.PCs[1],EHALRoomAction::ReturnToRoom,NAME_None,F.Player(1)->GetRoomData().SelectionRevision),EHALRoomResult::NotHost);
	TestEqual(TEXT("Stale host return is still rejected"),F.Mode->HandleRoomAction(F.PCs[0],EHALRoomAction::ReturnToRoom,NAME_None,Revision+1),EHALRoomResult::StaleSelection);
	TestEqual(TEXT("Valid host request reaches travel, absent only the fixture's travel context"),F.Mode->HandleRoomAction(F.PCs[0],EHALRoomAction::ReturnToRoom,NAME_None,Revision),EHALRoomResult::InvalidConfiguration);
	TestEqual(TEXT("Failed travel does not dismiss Results"),F.R->GetData().Phase,EHALRoomPhase::Results);
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
	TestTrue(TEXT("Solo fixture is explicitly marked and wins at timeout"),Solo.R->GetData().Match.bSinglePlayerTest && Solo.R->GetData().Match.Players[0].bWinner);
	FMatchWorld DeadSolo(1); DeadSolo.HP(0,0); FHALMatchLoopTestAccess::Evaluate(DeadSolo.Mode);
	const auto& Dead=DeadSolo.R->GetData().Match;
	TestEqual(TEXT("Dead solo fixture still completes"),Dead.Reason,EHALMatchEndReason::SoloTest);
	TestTrue(TEXT("Dead solo fixture is displayed as the winner"),Dead.bComplete && Dead.RankedPlayers.Num()==1 && Dead.Players[0].bWinner && Dead.Players[0].FinalRank==1);
	TestEqual(TEXT("Solo win does not rewrite frozen death HP"),Dead.Players[0].HP,0.f);
	TestEqual(TEXT("Solo win does not rewrite life state"),Dead.Players[0].Life,EHALRoundLife::Eliminated);
	auto* Summary=NewObject<UHALPracticeRoomWidget>(DeadSolo.PCs[0]);
	const FString ResultText=Summary->GetRoomView().ResultSummary.ToString();
	TestTrue(TEXT("Common summary names the dead solo winner"),ResultText.Contains(TEXT("WINNER")) && ResultText.Contains(TEXT("TOP 1")));
	TestFalse(TEXT("Common summary does not mislabel the dead winner"),ResultText.Contains(TEXT("OUT")) || ResultText.Contains(TEXT("DRAW")));
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
	const auto& Ranks=F.R->GetData().Match.RankedPlayers;
	TestEqual(TEXT("Forfeit result includes all original participants"),Ranks.Num(),4);
	TestEqual(TEXT("Quitter remains last behind eliminated players"),Ranks.Last().Number,3);
	TestEqual(TEXT("Quitter rank is fourth"),Ranks.Last().FinalRank,4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCompetitionRankTest,"HAL.FutureCreation.GameFlow.GF7.CompetitionRanks",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALCompetitionRankTest::RunTest(const FString&)
{
	FHALRoundMatchData D; D.bActive=true;
	for (int32 I=0;I<4;++I)
	{
		auto& P=D.Players.AddDefaulted_GetRef(); P.Number=4-I; P.PlayerKey=FGuid::NewGuid(); P.MaxHP=100;
		P.Life=I==0?EHALRoundLife::Eliminated:EHALRoundLife::Alive;
		P.HP=P.Number<=2?80:P.Number==3?20:0; P.EliminationBatch=I==0?1:0;
	}
	HALRoundMatch::Decide(D,true);
	for (int32 I=0;I<4;++I)
	{
		const auto& P=D.RankedPlayers[I];
		TestEqual(TEXT("Equal HP has stable display order"),P.Number,I+1);
		TestEqual(TEXT("Shared first place uses 1,1,3,4"),P.FinalRank,I<2?1:I+1);
		TestEqual(TEXT("Both highest HP players win"),P.bWinner,I<2);
		TestEqual(TEXT("Original participant order is preserved"),D.Players[I].Number,4-I);
	}
	const auto Frozen=D; HALRoundMatch::Decide(D,false);
	TestTrue(TEXT("Repeated decisions leave ranked snapshot immutable"),FHALRoundMatchData::StaticStruct()->CompareScriptStruct(&Frozen,&D,0));

	D=FHALRoundMatchData(); D.bActive=true;
	for (int32 I=0;I<3;++I)
	{ auto& P=D.Players.AddDefaulted_GetRef(); P.Number=I+1; P.Life=EHALRoundLife::Alive; P.HP=I==0?100.f:I==1?99.9995f:99.9989f; }
	HALRoundMatch::Decide(D,true);
	TestEqual(TEXT("HP tolerance shares the highest place"),D.RankedPlayers[1].FinalRank,1);
	TestEqual(TEXT("Tolerance groups use the group leader, not a transitive near-equal chain"),D.RankedPlayers[2].FinalRank,3);
	TestFalse(TEXT("Player outside the highest HP tolerance does not win"),D.RankedPlayers[2].bWinner);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMatchHUDDeadlineTest,"HAL.FutureCreation.GameFlow.GF7.MatchHUDServerDeadline",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALMatchHUDDeadlineTest::RunTest(const FString&)
{
	TestTrue(TEXT("No world means no fabricated countdown"),NewObject<UHALMatchHUDWidget>()->GetCountdownText().IsEmpty());
	FMatchWorld F;
	auto* HUD=NewObject<UHALMatchHUDWidget>(F.PCs[0]);
	auto Match=F.R->GetData().Match;
	const double Now=F.W->GetGameState<AHALMatchGameState>()->GetServerWorldTimeSeconds();
	Match.EndServerTime=Now+60.01; F.R->SetMatchData(Match);
	TestEqual(TEXT("Uses replicated deadline, rounding up"),HUD->GetCountdownText().ToString(),FString(TEXT("01:01")));
	Match.EndServerTime=Now+60; F.R->SetMatchData(Match);
	TestEqual(TEXT("Exact minute boundary"),HUD->GetCountdownText().ToString(),FString(TEXT("01:00")));
	Match.EndServerTime=Now-1; F.R->SetMatchData(Match);
	TestEqual(TEXT("Expired deadline never negative"),HUD->GetCountdownText().ToString(),FString(TEXT("00:00")));
	TestFalse(TEXT("Rendering never completes authoritative round"),F.R->GetData().Match.bComplete);
	Match.bComplete=true; F.R->SetMatchData(Match);
	TestTrue(TEXT("Completed match hides countdown"),HUD->GetCountdownText().IsEmpty());
	return true;
}
#endif
