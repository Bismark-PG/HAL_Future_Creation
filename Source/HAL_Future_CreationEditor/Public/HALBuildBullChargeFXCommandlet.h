#pragma once
#include "Commandlets/Commandlet.h"
#include "HALBuildBullChargeFXCommandlet.generated.h"

/** Creates only new Car04 VFX assets. Applying the recipe requires explicit skill/presentation paths. */
UCLASS()
class UHALBuildBullChargeFXCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UHALBuildBullChargeFXCommandlet();
	virtual int32 Main(const FString& Params) override;
};
