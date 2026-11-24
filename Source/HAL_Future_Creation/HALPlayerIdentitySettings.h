#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HALPlayerIdentitySettings.generated.h"

class AActor;
class UWorld;

/**
 * The one table of player identity colours. The room hands them out by seat (the authority copies the colour into the
 * replicated room data); every presentation layer that says "this belongs to that player" reads the same answer:
 * hold rings, holder marker, aim arrow, ball trail, incoming-ball warning and tire skid marks.
 * Cosmetic only. Deliberately no purple: purple means "being shocked" on a vehicle (the shock look moved off blue on
 * 2026-11-24 so that blue could be a seat colour).
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Player Identity"))
class HAL_FUTURE_CREATION_API UHALPlayerIdentitySettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UHALPlayerIdentitySettings();
	/** Linear colours by seat, seat 1 first: pink, yellow, teal, blue. Four seats; a shorter or longer list is used as it is, wrapping round. */
	UPROPERTY(Config, EditAnywhere, Category="Player Identity") TArray<FLinearColor> SeatColors;
	/** Used when the list is empty or holds an unusable colour. */
	UPROPERTY(Config, EditAnywhere, Category="Player Identity") FLinearColor FallbackColor = FLinearColor(0.98f, 0.62f, 0.12f, 1.f);
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Colour of a seat index, wrapping round the list; opaque. Negative indices and an empty list give the fallback. */
	static FLinearColor SeatColor(int32 SeatIndex);
	/** Same, from an explicit list: the testable core of SeatColor. */
	static FLinearColor SeatColorFrom(TConstArrayView<FLinearColor> Colors, const FLinearColor& Fallback, int32 SeatIndex);
	/**
	 * Identity colour of the player behind a pawn, or behind a player id when the pawn is gone. A seated player has the
	 * colour the room gave them; a player who never went through a room (a test map) takes the table by player id.
	 * False when there is no player at all, so the caller can fall back to something of its own.
	 */
	static bool ResolvePlayerColor(const UWorld* World, const AActor* Pawn, int32 PlayerId, FLinearColor& OutColor);
};
