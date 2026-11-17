#pragma once
#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "GameplayEffect.h"
#include "HALSmokeScreenTypes.h"
#include "HALSmokeScreenAbility.generated.h"

class AHALSmokeScreen;
UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALSmokeScreenConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,Category="Timing",meta=(Units="s",ClampMin="0",ClampMax="120")) float CooldownSeconds=18;
	UPROPERTY(EditAnywhere,Category="Smoke") FHALSmokeScreenRules Rules;
	UPROPERTY(EditAnywhere,Category="Presentation") FHALSmokeScreenFX FX;
	bool Validate(FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
UCLASS()
class HAL_FUTURE_CREATION_API UHALSmokeScreenCooldownEffect : public UGameplayEffect
{ GENERATED_BODY() public: UHALSmokeScreenCooldownEffect(); };
/** One contribution per cloud source. Leaving one source cannot clear another source's Tag. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALSmokeObscuredEffect : public UGameplayEffect
{ GENERATED_BODY() public: UHALSmokeObscuredEffect(); };
UCLASS()
class HAL_FUTURE_CREATION_API UHALSmokeScreenAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UHALSmokeScreenAbility();
	UPROPERTY(EditDefaultsOnly,Category="Smoke Screen") TObjectPtr<UHALSmokeScreenConfig> SkillConfig;
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,
		const FGameplayTagContainer* Source,const FGameplayTagContainer* Target,FGameplayTagContainer* Reasons) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation,const FGameplayEventData*) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation,bool bReplicate,bool bCancelled) override;
protected:
	virtual void ApplyCooldown(FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,FGameplayAbilityActivationInfo Activation) const override;
private:
	bool ResolveStart(const FGameplayAbilityActorInfo* Info,FVector& Ground) const;
	UFUNCTION() void SmokeDestroyed(AActor* Actor);
	TWeakObjectPtr<AHALSmokeScreen> ActiveSmoke;
	mutable FActiveGameplayEffectHandle CommittedCooldown;
};
