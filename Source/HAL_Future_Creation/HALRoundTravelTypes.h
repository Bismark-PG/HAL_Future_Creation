#pragma once
#include "CoreMinimal.h"
#include "HALRoomTypes.h"
#include "HALRoundTravelTypes.generated.h"
class UHALRoomConfig;
/** Transport only: no old World, Controller, PlayerState or Pawn references. */
USTRUCT()
struct HAL_FUTURE_CREATION_API FHALTravelPlayer
{
	GENERATED_BODY()
	UPROPERTY() FHALRoomPlayerData Identity;
	UPROPERTY() uint32 ChangeSerial=0;
	UPROPERTY() uint32 LifecycleRevision=0;
	UPROPERTY() uint32 PreviousRound=0;
};
USTRUCT()
struct HAL_FUTURE_CREATION_API FHALRoundTravel
{
	GENERATED_BODY()
	UPROPERTY() FGuid FlowId;
	UPROPERTY() FGuid HostKey;
	UPROPERTY() uint32 RoundId=0;
	UPROPERTY() bool bToPractice=false;
	UPROPERTY() FString Destination;
	UPROPERTY() FString PracticeMap;
	UPROPERTY() FString Reason;
	UPROPERTY() TObjectPtr<UHALRoomConfig> Config;
	UPROPERTY() TArray<FHALTravelPlayer> Players;
	double StartedAt=0;
	bool IsValid() const;
};
