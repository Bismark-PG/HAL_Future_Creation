#include "HALPracticeRoomGameMode.h"
#include "HALMatchGameState.h"
#include "HALRoomComponent.h"
#include "HALRoomConfig.h"
#include "HALPlayerState.h"
#include "HALPlayerController.h"
#include "HALAbilitySystemComponent.h"
#include "HALHealthAttributeSet.h"
#include "TestVehiclePawn.h"
#include "Engine/World.h"
#include "TimerManager.h"

void AHALPracticeRoomGameMode::BeginMatchRules()
{
	auto* S=GetGameState<AHALMatchGameState>(); auto* R=S?S->GetRoom():nullptr;
	if (!HasAuthority() || !bRoundWorld || !RoomConfig || !R || R->GetData().Phase!=EHALRoomPhase::Playing || ActiveMatch.bActive) { return; }
	StopMatchRules(); ActiveMatch.bActive=true; ActiveMatch.RoundId=R->GetData().RoundId;
	ActiveMatch.Rules=RoomConfig->MatchRules; ActiveMatch.EndServerTime=S->GetServerWorldTimeSeconds()+ActiveMatch.Rules.DurationSeconds;
	MatchFlow=R->GetData().FlowId;
	for (const auto& Entry:R->GetData().LockedRoster)
	{
		auto* PS=Entry.Player.Get(); auto* Pawn=PS?Cast<ATestVehiclePawn>(PS->GetPawn()):nullptr;
		auto* ASC=PS?PS->GetHALAbilitySystemComponent():nullptr;
		const auto* H=ASC?ASC->GetSet<UHALHealthAttributeSet>():nullptr;
		if (!PS || !Pawn || !H) { continue; }
		auto Identity=PS->GetRoomData(); Identity.RoundLife=EHALRoundLife::Alive; PS->SetRoomData(Identity);
		auto& P=ActiveMatch.Players.AddDefaulted_GetRef(); P.PlayerKey=Identity.PlayerKey; P.Number=Identity.Number;
		P.PlayerName=PS->GetPlayerName(); P.Color=Identity.Color; P.VehicleId=Identity.VehicleId;
		P.Life=EHALRoundLife::Alive; P.HP=H->GetCurrentHP(); P.MaxHP=H->GetMaxHP();
		auto& Watch=MatchWatches.AddDefaulted_GetRef(); Watch.Key=P.PlayerKey; Watch.Player=PS; Watch.Pawn=Pawn; Watch.ASC=ASC;
		const auto Key=P.PlayerKey;
		Watch.HP=ASC->GetGameplayAttributeValueChangeDelegate(UHALHealthAttributeSet::GetCurrentHPAttribute())
			.AddWeakLambda(this,[this,Key](const FOnAttributeChangeData&){ MatchHealthChanged(Key); });
		Watch.MaxHP=ASC->GetGameplayAttributeValueChangeDelegate(UHALHealthAttributeSet::GetMaxHPAttribute())
			.AddWeakLambda(this,[this,Key](const FOnAttributeChangeData&){ MatchHealthChanged(Key); });
	}
	ActiveMatch.Players.Sort([](const auto& A,const auto& B){ return A.Number<B.Number; });
	if (ActiveMatch.Players.IsEmpty() || ActiveMatch.Players.Num()!=R->GetData().LockedRoster.Num())
	{ StopMatchRules(); StartRoundTravel(true,TEXT("Could not prepare every driver. Please confirm again.")); return; }
	R->SetMatchData(ActiveMatch);
	GetWorldTimerManager().SetTimer(MatchPollTimer,this,&ThisClass::PollMatchRules,.1f,true);
	UE_LOG(LogTemp,Display,TEXT("GF7 match started Round=%lld Players=%d Deadline=%.3f"),ActiveMatch.RoundId,ActiveMatch.Players.Num(),ActiveMatch.EndServerTime);
}
void AHALPracticeRoomGameMode::MatchHealthChanged(FGuid Key)
{
	if (bEvaluatingMatch || !ActiveMatch.bActive || ActiveMatch.bComplete) { return; }
	auto* S=GetGameState<AHALMatchGameState>();
	if (!S || S->GetRoom()->GetData().FlowId!=MatchFlow || S->GetRoom()->GetData().RoundId!=ActiveMatch.RoundId) { return; }
	// Accepted damage before the cutoff updates the snapshot. Late modifiers cannot change the timed result.
	if (S->GetServerWorldTimeSeconds()<ActiveMatch.EndServerTime)
	{
		auto* P=ActiveMatch.Players.FindByPredicate([Key](const auto& E){ return E.PlayerKey==Key; });
		const auto* W=MatchWatches.FindByPredicate([Key](const auto& E){ return E.Key==Key; });
		const auto* ASC=W?W->ASC.Get():nullptr; const auto* H=ASC?ASC->GetSet<UHALHealthAttributeSet>():nullptr;
		// Latch lethal HP while waiting for the batch: a later heal cannot revive this round's driver.
		if (P && P->Life==EHALRoundLife::Alive && P->HP>0 && H && FMath::IsFinite(H->GetCurrentHP()) && FMath::IsFinite(H->GetMaxHP()))
		{ P->HP=FMath::Clamp(H->GetCurrentHP(),0.f,H->GetMaxHP()); P->MaxHP=H->GetMaxHP(); }
	}
	ScheduleMatchEvaluation();
}
void AHALPracticeRoomGameMode::ScheduleMatchEvaluation()
{
	if (!bEvaluatingMatch && !MatchEvaluateTimer.IsValid() && ActiveMatch.bActive && !ActiveMatch.bComplete)
	{ MatchEvaluateTimer=GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this]{ MatchEvaluateTimer.Invalidate(); PollMatchRules(); })); }
}
void AHALPracticeRoomGameMode::PollMatchRules()
{
	if (!HasAuthority() || bEvaluatingMatch || bTravelDeparting || bReturningToEntry || !ActiveMatch.bActive) { return; }
	auto* S=GetGameState<AHALMatchGameState>(); auto* R=S?S->GetRoom():nullptr;
	if (!R || R->GetData().RoundId!=ActiveMatch.RoundId || R->GetData().FlowId!=MatchFlow) { StopMatchRules(); return; }
	const double Now=S->GetServerWorldTimeSeconds();
	if (ActiveMatch.bComplete)
	{
		if (R->GetData().Phase==EHALRoomPhase::Results && Now>=ActiveMatch.ReturnServerTime)
		{
			if (ReturnActiveRoundToRoom()!=EHALRoomResult::Accepted)
			{ UE_LOG(LogTemp,Error,TEXT("GF7 automatic return failed Round=%lld"),ActiveMatch.RoundId);
				if (auto* Host=R->GetData().Host.Get()) { ReturnRoomToEntry(Cast<AHALPlayerController>(Host->GetOwner())); } }
		}
		return;
	}
	if (R->GetData().Phase!=EHALRoomPhase::Playing) { return; }
	TGuardValue<bool> Guard(bEvaluatingMatch,true);
	TArray<TWeakObjectPtr<ATestVehiclePawn>> Retiring;
	for (auto& P:ActiveMatch.Players)
	{
		if (P.Life!=EHALRoundLife::Alive) { continue; }
		const auto* W=MatchWatches.FindByPredicate([&P](const auto& V){ return V.Key==P.PlayerKey; });
		auto* PS=W?W->Player.Get():nullptr; auto* Pawn=W?W->Pawn.Get():nullptr;
		if (P.HP>0 && PS && Pawn && PS->GetPawn()==Pawn && Pawn->IsConfigurationValid()) { continue; }
		P.Life=EHALRoundLife::Eliminated;
		if (PS)
		{ auto I=PS->GetRoomData(); I.RoundLife=P.Life; I.ReadyAvatar=nullptr; PS->SetRoomData(I); }
		if (Pawn) { Retiring.Add(Pawn); }
		UE_LOG(LogTemp,Display,TEXT("GF7 eliminated Round=%lld P%d HP=%.3f"),ActiveMatch.RoundId,P.Number,P.HP);
	}
	const auto Reason=HALRoundMatch::Decide(ActiveMatch,Now>=ActiveMatch.EndServerTime);
	if (Reason!=EHALMatchEndReason::None) { ActiveMatch.ReturnServerTime=Now+ActiveMatch.Rules.ResultsSeconds; }
	// A single publication covers the entire damage batch, including simultaneous last-player deaths.
	if (!FHALRoundMatchData::StaticStruct()->CompareScriptStruct(&ActiveMatch,&R->GetData().Match,0)) { R->SetMatchData(ActiveMatch); }
	if (Reason!=EHALMatchEndReason::None)
	{
		R->AdvanceRound(uint32(ActiveMatch.RoundId),EHALRoomPhase::Results); S->SetMatchPhase(EHALMatchPhase::Finished);
		FString Winners; for (const auto& P:ActiveMatch.Players) { if (P.bWinner) { Winners+=FString::Printf(TEXT("P%d "),P.Number); } }
		UE_LOG(LogTemp,Display,TEXT("GF7 results Round=%lld Reason=%d Winners=%s Return=%.3f"),ActiveMatch.RoundId,int32(Reason),*Winners,ActiveMatch.ReturnServerTime);
	}
	// Do not destroy actors inside the GAS modifier/collision callback which was still using them.
	for (const auto& Weak:Retiring)
	{
		if (auto* Pawn=Weak.Get())
		{ auto* PC=Pawn->GetController(); Pawn->RetireForVehicleTransition(); if (PC && PC->GetPawn()==Pawn) { PC->UnPossess(); } Pawn->Destroy(); }
	}
}
void AHALPracticeRoomGameMode::ForfeitMatchPlayer(AHALPlayerState* PS)
{
	if (!PS || !ActiveMatch.bActive || ActiveMatch.bComplete) { return; }
	if (GetGameState<AHALMatchGameState>()->GetServerWorldTimeSeconds()>=ActiveMatch.EndServerTime) { PollMatchRules(); }
	if (ActiveMatch.bComplete) { return; }
	auto* P=ActiveMatch.Players.FindByPredicate([PS](const auto& E){ return E.PlayerKey==PS->GetRoomData().PlayerKey; });
	if (P && P->Life==EHALRoundLife::Alive)
	{ P->Life=EHALRoundLife::Forfeited; auto I=PS->GetRoomData(); I.RoundLife=P->Life; PS->SetRoomData(I);
		UE_LOG(LogTemp,Display,TEXT("GF7 forfeited Round=%lld P%d"),ActiveMatch.RoundId,P->Number); ScheduleMatchEvaluation(); }
}
void AHALPracticeRoomGameMode::StopMatchRules()
{
	GetWorldTimerManager().ClearTimer(MatchPollTimer); GetWorldTimerManager().ClearTimer(MatchEvaluateTimer);
	for (const auto& W:MatchWatches)
	{
		if (auto* A=W.ASC.Get())
		{ A->GetGameplayAttributeValueChangeDelegate(UHALHealthAttributeSet::GetCurrentHPAttribute()).Remove(W.HP);
			A->GetGameplayAttributeValueChangeDelegate(UHALHealthAttributeSet::GetMaxHPAttribute()).Remove(W.MaxHP); }
	}
	MatchWatches.Reset(); ActiveMatch={}; MatchFlow.Invalidate();
}
void AHALPracticeRoomGameMode::EndPlay(const EEndPlayReason::Type Reason)
{ StopMatchRules(); Super::EndPlay(Reason); }
