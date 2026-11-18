#pragma once
#include "Commandlets/Commandlet.h"
#include "HALBuildDamageFeedbackFXCommandlet.generated.h"

/** Authors only the new shared received-damage recipe/material. Never touches vehicle BP or base materials. */
UCLASS()
class UHALBuildDamageFeedbackFXCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params) override;
};
