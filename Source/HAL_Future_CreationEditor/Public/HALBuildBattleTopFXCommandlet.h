#pragma once
#include "Commandlets/Commandlet.h"
#include "HALBuildBattleTopFXCommandlet.generated.h"

/** Creates only new Niagara/material assets; never edits a Blueprint or an existing skill recipe. */
UCLASS()
class UHALBuildBattleTopFXCommandlet : public UCommandlet
{
	GENERATED_BODY()
  public:
	UHALBuildBattleTopFXCommandlet();
	virtual int32 Main(const FString& Params) override;
};
