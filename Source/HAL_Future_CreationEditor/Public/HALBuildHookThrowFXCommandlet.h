#pragma once
#include "Commandlets/Commandlet.h"
#include "HALBuildHookThrowFXCommandlet.generated.h"

/** Creates only the four new hook assets. Never edits a Blueprint, map or existing vehicle DA. */
UCLASS()
class UHALBuildHookThrowFXCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UHALBuildHookThrowFXCommandlet();
	virtual int32 Main(const FString& Params) override;
};
