#pragma once
#include "Commandlets/Commandlet.h"
#include "HALBuildSideShiftFXCommandlet.generated.h"

/** Creates new native skill/art assets; -Quicken30 only retunes this skill DA, never BP/Definition/effects. */
UCLASS()
class UHALBuildSideShiftFXCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params) override;
};
