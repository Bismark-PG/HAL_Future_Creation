#pragma once
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "HALSmokeScreenTypes.h"
#include "HALSmokeScreen.generated.h"

class ATestVehiclePawn;
class UHALSmokeScreenConfig;
class UAbilitySystemComponent;
class UNiagaraComponent;

/** Authority-owned smoke field; cosmetics are reconstructed locally from bounded replicated samples. */
UCLASS(NotBlueprintable)
class HAL_FUTURE_CREATION_API AHALSmokeScreen : public AActor
{
	GENERATED_BODY()
public:
	AHALSmokeScreen();
	bool Start(const UHALSmokeScreenConfig& Config,const FVector& Ground);
	static bool FindOutletGround(const ATestVehiclePawn& Vehicle,const FHALSmokeScreenRules& Rules,FVector& Ground);
	static bool ClearLine(const UWorld& World,const AActor* Ignore,const FVector& From,const FVector& To);
	/** Same query on authority and clients; owner immunity only applies to the extra local screen veil. */
	float Influence(const AActor& Target,bool bScreen,double Now,const FVector* Previous=nullptr) const;
	const TArray<FHALSmokePuff>& GetPuffs() const { return Puffs; }
	const FHALSmokeScreenRules& GetRules() const { return Rules; }
	const FHALSmokeScreenFX& GetFX() const { return FX; }
	double GetStartTime() const { return StartTime; }
	bool IsEmitting() const { return bEmitting; }
	DECLARE_MULTICAST_DELEGATE_OneParam(FEmissionEnded, AHALSmokeScreen*);
	FEmissionEnded OnEmissionEnded;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Destroyed() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHALSmokeTestAccess;
#endif
	UFUNCTION() void OnRep_Smoke();
	void Update();
	void AddPuff(const FVector& Ground,float Age);
	void ClipPuff(FHALSmokePuff& Puff);
	void UpdateReceivers();
	void ClearReceivers();
	void Cleanup();
	void UpdateJet();
	UPROPERTY(ReplicatedUsing=OnRep_Smoke) FHALSmokeScreenRules Rules;
	UPROPERTY(ReplicatedUsing=OnRep_Smoke) FHALSmokeScreenFX FX;
	UPROPERTY(ReplicatedUsing=OnRep_Smoke) double StartTime=-1;
	UPROPERTY(ReplicatedUsing=OnRep_Smoke) TArray<FHALSmokePuff> Puffs;
	UPROPERTY(ReplicatedUsing=OnRep_Smoke) bool bEmitting=false;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> Jet;
	TMap<TWeakObjectPtr<UAbilitySystemComponent>,FActiveGameplayEffectHandle> Receivers;
	TMap<TWeakObjectPtr<ATestVehiclePawn>,FVector> LastPositions;
	FTimerHandle UpdateTimer;
	double LastWallRefresh=-1;
	bool bEmissionWindowEnded=false;
};
