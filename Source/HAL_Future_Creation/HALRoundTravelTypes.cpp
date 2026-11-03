#include "HALRoundTravelTypes.h"
#include "HALRoomConfig.h"
bool FHALRoundTravel::IsValid() const
{
	FString Error;
	if (!FlowId.IsValid() || !HostKey.IsValid() || !RoundId || !Config || !Config->Validate(Error)
		|| Players.IsEmpty() || Players.Num()>4 || (!bToPractice && Players.Num()<Config->Rules.MinimumPlayers())) { return false; }
	TSet<FGuid> Keys; TSet<int32> Seats;
	for (const auto& P:Players)
	{
		const auto& I=P.Identity;
		if (!I.PlayerKey.IsValid() || Keys.Contains(I.PlayerKey) || Seats.Contains(I.Seat) || I.Seat<0 || I.Seat>3 || I.ReadyAvatar || I.LoadToken.IsValid()) { return false; }
		Keys.Add(I.PlayerKey); Seats.Add(I.Seat);
	}
	return Keys.Contains(HostKey);
}
