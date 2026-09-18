// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleHealthComponent.generated.h"

struct FCombatResolver;
struct FVehicleHealthConfig;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVehicleHealthChanged, float, CurrentHP, float, MaxHP);

/** Authority-owned MVP-A HP; physics and collision callbacks never write HP directly. */
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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Vehicle|Health")
	float GetCurrentHP() const { return CurrentHP; }

	UFUNCTION(BlueprintPure, Category = "Vehicle|Health")
	float GetMaxHP() const { return MaxHP; }

	UPROPERTY(BlueprintAssignable, Category = "Vehicle|Health")
	FVehicleHealthChanged OnHealthChanged;

protected:
	virtual void BeginPlay() override;

	/** Starting HP; damage is clamped to [0, MaxHP]. */
	UPROPERTY(EditDefaultsOnly, Category = "Vehicle|Health", meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "500.0"))
	float MaxHP = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vehicle|Health|Debug")
	bool bLogHealthChanges = false;

private:
	friend struct FCombatResolver;

	bool ApplyResolvedDamage(float Damage);

	UFUNCTION()
	void OnRep_CurrentHP();

	UPROPERTY(ReplicatedUsing = OnRep_CurrentHP, VisibleInstanceOnly, Category = "Vehicle|Health")
	float CurrentHP = 100.0f;
};
