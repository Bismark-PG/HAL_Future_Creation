#pragma once
#include "Commandlets/Commandlet.h"
#include "HALBuildVehicleHUDStyleCommandlet.generated.h"

/** Creates HUD art/material and optionally bakes a catalog's selected BP -> skeletal mesh colours. Does not edit BPs. */
UCLASS()
class UHALBuildVehicleHUDStyleCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UHALBuildVehicleHUDStyleCommandlet();
	virtual int32 Main(const FString& Params) override;
};
