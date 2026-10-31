#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleRecoveryTypes.h"
#include "VehicleRecoveryComponent.generated.h"

struct FHALCombatHitRequest;

/** Authority game-time permission clock. Physics receives discrete, replayable phase actions. */
UCLASS(ClassGroup = (Vehicle))
class HAL_FUTURE_CREATION_API UVehicleRecoveryComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UVehicleRecoveryComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UFUNCTION(BlueprintPure, Category = "Vehicle|Recovery") FVehicleRecoveryStatus GetStatus() const { return Status; }
	bool IsExecuting() const { return Status.Phase == EVehicleRecoveryPhase::Executing; }
	void CancelForOwnerExit();
	void ResumeForOwner();
	void NotifyExternalImpact(const FHALCombatHitRequest& Request, EHALImpactTier EffectiveTier);
private:
	friend struct FHALStunTestAccess;
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALRecoveryTestAccess;
#endif
	void UpdateRecovery();
	bool Start(const FVehicleStabilityObservation& Sample);
	void Finish(FName Outcome);
	void LogTransition(EVehicleRecoveryPhase Before, FName PreviousBlockReason = NAME_None) const;
	FVehicleRecoveryConfig Rules;
	FVehicleRecoveryClock Clock;
	FTimerHandle Timer;
	double LastUpdateTime = 0, LastPhysicsTime = 0;
	int32 ObservedPhysicsFrame = INDEX_NONE;
	uint32 NextSequence = 0;
	bool bOwnerExiting = false;
	UPROPERTY(Replicated) FVehicleRecoveryStatus Status;
};
