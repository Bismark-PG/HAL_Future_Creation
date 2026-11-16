#pragma once
#include "Commandlets/Commandlet.h"
#include "HALBuildSideShiftFXCommandlet.generated.h"

/** Creates new native skill/art assets only; never writes vehicle BP, Definition or existing effects. */
UCLASS()
class UHALBuildSideShiftFXCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params) override;
};
