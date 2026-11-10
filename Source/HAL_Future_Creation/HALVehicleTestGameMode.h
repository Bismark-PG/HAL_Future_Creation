#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HALVehicleTestGameMode.generated.h"

class ATestVehiclePawn;
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

protected:
	/** Default Pawn Class is always included. Add other existing vehicle BPs here for runtime switching. */
	UPROPERTY(EditDefaultsOnly, Category="Vehicle Test")
	TArray<TSubclassOf<ATestVehiclePawn>> TestVehicleClasses;

private:
	friend struct FHALVehicleTestAccess;
	TWeakObjectPtr<AHALVehicleTestPlayerController> TestController;
	FString TestStatus;
};
