#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "TimerManager.h"
#include "HALSideShiftAbility.h"
#include "HALSideShiftComponent.generated.h"

class UHALAbilitySystemComponent;
class UNiagaraComponent;
struct FOnAttributeChangeData;

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALSideShiftView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bConfigured = false;
	UPROPERTY(BlueprintReadOnly) int32 Charges = 0;
	UPROPERTY(BlueprintReadOnly) int32 MaximumCharges = 0;
	UPROPERTY(BlueprintReadOnly) float RechargeSeconds = 0;
	UPROPERTY(BlueprintReadOnly) double NextRechargeServerTime = -1;
};
USTRUCT()
struct FHALSideShiftVisualState
{
	GENERATED_BODY()
	UPROPERTY() FGuid Activation;
	UPROPERTY() FHALSideShiftFX FX;
	UPROPERTY() double StartServerTime = -1;
	UPROPERTY() float ShiftSeconds = 0;
	UPROPERTY() float DriftSeconds = 0;
	UPROPERTY() FVector Direction = FVector::ZeroVector;
	UPROPERTY() int8 Side = 0;
	UPROPERTY() bool bDriving = false;
};

/** Skill-specific charge resource, input transaction and cosmetic lifetime. No GA Tick. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALSideShiftComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALSideShiftComponent();
	void Initialize(UHALAbilitySystemComponent& ASC,UHALSideShiftConfig& Config);
	void Shutdown();
	bool UsesPrimary() const;
	/** Input intent only; the server chooses direction from its own vehicle pose and executes GAS. */
	bool Request(float Steering);
	bool CanStart() const;
	bool Start();
	void Stop();
	UFUNCTION(BlueprintPure, Category="Side Shift") FHALSideShiftView GetChargeView() const { return ChargeView; }
	UFUNCTION(BlueprintPure, Category="Side Shift") float GetRechargeRemaining() const;
	const FHALTireSkidConfig* GetActiveTireMarks() const;
	bool IsSkidActive() const;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* Function) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALSideShiftTestAccess;
#endif
	UFUNCTION(Server,Reliable) void ServerRequest(int8 Side,uint32 Sequence);
	void ProcessRequest(int8 Side,uint32 Sequence);
	void Recharge();
	void PublishCharges();
	void WatchActive(UWorld* World,ELevelTick TickType,float DeltaTime);
	void TagChanged(FGameplayTag Tag,int32 Count);
	void HealthChanged(const FOnAttributeChangeData& Change);
	bool HasUsablePose() const;
	int32 ResourceCount() const;
	bool AddCharge(int32 Count);
	UFUNCTION() void OnRep_Visual();
	double ServerTime() const;
	void UpdateParticles();
	void ClearParticles(bool bImmediate);
	UPROPERTY(Transient) TObjectPtr<UHALSideShiftConfig> Config;
	TWeakObjectPtr<UHALAbilitySystemComponent> BoundASC;
	FActiveGameplayEffectHandle ResourceEffect, ActiveEffect;
	FDelegateHandle StunHandle, ActiveTagHandle, HealthHandle, WorldHandle;
	FTimerHandle RechargeTimer, EndTimer;
	uint32 NextRequest=0, LastRequest=0;
	int8 PendingSide=0;
	double ReadyAt=-1;
	bool bStopping=false;
	UPROPERTY(Replicated) FHALSideShiftView ChargeView;
	UPROPERTY(ReplicatedUsing=OnRep_Visual) FHALSideShiftVisualState Visual;
	FGuid DrawActivation;
	UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraComponent>> Jets;
	struct FFadingJet { TWeakObjectPtr<UNiagaraComponent> Component; double Deadline=0; };
	TArray<FFadingJet> FadingJets;
	TSet<FName> WarnedMounts;
};
