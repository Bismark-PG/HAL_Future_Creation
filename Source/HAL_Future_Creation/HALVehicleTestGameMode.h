#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HALVehicleTestGameMode.generated.h"

class ATestVehiclePawn;
class AHALSkillTestDummy;
class AHALVehicleTestPlayerController;

/** Local vehicle inspection only: no room, countdown, match timer or automatic travel. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALVehicleTestGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AHALVehicleTestGameMode();
	/** Test-only integration boundary, independent of map names and room state. */
	static bool IsTestWorld(const UWorld* World);
	virtual void InitGameState() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& Id, FString& Error) override;
	TArray<TSubclassOf<ATestVehiclePawn>> GetTestVehicleClasses() const;
	bool ChangeTestVehicle(AHALVehicleTestPlayerController* Controller, TSubclassOf<ATestVehiclePawn> VehicleClass);
	const FString& GetTestStatus() const { return TestStatus; }
	/** Second, undriven car for collision tests: a skill dummy built from the chosen car's own Definition.
	 * Spawned ahead of the player's car, replacing the previous opponent. The player's car is never touched. */
	bool ChangeOpponentVehicle(AHALVehicleTestPlayerController* Controller, TSubclassOf<ATestVehiclePawn> VehicleClass);
	void RemoveOpponentVehicle();
	AHALSkillTestDummy* GetOpponentVehicle() const;
	const FString& GetOpponentStatus() const { return OpponentStatus; }

protected:
	/** Default Pawn Class is always included. Add other existing vehicle BPs here for runtime switching. */
	UPROPERTY(EditDefaultsOnly, Category="Vehicle Test")
	TArray<TSubclassOf<ATestVehiclePawn>> TestVehicleClasses;
	/** Actor class used for the opponent. The native dummy needs no Blueprint; its car type comes from the panel. */
	UPROPERTY(EditDefaultsOnly, Category="Vehicle Test|Opponent")
	TSubclassOf<AHALSkillTestDummy> OpponentClass;
	/** Measured from the player's car along its heading; the opponent faces back toward the player. */
	UPROPERTY(EditDefaultsOnly, Category="Vehicle Test|Opponent", meta=(Units="cm", ClampMin="200", ClampMax="5000"))
	float OpponentSpawnDistanceCm = 700;
	/** Raised above the player's car height so the body settles onto the floor instead of starting inside it. */
	UPROPERTY(EditDefaultsOnly, Category="Vehicle Test|Opponent", meta=(Units="cm", ClampMin="0", ClampMax="500"))
	float OpponentSpawnLiftCm = 30;

private:
	friend struct FHALVehicleTestAccess;
	TWeakObjectPtr<AHALVehicleTestPlayerController> TestController;
	FString TestStatus;
	UPROPERTY(Transient) TObjectPtr<AHALSkillTestDummy> Opponent;
	FString OpponentStatus = TEXT("none");
};
