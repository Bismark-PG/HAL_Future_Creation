#pragma once
#include "Commandlets/Commandlet.h"
#include "HALBuildSmokeScreenFXCommandlet.generated.h"

/** Creates new Car08 smoke resources through UE APIs; never edits Blueprint or existing assets. */
UCLASS()
class UHALBuildSmokeScreenFXCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UHALBuildSmokeScreenFXCommandlet() { IsClient=true; IsServer=false; IsEditor=true; LogToConsole=true; }
	virtual int32 Main(const FString& Params) override;
};
