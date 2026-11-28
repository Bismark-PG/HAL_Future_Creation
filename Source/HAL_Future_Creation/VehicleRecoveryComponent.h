#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleRecoveryTypes.h"
#include "VehicleRecoveryComponent.generated.h"

struct FHALCombatHitRequest;
class UHALRecoveryPromptWidget;

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
	void PressRecovery();
	int32 GetRequiredPresses() const { return Rules.RequiredPresses; }
	float GetDisplayProgress() const;
	double GetLastLocalPressTime() const { return LastLocalPressTime; }
	double GetExecutionCueTime() const { return ExecutionCueTime; }
	void CancelForOwnerExit();
	void ResumeForOwner();
	void NotifyExternalImpact(const FHALCombatHitRequest& Request, EHALImpactTier EffectiveTier);
private:
	friend struct FHALStunTestAccess;
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALRecoveryTestAccess;
	friend struct FHALPursuitTestAccess;
#endif
	void UpdateRecovery();
	void EvaluateRecovery(bool bPressed);
	UFUNCTION(Server, Reliable) void ServerPressRecovery(uint32 PressId);
	UFUNCTION() void OnRep_Status();
	void RefreshLocalPrompt(bool bAnticipate = false);
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
	bool bRulesReady = false;
	uint32 LocalPressId = 0;
	TArray<uint32, TInlineAllocator<8>> PendingDisplayPresses;
	double LastLocalPressTime = -100, ExecutionCueTime = -100;
	bool bShowingExecution = false;
	UPROPERTY(Transient) TObjectPtr<UHALRecoveryPromptWidget> Prompt;
	UPROPERTY(ReplicatedUsing=OnRep_Status) FVehicleRecoveryStatus Status;
};
