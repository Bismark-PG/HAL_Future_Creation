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

HAL_FUTURE_CREATION_API DECLARE_LOG_CATEGORY_EXTERN(LogHALAbility, Log, All);
namespace HALAbilities
{
	HAL_FUTURE_CREATION_API bool ShouldLog();
}
