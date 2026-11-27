#pragma once
#include "CoreMinimal.h"
#include "HALRoundMatchTypes.generated.h"

class UHALMatchFeverDefinition;

UENUM(BlueprintType)
enum class EHALRoundLife : uint8 { None, Alive, Eliminated, Forfeited };
UENUM(BlueprintType)
enum class EHALMatchEndReason : uint8 { None, LastSurvivor, TimeExpired, NoSurvivors, Aborted, SharedVictory, SoloTest };

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALMatchRules
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Match", meta=(Units="s",ClampMin="1",ClampMax="3600")) float DurationSeconds=300;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Match", meta=(Units="s",ClampMin="1",ClampMax="60")) float ResultsSeconds=8;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spectator", meta=(Units="cm",ClampMin="300",ClampMax="5000")) float SpectatorDistance=1400;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spectator", meta=(Units="deg",ClampMin="30",ClampMax="120")) float SpectatorFOV=60;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spectator", meta=(Units="deg",ClampMin="-89",ClampMax="-30")) float SpectatorPitch=-65;
	bool Validate(FString& Error) const;
};

/** Value snapshot survives a player's departure and map travel without old-World references. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALRoundParticipant
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Match") FGuid PlayerKey;
	UPROPERTY(BlueprintReadOnly, Category="Match") int32 Number=0;
	UPROPERTY(BlueprintReadOnly, Category="Match") FString PlayerName;
	UPROPERTY(BlueprintReadOnly, Category="Match") FLinearColor Color=FLinearColor::White;
	UPROPERTY(BlueprintReadOnly, Category="Match") FName VehicleId;
	UPROPERTY(BlueprintReadOnly, Category="Match") EHALRoundLife Life=EHALRoundLife::None;
	UPROPERTY(BlueprintReadOnly, Category="Match") float HP=0;
	UPROPERTY(BlueprintReadOnly, Category="Match") float MaxHP=0;
	UPROPERTY(BlueprintReadOnly, Category="Match") bool bWinner=false;
	/** Server evaluation batch when eliminated/forfeited; zero while still alive. */
	UPROPERTY(BlueprintReadOnly, Category="Match") int32 EliminationBatch=0;
	/** Competition rank (1,1,3...), assigned once when the result is frozen. Zero before results. */
	UPROPERTY(BlueprintReadOnly, Category="Match") int32 FinalRank=0;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALRoundMatchData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Match") bool bActive=false;
	UPROPERTY(BlueprintReadOnly, Category="Match") bool bComplete=false;
	/** Temporary Development-only, one-driver fixture. Remove its entry point before release. */
	UPROPERTY(BlueprintReadOnly, Category="Match") bool bSinglePlayerTest=false;
	UPROPERTY(BlueprintReadOnly, Category="Match") int32 LastEliminationBatch=0;
	UPROPERTY(BlueprintReadOnly, Category="Match") int64 RoundId=0;
	UPROPERTY(BlueprintReadOnly, Category="Match") double EndServerTime=0;
	UPROPERTY(BlueprintReadOnly, Category="Match|Fever") TObjectPtr<UHALMatchFeverDefinition> FeverDefinition;
	/** Negative until Fever first starts in this round. Shared server game clock. */
	UPROPERTY(BlueprintReadOnly, Category="Match|Fever") double FeverStartedAt=-1.;
	UPROPERTY(BlueprintReadOnly, Category="Match") double ReturnServerTime=0;
	UPROPERTY(BlueprintReadOnly, Category="Match") EHALMatchEndReason Reason=EHALMatchEndReason::None;
	UPROPERTY(BlueprintReadOnly, Category="Match") FHALMatchRules Rules;
	UPROPERTY(BlueprintReadOnly, Category="Match") TArray<FHALRoundParticipant> Players;
	/** Frozen display order; Players keeps the original identity/seat order. */
	UPROPERTY(BlueprintReadOnly, Category="Match") TArray<FHALRoundParticipant> RankedPlayers;
	const FHALRoundParticipant* Find(FGuid Key) const { return Players.FindByPredicate([Key](const auto& P){ return P.PlayerKey==Key; }); }
	bool IsFeverActive() const { return bActive && !bComplete && FeverDefinition && FeverStartedAt >= 0.; }
	bool IsFeverBannerVisible(double ServerNow) const;
	bool TryStartFever(double ServerNow, bool bForceTrigger = false);
};

namespace HALRoundMatch
{
	/** Caller batches all deaths first. No mutation unless a terminal condition has been reached. */
	HAL_FUTURE_CREATION_API EHALMatchEndReason Decide(FHALRoundMatchData& Data,bool bExpired);
}
