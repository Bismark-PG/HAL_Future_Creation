#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "Subsystems/WorldSubsystem.h"
#include "VehicleStabilityRules.generated.h"

class UPhysicalMaterial;

/** Pure contact policy; authority resolves it before queuing the existing physics action. */
struct HAL_FUTURE_CREATION_API FVehicleContactStabilityRules
{
	bool bSuppressSupportSupplement = false;
	float MinimumSupportUpDot = 1;
	bool SuppressesSupplement(bool bOrdinaryStaticEnvironment, const FVector& Normal, const FVector& ImpulseDirection) const;
};

/** Shared vehicle contact rules. Self-righting parameters will be added with that implementation. */
UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UVehicleStabilityRules : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category = "Validation")
	bool bReadyForUse = false;
	/** Applied to vehicle collision roots only; does not replace the ball or arena material. */
	UPROPERTY(EditDefaultsOnly, Category = "Contact")
	TObjectPtr<UPhysicalMaterial> VehiclePhysicalMaterial;
	/** Preserve impact tier/drop, but add no rule velocity on ordinary static support. */
	UPROPERTY(EditDefaultsOnly, Category = "Contact")
	bool bSuppressSupportSupplement = true;
	/** Maximum slope from world up; both geometric normal and impulse must qualify. */
	UPROPERTY(EditDefaultsOnly, Category = "Contact", meta = (ClampMin = "0", ClampMax = "80", Units = "deg"))
	float MaximumSupportSlopeDegrees = 45;
	bool Validate(FString& Error) const;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Vehicle Stability"))
class HAL_FUTURE_CREATION_API UVehicleStabilitySettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	/** Explicit migration switch. Stop the match, save changes, then start a new world. */
	UPROPERTY(Config, EditAnywhere, Category = "Shared Rules")
	bool bUseSharedRules = false;
	UPROPERTY(Config, EditAnywhere, Category = "Shared Rules", meta = (EditCondition = "bUseSharedRules"))
	TSoftObjectPtr<UVehicleStabilityRules> SharedRules;
};

/** One immutable contact/material snapshot per game world, shared by all of its vehicles. */
UCLASS()
class HAL_FUTURE_CREATION_API UVehicleStabilitySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	bool InitializeRules();
	const FVehicleContactStabilityRules& GetContactRules() const { return ContactRules; }
	UPhysicalMaterial* GetVehicleMaterial() const { return Material; }
	uint32 GetSignature() const { return Signature; }
private:
	bool bInitialized = false;
	bool bValid = false;
	FVehicleContactStabilityRules ContactRules;
	uint32 Signature = 0;
	UPROPERTY(Transient) TObjectPtr<UPhysicalMaterial> Material;
};
