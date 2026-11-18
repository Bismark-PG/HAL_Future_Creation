#pragma once
#include "Commandlets/Commandlet.h"
#include "HALBuildBattleTopCartoonFXCommandlet.generated.h"

/** Creates new cartoon resources, or explicitly changes only the three skill FX resource references. */
UCLASS()
class UHALBuildBattleTopCartoonFXCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UHALBuildBattleTopCartoonFXCommandlet();
	virtual int32 Main(const FString& Params) override;
};
