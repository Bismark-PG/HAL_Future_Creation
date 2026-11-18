#pragma once
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "HALPolicePursuitTypes.h"
#include "HALPursuitTargetComponent.generated.h"
class AHALPolicePursuit; class UHALAbilitySystemComponent; class UInstancedStaticMeshComponent; class UMaterialInstanceDynamic;
USTRUCT()
struct FHALPursuitTargetState
{
	GENERATED_BODY()
	UPROPERTY() bool bArrested=false;
	UPROPERTY() bool bProtected=false;
	UPROPERTY() double ProtectionEnd=-1;
	UPROPERTY() double ArrestStart=-1;
	UPROPERTY() double ReleaseStart=-1;
	UPROPERTY() FHALPursuitFX FX;
};
/** Avatar-scoped arrest and shared Car06-only protection. Never gates ordinary Stun. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALPursuitTargetComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALPursuitTargetComponent();
	bool AcceptArrest(AHALPolicePursuit& Source);
	void Clear();
	const FHALPursuitTargetState& GetState() const { return State; }
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Function) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FHALPursuitRenderTest;
	friend struct FHALPursuitTestAccess;
	friend class FHALArrestChainRenderTest;
	friend struct FHALArrestChainTestAccess;
	friend struct FHALArrestChainNetworkAccess;
#endif
	void UpdateProtection();
	void StunChanged(FGameplayTag Tag,int32 Count);
	void UpdateMarker();
	void StopChains();
	double PresentationTime() const;
	UFUNCTION() void OnRep_State();
	UPROPERTY(ReplicatedUsing=OnRep_State) FHALPursuitTargetState State;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Chains;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Material;
	TWeakObjectPtr<UHALAbilitySystemComponent> ASC;
	FActiveGameplayEffectHandle Arrest,Protection;
	FDelegateHandle StunHandle;
	float ProtectionSeconds=2;
	FTimerHandle Timer;
};
