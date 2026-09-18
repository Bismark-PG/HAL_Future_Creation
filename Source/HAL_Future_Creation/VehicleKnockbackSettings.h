// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VehicleDefinition.h"
#include "VehicleKnockbackSettings.generated.h"

/** Project-wide impact thresholds and tier outcomes shared by every standard ball. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Vehicle Knockback"))
class HAL_FUTURE_CREATION_API UVehicleKnockbackSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UVehicleKnockbackSettings();

	bool InitializeRules(UWorld* World);
	bool GetRuntimeRules(FVehicleKnockbackConfig& OutRules) const;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
	virtual bool CanEditChange(const FProperty* Property) const override;
#endif

	UPROPERTY(Config, EditAnywhere, Category = "Configuration")
	EVehicleConfigurationSource ConfigurationSource = EVehicleConfigurationSource::Legacy;

	UPROPERTY(Config, EditAnywhere, Category = "Configuration", meta = (EditCondition = "ConfigurationSource == EVehicleConfigurationSource::Definition"))
	TSoftObjectPtr<UCombatRulesDefinition> CombatRules;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Medium begins at this equivalent collision speed. */
	UPROPERTY(Config, EditAnywhere, Category = "Thresholds", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MediumImpactSpeedThreshold = 1000.0f;

	/** Heavy begins here; runtime clamps it to at least the Medium threshold. */
	UPROPERTY(Config, EditAnywhere, Category = "Thresholds", meta = (ClampMin = "0.0", Units = "cm/s"))
	float HeavyImpactSpeedThreshold = 2600.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Tier Outcomes")
	FVehicleKnockbackTierDefinition LightTier;

	UPROPERTY(Config, EditAnywhere, Category = "Tier Outcomes")
	FVehicleKnockbackTierDefinition MediumTier;

	UPROPERTY(Config, EditAnywhere, Category = "Tier Outcomes")
	FVehicleKnockbackTierDefinition HeavyTier;

	/** Hard cap for tier-authored Pitch/Roll angular speed. */
	UPROPERTY(Config, EditAnywhere, Category = "Stability", meta = (ClampMin = "0.0", Units = "rad/s"))
	float MaxAirborneAngularSpeed = 8.0f;

	/** Extra distance below the target bounds used to recognize a grounded Light hit. */
	UPROPERTY(Config, EditAnywhere, Category = "Stability", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundProbeExtraDistance = 20.0f;
private:
	UPROPERTY(Transient)
	TObjectPtr<UCombatRulesDefinition> CachedDefinition;
	FVehicleKnockbackConfig CachedRules;
	TWeakObjectPtr<UWorld> CachedWorld;
	bool bRulesInitialized = false;
	bool bRulesValid = false;
};
