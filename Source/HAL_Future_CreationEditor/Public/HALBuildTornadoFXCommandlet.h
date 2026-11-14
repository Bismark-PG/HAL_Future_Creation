#pragma once
#include "Commandlets/Commandlet.h"
#include "HALBuildTornadoFXCommandlet.generated.h"
UCLASS()
class UHALBuildTornadoFXCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params) override;
};
