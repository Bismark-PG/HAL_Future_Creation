#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HALImpactTypes.h"
#include "HALImpactSourceComponent.generated.h"

UENUM(BlueprintType)
enum class EHALCollisionSourceMode : uint8 { AutomaticCap, FixedTier };

/** Optional wall/gimmick collision configuration. No ASC, Tick or gameplay Blueprint graph. */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class HAL_FUTURE_CREATION_API UHALImpactSourceComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALImpactSourceComponent() { PrimaryComponentTick.bCanEverTick = false; }
	UPROPERTY(EditAnywhere, Category = "Impact")
	EHALCollisionSourceMode Mode = EHALCollisionSourceMode::AutomaticCap;
	/** Upper bound in automatic mode, exact source tier in fixed mode. */
	UPROPERTY(EditAnywhere, Category = "Impact")
	EHALImpactTier Tier = EHALImpactTier::Heavy;
};
