// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "HALAbilityTypes.h"
#include "HALPlayerState.generated.h"

class UHALAbilitySystemComponent;
class UHALHealthAttributeSet;
class UHALCombatAttributeSet;
class UHALImpactAttributeSet;
class UHALMovementAttributeSet;

/** Stable player identity and GAS Owner; the current vehicle is its Avatar. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	AHALPlayerState();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UHALAbilitySystemComponent* GetHALAbilitySystemComponent() const { return AbilitySystem; }
	/** Called after Avatar binding, authority only. Existing specs survive a repeated bind. */
	void GrantVehicleAbilities();

protected:
	/** Initial player loadout. Additional grant sources can use the same ASC contract later. */
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Abilities", meta = (TitleProperty = "InputTag"))
	TArray<FHALAbilityGrant> DefaultAbilities;

private:
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UHALAbilitySystemComponent> AbilitySystem;
	UPROPERTY()
	TObjectPtr<UHALHealthAttributeSet> HealthAttributes;
	UPROPERTY()
	TObjectPtr<UHALCombatAttributeSet> CombatAttributes;
	UPROPERTY()
	TObjectPtr<UHALImpactAttributeSet> ImpactAttributes;
	UPROPERTY()
	TObjectPtr<UHALMovementAttributeSet> MovementAttributes;
};
