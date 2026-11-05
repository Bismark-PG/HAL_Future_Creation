#include "HALRoundMatchTypes.h"
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
	int32 Alive=0; float Best=-1;
	for (const auto& P:D.Players) { if (P.Life==EHALRoundLife::Alive) { ++Alive; Best=FMath::Max(Best,P.HP); } }
	const auto Reason=Alive==0?EHALMatchEndReason::NoSurvivors:Expired?EHALMatchEndReason::TimeExpired
		:(Alive==1 && D.Players.Num()>1)?EHALMatchEndReason::LastSurvivor:EHALMatchEndReason::None;
	if (Reason==EHALMatchEndReason::None) { return Reason; }
	for (auto& P:D.Players)
	{ P.bWinner=P.Life==EHALRoundLife::Alive && (Reason==EHALMatchEndReason::LastSurvivor || FMath::IsNearlyEqual(P.HP,Best,.001f)); }
	D.Reason=Reason; D.bComplete=true; return Reason;
}
