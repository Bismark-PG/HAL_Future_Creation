#pragma once
#include "Commandlets/Commandlet.h"
#include "HALBuildArrestChainsFXCommandlet.generated.h"

/** Native art authoring. Only new chain resources and the skill DA's new chain references are saved. */
UCLASS()
class UHALBuildArrestChainsFXCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params) override;
};
