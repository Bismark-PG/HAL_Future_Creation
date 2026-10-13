// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleHealthComponent.generated.h"

struct FCombatResolver;
struct FVehicleHealthConfig;
struct FVehicleHitContext;
struct FOnAttributeChangeData;
class UHALAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVehicleHealthChanged, float, CurrentHP, float, MaxHP);

/** Configuration seed and read-only health notifications backed by the owner's GAS attributes. */
UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent))
class HAL_FUTURE_CREATION_API UVehicleHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleHealthComponent();

	/** Initialization only; never resets gameplay state after BeginPlay. */
	bool ApplyConfiguration(const FVehicleHealthConfig& Config);

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* Property) const override;
#endif

	bool BindToAbilitySystem(UHALAbilitySystemComponent* InAbilitySystem);
	void UnbindFromAbilitySystem();
	UHALAbilitySystemComponent* GetBoundAbilitySystem() const { return AbilitySystem.Get(); }
	bool IsHealthReady() const;

	UFUNCTION(BlueprintPure, Category = "Vehicle|Health")
	float GetCurrentHP() const;

	UFUNCTION(BlueprintPure, Category = "Vehicle|Health")
	float GetMaxHP() const;

	UPROPERTY(BlueprintAssignable, Category = "Vehicle|Health")
	FVehicleHealthChanged OnHealthChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Saved initialization seed, not runtime HP. Preserved for existing Blueprint/DA configuration. */
	UPROPERTY(EditDefaultsOnly, Category = "Vehicle|Health", meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "500.0"))
	float MaxHP = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vehicle|Health|Debug")
	bool bLogHealthChanges = false;

private:
	friend struct FCombatResolver;

	bool ApplyResolvedDamage(const FVehicleHitContext& Hit, float Damage);
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	void PublishHealth();
	TWeakObjectPtr<UHALAbilitySystemComponent> AbilitySystem;
	FDelegateHandle CurrentHPChangedHandle;
	FDelegateHandle MaxHPChangedHandle;
	FDelegateHandle HealthInitializedHandle;
};
