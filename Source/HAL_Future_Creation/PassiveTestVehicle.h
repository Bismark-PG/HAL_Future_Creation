// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "VehicleConfigurationTypes.h"
#include "HALCombatTypes.h"
#include "PassiveTestVehicle.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UVehicleHealthComponent;
class UHALAbilitySystemComponent;
class UHALHealthAttributeSet;
class UHALCombatAttributeSet;
class UHALImpactAttributeSet;
class UHALImpactReceiverComponent;

/** Pushable, damageable test vehicle without input, movement forces, or ball acquisition. */
class UVehicleDefinition;

UCLASS()
class HAL_FUTURE_CREATION_API APassiveTestVehicle : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	APassiveTestVehicle();

	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	EVehicleConfigurationSource GetConfigurationSource() const { return ConfigurationSource; }
	bool IsConfigurationValid() const { return bConfigurationValid; }

protected:
	UPROPERTY(EditAnywhere, Category = "Vehicle|Configuration")
	EVehicleConfigurationSource ConfigurationSource = EVehicleConfigurationSource::Legacy;

	UPROPERTY(EditAnywhere, Category = "Vehicle|Configuration", meta = (EditCondition = "ConfigurationSource == EVehicleConfigurationSource::Definition"))
	TObjectPtr<UVehicleDefinition> Definition;

	UPROPERTY(VisibleInstanceOnly, Category = "Vehicle|Configuration")
	bool bConfigurationValid = true;

	bool ApplyDefinition(bool bPreview);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Components")
	TObjectPtr<UBoxComponent> CollisionRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Components")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Components")
	TObjectPtr<UVehicleHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UHALAbilitySystemComponent> AbilitySystem;
	UPROPERTY()
	TObjectPtr<UHALHealthAttributeSet> HealthAttributes;
	UPROPERTY()
	TObjectPtr<UHALCombatAttributeSet> CombatAttributes;
	UPROPERTY()
	TObjectPtr<UHALImpactAttributeSet> ImpactAttributes;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle|Components")
	TObjectPtr<UHALImpactReceiverComponent> ImpactReceiver;
	FVehicleCombatConfig RuntimeCombatConfig;
};
