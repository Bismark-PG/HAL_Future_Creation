#pragma once

#include "CoreMinimal.h"

class AActor;
class UPrimitiveComponent;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
struct FVehicleBodyConfig;
struct FVehicleVisualConfig;
struct FVehicleCameraConfig;

/** Initialization/Editor-preview helpers only; never edit the shared asset. */
namespace VehicleConfiguration
{
	bool UsesDefinition(const AActor* Actor);
	bool IsReadyForGameplay(const AActor* Actor);
	void ApplyBody(UPrimitiveComponent& Component, const FVehicleBodyConfig& Config);
	void ApplyVisual(UStaticMeshComponent& Component, const FVehicleVisualConfig& Config);
	void ApplyCamera(USpringArmComponent& Boom, UCameraComponent& Camera, const FVehicleCameraConfig& Config);
}
