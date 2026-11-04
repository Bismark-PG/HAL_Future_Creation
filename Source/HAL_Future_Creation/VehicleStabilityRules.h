#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "Subsystems/WorldSubsystem.h"
#include "VehicleRecoveryTypes.h"
#include "VehicleStabilityRules.generated.h"

class UPhysicalMaterial;
class UPrimitiveComponent;

namespace HALVehicleStability
{
	/** GT-only contact classification. Mobility is permission to move, not proof of motion. */
	HAL_FUTURE_CREATION_API FName ClassifySupportSource(const UPrimitiveComponent* SourceBody, const FVector& ContactPoint);
	HAL_FUTURE_CREATION_API float GetVehicleGravityZ(const UPrimitiveComponent* Body);
	HAL_FUTURE_CREATION_API float FlipAngularSpeed(float VerticalSpeed, float Turns, float GravityZ, float MaximumSpeed);
}

/** Pure contact policy; authority resolves it before queuing the existing physics action. */
struct HAL_FUTURE_CREATION_API FVehicleContactStabilityRules
{
	bool bSuppressSupportSupplement = false;
	float MinimumSupportUpDot = 1;
	float MaximumWallUpDot = 0.5f;
	float WallDropNormalDeltaSpeed = 0;
	bool SuppressesSupplement(bool bOrdinaryStationaryEnvironment, const FVector& Normal, const FVector& ImpulseDirection) const;
	bool IsOrdinaryWall(bool bOrdinaryStationaryEnvironment, const FVector& Normal) const;
	bool AllowsWallDrop(bool bOrdinaryStationaryEnvironment, const FVector& Normal, float NormalDeltaSpeed) const;
};

/** Shared vehicle contact, pose and recovery rules. */
UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UVehicleStabilityRules : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category = "Validation")
	bool bReadyForUse = false;
	/** All vehicle collision roots; free balls keep world gravity. Frozen until a new world starts. */
	UPROPERTY(EditDefaultsOnly, Category = "Gravity", meta = (ClampMin = "0.1", ClampMax = "5"))
	float GravityScale = 1;
	/** Applied to vehicle collision roots only; does not replace the ball or arena material. */
	UPROPERTY(EditDefaultsOnly, Category = "Contact")
	TObjectPtr<UPhysicalMaterial> VehiclePhysicalMaterial;
	/** Preserve impact tier/drop, but add no rule velocity on ordinary stationary WorldStatic support. */
	UPROPERTY(EditDefaultsOnly, Category = "Contact")
	bool bSuppressSupportSupplement = true;
	/** Maximum slope from world up; both geometric normal and impulse must qualify. */
	UPROPERTY(EditDefaultsOnly, Category = "Contact", meta = (ClampMin = "0", ClampMax = "80", Units = "deg"))
	float MaximumSupportSlopeDegrees = 45;
	/** Additional drop gate for ordinary stationary walls. Does not scale collision physics or tiers. */
	UPROPERTY(EditDefaultsOnly, Category = "Wall Ball Drop", meta = (ClampMin = "0", ClampMax = "5000", Units = "cm/s"))
	float WallDropNormalDeltaSpeed = 600;
	/** Surface slope from world up; excludes ordinary floor/ramp support and ceilings. */
	UPROPERTY(EditDefaultsOnly, Category = "Wall Ball Drop", meta = (ClampMin = "45", ClampMax = "90", Units = "deg"))
	float MinimumWallSlopeDegrees = 60;
	UPROPERTY(EditDefaultsOnly, Category = "Recovery") FVehicleRecoveryConfig Recovery;
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
	const FVehicleRecoveryConfig& GetRecoveryRules() const { return RecoveryRules; }
	UPhysicalMaterial* GetVehicleMaterial() const { return Material; }
	uint32 GetSignature() const { return Signature; }
	void ConfigureVehicleGravity(UPrimitiveComponent* Body) const;
	float GetVehicleGravityZ() const { return VehicleGravityZ; }
private:
	bool bInitialized = false;
	bool bValid = false;
	FVehicleContactStabilityRules ContactRules;
	FVehicleRecoveryConfig RecoveryRules;
	uint32 Signature = 0;
	bool bUseVehicleGravity = false;
	float VehicleGravityZ = -980;
	UPROPERTY(Transient) TObjectPtr<UPhysicalMaterial> Material;
};
