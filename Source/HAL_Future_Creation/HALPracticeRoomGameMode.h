#pragma once
#include "CoreMinimal.h"
#include "HALMatchGameMode.h"
#include "HALPracticeRoomGameMode.generated.h"

/** GF-3 only: selected-vehicle spawning/practice and basic room exit. No round travel. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALPracticeRoomGameMode : public AHALMatchGameMode
{
	GENERATED_BODY()
public:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& Error) override;
	virtual void InitGameState() override;
	virtual void PostLogin(APlayerController* Player) override;
	virtual void Logout(AController* Exiting) override;
	virtual EHALRoomResult HandleRoomAction(AHALPlayerController* Player, EHALRoomAction Action, FName VehicleId, uint32 Revision) override;
	void ReturnRoomToEntry(AHALPlayerController* Host);
private:
	friend struct FHALPracticeRoomTestAccess;
	void RemovePracticeAvatar(AHALPlayerController* Player);
	void SetSelectionView(AHALPlayerController* Player);
	EHALRoomResult SpawnSelectedVehicle(AHALPlayerController* Player, uint32 Revision);
	bool bReturningToEntry = false;
};
