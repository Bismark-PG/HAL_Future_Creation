#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HALAbilityTypes.generated.h"

class UGameplayAbility;

/** One owner-scoped grant. Input identity is independent of the concrete GA class. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALAbilityGrant
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	TSubclassOf<UGameplayAbility> Ability;
	UPROPERTY(EditDefaultsOnly, Category = "Ability", meta = (Categories = "Input.Ability"))
	FGameplayTag InputTag;
	UPROPERTY(EditDefaultsOnly, Category = "Ability", meta = (ClampMin = "1"))
	int32 Level = 1;
};

UENUM(BlueprintType)
enum class EHALVehicleAbilityVersion : uint8 { LegacyPlayerState, VehicleDefinitionV1 };

/** Explicit migration: an empty V1 loadout never falls back to the legacy player list. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALVehicleAbilityConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Abilities")
	EHALVehicleAbilityVersion Version = EHALVehicleAbilityVersion::LegacyPlayerState;
	/** Game seconds to regenerate 0 to 100 points during play. Also applies to legacy player grants. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ultimate Energy", meta=(Units="s", ClampMin="0.1", ClampMax="600"))
	float FullRechargeSeconds = 20.f;
	/** Current input protocol supports exactly one Primary. Other slots require a routing extension. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Abilities", meta=(TitleProperty="InputTag", EditCondition="Version == EHALVehicleAbilityVersion::VehicleDefinitionV1"))
	TArray<FHALAbilityGrant> Grants;
	bool Validate(FString& Error) const;
};

HAL_FUTURE_CREATION_API DECLARE_LOG_CATEGORY_EXTERN(LogHALAbility, Log, All);
namespace HALAbilities
{
	HAL_FUTURE_CREATION_API bool ShouldLog();
	HAL_FUTURE_CREATION_API bool ValidateGrants(const TArray<FHALAbilityGrant>& Grants, FString& Error);
}
