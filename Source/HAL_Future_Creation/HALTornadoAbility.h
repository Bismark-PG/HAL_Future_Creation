#pragma once
#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "GameplayEffect.h"
#include "HALTornadoTypes.h"
#include "HALTornadoAbility.generated.h"
class AHALTornado;
UCLASS()
class HAL_FUTURE_CREATION_API UHALTornadoConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Timing", meta=(ClampMin="0", ClampMax="120", Units="s")) float CooldownSeconds = 16;
	UPROPERTY(EditAnywhere, Category="Tornado") FHALTornadoRules Rules;
	UPROPERTY(EditAnywhere, Category="Presentation") FHALTornadoFX FX;
	bool Validate(FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
UCLASS()
class UHALTornadoCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UHALTornadoCooldownEffect();
};
UCLASS()
class HAL_FUTURE_CREATION_API UHALTornadoAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UHALTornadoAbility();
	UPROPERTY(EditDefaultsOnly, Category="Tornado") TObjectPtr<UHALTornadoConfig> SkillConfig;
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayTagContainer* Source, const FGameplayTagContainer* Target, FGameplayTagContainer* Failure) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
	virtual void ApplyCooldown(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, FGameplayAbilityActivationInfo Activation) const override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, bool bReplicate, bool bCancelled) override;
private:
	bool ResolveStart(const FGameplayAbilityActorInfo* Info, FVector& Ground) const;
	UFUNCTION() void TornadoDestroyed(AActor* Actor);
	void GameplayEnded(AHALTornado* Tornado);
	TWeakObjectPtr<AHALTornado> ActiveTornado;
	mutable FActiveGameplayEffectHandle CommittedCooldown;
};
