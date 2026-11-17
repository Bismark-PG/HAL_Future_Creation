#pragma once
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "HALHookThrowTypes.h"
#include "HALHookThrow.generated.h"

class ATestVehiclePawn;
class UHALHookThrowConfig;
class UHALAbilitySystemComponent;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

USTRUCT()
struct FHALHookState
{
	GENERATED_BODY()
	UPROPERTY() FGuid Activation;
	UPROPERTY() uint32 Revision=0;
	UPROPERTY() EHALHookPhase Phase=EHALHookPhase::Finished;
	UPROPERTY() double StartTime=0;
	UPROPERTY() double PhaseTime=0;
	UPROPERTY() FVector Point=FVector::ZeroVector;
	UPROPERTY() FVector Forward=FVector::ForwardVector;
	UPROPERTY() FVector Right=FVector::RightVector;
	UPROPERTY() TObjectPtr<ATestVehiclePawn> Target;
	UPROPERTY() FVector TargetOffset=FVector::ZeroVector;
};

/** Authority sweep/claim/lifecycle; clients only reconstruct the chain and hook from server time. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALHookThrow : public AActor
{
	GENERATED_BODY()
public:
	AHALHookThrow();
	static bool CanStart(const ATestVehiclePawn& Pawn);
	bool Start(const UHALHookThrowConfig& Config);
	void AcceptDirection(FGuid Activation,uint32 Sequence,float Value);
	void Interrupt(bool OwnerExit);
	void TargetLost();
	bool LocksCaster() const;
	EHALHookPhase GetPhase() const { return State.Phase; }
	FGuid GetActivation() const { return State.Activation; }
	double ServerTime() const;
	FVector TipAt(double Now) const;
	FVector Outlet() const;
	virtual void Tick(float Delta) override;
	virtual void Destroyed() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALHookTestAccess;
	friend class FHALHookMetalRenderTest;
#endif
	bool CasterValid() const;
	bool IsInterrupted() const;
	bool Capture(ATestVehiclePawn& Target,const FVector& Point);
	void StartSwing(int32 Side);
	void ReleaseTarget(bool Throw);
	void Return(bool Interrupted);
	void ClearCaster();
	void Cleanup();
	void Sweep(const FVector& From,const FVector& To);
	void CasterStun(FGameplayTag Tag,int32 Count);
	void UpdateVisual();
	void PublishState(bool Reliable=false);
	void AcceptReplicatedState(const FHALHookState& Value);
	UFUNCTION(NetMulticast,Reliable) void MulticastPhase(FHALHookState Value);
	UFUNCTION() void OnRep_State();
	UPROPERTY(ReplicatedUsing=OnRep_State) FHALHookState State;
	UPROPERTY(Transient) FHALHookState AcceptedState;
	UPROPERTY(Replicated) FHALHookRules Rules;
	UPROPERTY(Replicated) FHALHookFX FX;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Chain;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Hook;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Metal;
	TWeakObjectPtr<UHALAbilitySystemComponent> CasterASC,TargetASC;
	FActiveGameplayEffectHandle CastingEffect,CaptureEffect;
	FDelegateHandle StunHandle;
	FVector PreviousTip=FVector::ZeroVector,TargetAnchor=FVector::ZeroVector,CapturePivot=FVector::ZeroVector;
	uint32 LastDirectionSequence=0;
	bool bDirectionArmed=false,bCleaning=false;
};
