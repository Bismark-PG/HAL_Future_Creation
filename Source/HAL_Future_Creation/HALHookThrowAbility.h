#pragma once
#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "GameplayEffect.h"
#include "HALHookThrowTypes.h"
#include "HALHookThrowAbility.generated.h"

class AHALHookThrow;
UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALHookThrowConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,Category="Timing",meta=(ClampMin="0",ClampMax="120",Units="s")) float CooldownSeconds=12;
	UPROPERTY(EditAnywhere,Category="Gameplay") FHALHookRules Rules;
	UPROPERTY(EditAnywhere,Category="Presentation") FHALHookFX FX;
	bool Validate(FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
UCLASS()
class HAL_FUTURE_CREATION_API UHALHookCooldownEffect : public UGameplayEffect
{ GENERATED_BODY() public: UHALHookCooldownEffect(); };
UCLASS()
class HAL_FUTURE_CREATION_API UHALHookCastingEffect : public UGameplayEffect
{ GENERATED_BODY() public: UHALHookCastingEffect(); };
/** Separate handle/asset identity; grants the ordinary Stun tag and therefore ordinary immediate ball drop. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALHookCapturedEffect : public UGameplayEffect
{ GENERATED_BODY() public: UHALHookCapturedEffect(); };
UCLASS()
class HAL_FUTURE_CREATION_API UHALHookThrowAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UHALHookThrowAbility();
	UPROPERTY(EditDefaultsOnly,Category="Hook Throw") TObjectPtr<UHALHookThrowConfig> SkillConfig;
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle,const FGameplayAbilityActorInfo*,const FGameplayTagContainer*,const FGameplayTagContainer*,FGameplayTagContainer*) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle,const FGameplayAbilityActorInfo*,FGameplayAbilityActivationInfo,const FGameplayEventData*) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle,const FGameplayAbilityActorInfo*,FGameplayAbilityActivationInfo,bool,bool) override;
protected:
	virtual void ApplyCooldown(FGameplayAbilitySpecHandle,const FGameplayAbilityActorInfo*,FGameplayAbilityActivationInfo) const override;
private:
	bool Ready(const FGameplayAbilityActorInfo*) const;
	UFUNCTION() void HookDestroyed(AActor* Actor);
	TWeakObjectPtr<AHALHookThrow> ActiveHook;
	mutable FActiveGameplayEffectHandle CommittedCooldown;
};
