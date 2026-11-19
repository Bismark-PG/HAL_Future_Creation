#pragma once
#include "HALVehicleUltimateAbility.h"
#include "Engine/DataAsset.h"
#include "GameplayEffect.h"
#include "HALPolicePursuitTypes.h"
#include "HALPolicePursuitAbility.generated.h"
class AHALPolicePursuit;
UCLASS()
class HAL_FUTURE_CREATION_API UHALPolicePursuitConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	/** Legacy serialized value; recharge is authored on the vehicle. */
	UPROPERTY() float CooldownSeconds=18;
	UPROPERTY(EditAnywhere,Category="Pursuit") FHALPursuitRules Rules;
	UPROPERTY(EditAnywhere,Category="Presentation") FHALPursuitFX FX;
	bool Validate(FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
UCLASS() class UHALPursuitCooldownEffect : public UGameplayEffect { GENERATED_BODY() public: UHALPursuitCooldownEffect(); };
UCLASS() class UHALPursuitArrestEffect : public UGameplayEffect { GENERATED_BODY() public: UHALPursuitArrestEffect(); };
UCLASS() class UHALPursuitProtectionEffect : public UGameplayEffect { GENERATED_BODY() public: UHALPursuitProtectionEffect(); };
UCLASS()
class HAL_FUTURE_CREATION_API UHALPolicePursuitAbility : public UHALVehicleUltimateAbility
{
	GENERATED_BODY()
public:
	UHALPolicePursuitAbility();
	UPROPERTY(EditDefaultsOnly,Category="Police Pursuit") TObjectPtr<UHALPolicePursuitConfig> SkillConfig;
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle H,const FGameplayAbilityActorInfo* I,const FGameplayTagContainer* S,const FGameplayTagContainer* T,FGameplayTagContainer* F) const override;
	virtual EHALUltimateStart StartUltimate(FGameplayAbilitySpecHandle H,const FGameplayAbilityActorInfo* I,FGameplayAbilityActivationInfo A,const FGameplayEventData*) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle H,const FGameplayAbilityActorInfo* I,FGameplayAbilityActivationInfo A,bool R,bool C) override;
private:
	bool Ready(const FGameplayAbilityActorInfo* I) const;
	UFUNCTION() void PursuitEnded(AActor* Actor);
	TWeakObjectPtr<AHALPolicePursuit> Active;
};
