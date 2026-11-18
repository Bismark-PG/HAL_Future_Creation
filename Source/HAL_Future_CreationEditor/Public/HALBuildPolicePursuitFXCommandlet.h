#pragma once
#include "Commandlets/Commandlet.h"
#include "HALBuildPolicePursuitFXCommandlet.generated.h"
UCLASS()
class UHALBuildPolicePursuitFXCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params) override;
};
