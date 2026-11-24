#include "HALPlayerIdentitySettings.h"
#include "HALPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

UHALPlayerIdentitySettings::UHALPlayerIdentitySettings()
{
	// sRGB #EA1E8C, #FFE13F, #17CFA6, #1FA8FF.
	SeatColors = { FLinearColor(.8228f, .0130f, .2623f), FLinearColor(1.f, .75f, .05f), FLinearColor(.0086f, .6240f, .3813f), FLinearColor(.0137f, .3916f, 1.f) };
}

FLinearColor UHALPlayerIdentitySettings::SeatColorFrom(TConstArrayView<FLinearColor> Colors, const FLinearColor& Fallback, int32 SeatIndex)
{
	if (SeatIndex < 0 || Colors.IsEmpty()) { return Fallback; }
	const FLinearColor& Colour = Colors[SeatIndex % Colors.Num()];
	if (!FMath::IsFinite(Colour.R) || !FMath::IsFinite(Colour.G) || !FMath::IsFinite(Colour.B) || Colour.GetMin() < 0) { return Fallback; }
	return FLinearColor(Colour.R, Colour.G, Colour.B, 1.f);
}

FLinearColor UHALPlayerIdentitySettings::SeatColor(int32 SeatIndex)
{
	const UHALPlayerIdentitySettings* Settings = GetDefault<UHALPlayerIdentitySettings>();
	return SeatColorFrom(Settings->SeatColors, Settings->FallbackColor, SeatIndex);
}

bool UHALPlayerIdentitySettings::ResolvePlayerColor(const UWorld* World, const AActor* Pawn, int32 PlayerId, FLinearColor& OutColor)
{
	const APawn* AsPawn = Cast<APawn>(Pawn);
	const APlayerState* State = AsPawn ? AsPawn->GetPlayerState() : nullptr;
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!State && PlayerId != INDEX_NONE && GameState)
	{
		for (const APlayerState* Candidate : GameState->PlayerArray)
		{ if (Candidate && Candidate->GetPlayerId() == PlayerId) { State = Candidate; break; } }
	}
	if (!State) { return false; }
	if (const auto* RoomPlayer = Cast<AHALPlayerState>(State); RoomPlayer && RoomPlayer->GetRoomData().Seat != INDEX_NONE)
	{
		OutColor = RoomPlayer->GetRoomData().Color;
		return true;
	}
	if (State->GetPlayerId() < 0) { return false; }
	OutColor = SeatColor(State->GetPlayerId());
	return true;
}
