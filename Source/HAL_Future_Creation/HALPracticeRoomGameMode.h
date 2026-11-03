#pragma once
#include "CoreMinimal.h"
#include "HALMatchGameMode.h"
#include "HALRoundTravelTypes.h"
#include "HALPracticeRoomGameMode.generated.h"

/** Server room/round orchestration; Avatars and physics are rebuilt across seamless map travel. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALPracticeRoomGameMode : public AHALMatchGameMode
{
	GENERATED_BODY()
public:
	AHALPracticeRoomGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& Error) override;
	virtual void InitGameState() override;
	virtual void PostLogin(APlayerController* Player) override;
	virtual void Logout(AController* Exiting) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& Id, FString& Error) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* Player) override;
	virtual EHALRoomResult HandleRoomAction(AHALPlayerController* Player, EHALRoomAction Action, FName VehicleId, uint32 Revision) override;
	void ReturnRoomToEntry(AHALPlayerController* Host);
	bool IsRoundWorld() const { return bRoundWorld; }
	/** Local host's manual test control. Exhibition requires a non-Shipping server opt-in. */
	bool CanRequestRoundReturn() const;
	/** Trusted server rules may end an active round through this path, regardless of test UI policy. */
	EHALRoomResult ReturnActiveRoundToRoom();
private:
	friend struct FHALPracticeRoomTestAccess;
	void RemovePracticeAvatar(AHALPlayerController* Player);
	void SetSelectionView(AHALPlayerController* Player);
	EHALRoomResult SpawnSelectedVehicle(AHALPlayerController* Player, uint32 Revision, uint32 Round=0);
	bool PreflightRound() const;
	bool StartRoundTravel(bool ToPractice, const FString& Reason=FString());
	void PollRoundBarrier();
	UPROPERTY(Transient) FHALRoundTravel Travel;
	bool bRestoringTravel=false, bRoundWorld=false, bTravelDeparting=false;
	FTimerHandle RoundBarrierTimer;
	bool bReturningToEntry = false;
	bool bAllowTestRoundEnd = false;
};
