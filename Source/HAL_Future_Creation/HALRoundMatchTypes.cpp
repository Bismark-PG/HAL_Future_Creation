#include "HALRoundMatchTypes.h"
#include "HALMatchFeverDefinition.h"

bool UHALMatchFeverDefinition::Validate(FString& Error) const
{
	if (!FMath::IsWithinInclusive(RemainingSeconds, 0.f, 3600.f)
		|| !FMath::IsWithinInclusive(GimmickWaitMultiplier, .05f, 4.f)
		|| !FMath::IsWithinInclusive(SpecialBallIntervalMultiplier, .05f, 4.f)
		|| !FMath::IsWithinInclusive(NaturalEnergyRateMultiplier, .05f, 8.f)
		|| !FMath::IsWithinInclusive(BannerSeconds, .1f, 5.f))
	{
		Error = TEXT("Fever timing or pacing multiplier is outside its supported range.");
		return false;
	}
	return true;
}

bool FHALRoundMatchData::TryStartFever(double ServerNow, bool bForceTrigger)
{
	if (!bActive || bComplete || !FeverDefinition || FeverStartedAt >= 0. || ServerNow >= EndServerTime) { return false; }
	if (!bForceTrigger && EndServerTime - ServerNow > FeverDefinition->RemainingSeconds)
	{
		if (Players.Num() < 3) { return false; }
		int32 Alive = 0;
		for (const auto& Player : Players) { Alive += Player.Life == EHALRoundLife::Alive; }
		if (Alive != 2) { return false; }
	}
	FeverStartedAt = ServerNow;
	return true;
}

bool FHALRoundMatchData::IsFeverBannerVisible(double ServerNow) const
{
	return IsFeverActive() && ServerNow < FeverStartedAt + FeverDefinition->BannerSeconds;
}
bool FHALMatchRules::Validate(FString& Error) const
{
	if (!FMath::IsFinite(DurationSeconds) || DurationSeconds<1 || DurationSeconds>3600
		|| !FMath::IsFinite(ResultsSeconds) || ResultsSeconds<1 || ResultsSeconds>60
		|| !FMath::IsFinite(SpectatorDistance) || SpectatorDistance<300 || SpectatorDistance>5000
		|| !FMath::IsFinite(SpectatorFOV) || SpectatorFOV<30 || SpectatorFOV>120
		|| !FMath::IsFinite(SpectatorPitch) || SpectatorPitch<-89 || SpectatorPitch>-30)
	{ Error=TEXT("Match needs duration 1-3600s, results 1-60s and finite spectator distance/FOV/pitch in the configured ranges."); return false; }
	return true;
}
EHALMatchEndReason HALRoundMatch::Decide(FHALRoundMatchData& D,bool Expired)
{
	if (!D.bActive || D.bComplete || D.Players.IsEmpty()) { return EHALMatchEndReason::None; }
	int32 Alive=0; bool HasEliminated=false;
	for (const auto& P:D.Players) { Alive+=P.Life==EHALRoundLife::Alive; HasEliminated|=P.Life==EHALRoundLife::Eliminated; }
	const bool Solo=D.bSinglePlayerTest && D.Players.Num()==1;
	const auto Reason=Alive==0?(Solo?EHALMatchEndReason::SoloTest:HasEliminated?EHALMatchEndReason::SharedVictory:EHALMatchEndReason::Aborted):Expired?EHALMatchEndReason::TimeExpired
		:(Alive==1 && D.Players.Num()>1)?EHALMatchEndReason::LastSurvivor:EHALMatchEndReason::None;
	if (Reason==EHALMatchEndReason::None) { return Reason; }
	const auto Category=[](const FHALRoundParticipant& P)
	{ return P.Life==EHALRoundLife::Alive?0:P.Life==EHALRoundLife::Eliminated?1:P.Life==EHALRoundLife::Forfeited?2:3; };
	TArray<int32> Order; Order.Reserve(D.Players.Num());
	for (int32 I=0;I<D.Players.Num();++I) { Order.Add(I); }
	Order.Sort([&](int32 A,int32 B)
	{
		const auto& X=D.Players[A]; const auto& Y=D.Players[B];
		const int32 CX=Category(X),CY=Category(Y);
		if (CX!=CY) { return CX<CY; }
		// Keep the comparator strict; HP tolerance is applied against each group's leader below.
		if (CX==0 && X.HP!=Y.HP) { return X.HP>Y.HP; }
		if (CX!=0 && X.EliminationBatch!=Y.EliminationBatch) { return X.EliminationBatch>Y.EliminationBatch; }
		return X.Number!=Y.Number?X.Number<Y.Number:A<B;
	});
	D.RankedPlayers.Reset(D.Players.Num()); int32 Leader=0;
	for (int32 I=0;I<Order.Num();++I)
	{
		auto& P=D.Players[Order[I]]; const auto& L=D.Players[Order[Leader]];
		const bool Same=Category(P)==Category(L) && (P.Life==EHALRoundLife::Alive
			?FMath::IsNearlyEqual(P.HP,L.HP,.001f):P.EliminationBatch==L.EliminationBatch);
		if (!Same) { Leader=I; }
		P.FinalRank=Leader+1;
		P.bWinner=Solo || (P.FinalRank==1 && (P.Life==EHALRoundLife::Alive
			|| (Reason==EHALMatchEndReason::SharedVictory && P.Life==EHALRoundLife::Eliminated)));
		D.RankedPlayers.Add(P);
	}
	D.RankedPlayers.Sort([](const auto& A,const auto& B)
	{ return A.FinalRank!=B.FinalRank?A.FinalRank<B.FinalRank:A.Number<B.Number; });
	D.Reason=Reason; D.bComplete=true; return Reason;
}
