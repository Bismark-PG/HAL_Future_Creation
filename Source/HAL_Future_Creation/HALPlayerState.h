// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "HALPlayerState.generated.h"

class UHALAbilitySystemComponent;
class UHALHealthAttributeSet;
class UHALCombatAttributeSet;

/** Stable player identity and GAS Owner; the current vehicle is its Avatar. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	AHALPlayerState();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UHALAbilitySystemComponent* GetHALAbilitySystemComponent() const { return AbilitySystem; }

private:
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UHALAbilitySystemComponent> AbilitySystem;
	UPROPERTY()
	TObjectPtr<UHALHealthAttributeSet> HealthAttributes;
	UPROPERTY()
	TObjectPtr<UHALCombatAttributeSet> CombatAttributes;
};
