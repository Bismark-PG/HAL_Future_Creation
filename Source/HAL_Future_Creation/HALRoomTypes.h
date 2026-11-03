#pragma once
#include "CoreMinimal.h"
#include "HALRoomTypes.generated.h"

class AHALPlayerState;
class APawn;
class UHALVehicleCatalog;

UENUM(BlueprintType)
enum class EHALRoomProfile : uint8 { Exhibition, Development };
UENUM(BlueprintType)
enum class EHALRoomPhase : uint8 { Inactive, Open, Locked, Loading, Countdown, Playing, Returning };
UENUM(BlueprintType)
enum class EHALRoomPlayerPhase : uint8 { Unassigned, Selecting, PreparingAvatar, Ready };
UENUM(BlueprintType)
enum class EHALRoomAction : uint8 { SelectVehicle, ConfirmVehicle, CancelConfirmation, Start, ReturnToRoom };
UENUM(BlueprintType)
enum class EHALRoomResult : uint8
{
	Accepted, NotAuthority, Inactive, InvalidConfiguration, NotMember, WrongPhase,
	InvalidVehicle, StaleSelection, NotHost, NotReady, Full, InvalidRequest,
	MissingSpawn, SpawnBlocked, SpawnFailed
};

/** Explicit server profile. Never inferred from PIE, player count or packaged/editor builds. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALRoomRules
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Room") EHALRoomProfile Profile = EHALRoomProfile::Exhibition;
	int32 MinimumPlayers() const { return Profile == EHALRoomProfile::Development ? 1 : 4; }
	int32 MaximumPlayers() const { return 4; }
	bool IsValid() const { return Profile == EHALRoomProfile::Development || Profile == EHALRoomProfile::Exhibition; }
};

/** Confirmed is not the same as ready, and neither is a data asset's ReadyForUse. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALRoomPlayerData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Room") FGuid PlayerKey;
	UPROPERTY(BlueprintReadOnly, Category="Room") int32 Seat = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category="Room") int32 Number = 0;
	UPROPERTY(BlueprintReadOnly, Category="Room") FLinearColor Color = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly, Category="Room") FName VehicleId;
	UPROPERTY(BlueprintReadOnly, Category="Room") EHALRoomPlayerPhase Phase = EHALRoomPlayerPhase::Unassigned;
	/** Runtime readiness belongs to this Avatar, never automatically inherited by a replacement Pawn. */
	UPROPERTY(BlueprintReadOnly, Category="Room") TObjectPtr<APawn> ReadyAvatar;
	UPROPERTY(BlueprintReadOnly, Category="Room") EHALRoomResult AvatarResult = EHALRoomResult::Accepted;
	UPROPERTY(BlueprintReadOnly, Category="Room") bool bClientLoaded=false;
	UPROPERTY() FGuid LoadToken;
	UPROPERTY() uint32 SelectionRevision = 0;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALRoomRosterEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Room") TObjectPtr<AHALPlayerState> Player;
	UPROPERTY(BlueprintReadOnly, Category="Room") int32 Seat = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category="Room") FName VehicleId;
	UPROPERTY() uint32 SelectionRevision = 0;
};

/** One replicated public snapshot. The locked roster is frozen, separate from current membership. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALRoomData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Room") EHALRoomPhase Phase = EHALRoomPhase::Inactive;
	UPROPERTY(BlueprintReadOnly, Category="Room") FHALRoomRules Rules;
	/** Dedicated GF-3 room; legacy regression fixtures remain data-only. */
	UPROPERTY(BlueprintReadOnly, Category="Room") bool bPracticeRoom = false;
	UPROPERTY(BlueprintReadOnly, Category="Room") TSoftObjectPtr<UHALVehicleCatalog> Catalog;
	UPROPERTY(BlueprintReadOnly, Category="Room") TObjectPtr<AHALPlayerState> Host;
	UPROPERTY(BlueprintReadOnly, Category="Room") TArray<TObjectPtr<AHALPlayerState>> Members;
	UPROPERTY(BlueprintReadOnly, Category="Room") TArray<FHALRoomRosterEntry> LockedRoster;
	UPROPERTY() uint32 RoundId = 0;
	UPROPERTY() uint32 Revision = 0;
	UPROPERTY(BlueprintReadOnly, Category="Room") FGuid FlowId;
	UPROPERTY(BlueprintReadOnly, Category="Room") int32 ExpectedPlayers=0;
	UPROPERTY(BlueprintReadOnly, Category="Room") double CountdownEndServerTime=0;
	UPROPERTY(BlueprintReadOnly, Category="Room") FString FlowMessage;
};
