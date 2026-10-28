#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "../HALDynamicCameraComponent.h"
#include "../HALCameraConfig.h"
#include "../TestVehiclePawn.h"

struct FHALCameraTestAccess
{
	static bool Compute(UHALDynamicCameraComponent* C, float Dt, FMinimalViewInfo& View, bool bAccept = true,
		FVector2D Size = FVector2D(960,540)) { return C->Compute(Dt,Size,bAccept,View); }
	static UHALCameraConfig* Enable(ATestVehiclePawn* Pawn, float FOV = 90, float Distance = 1200)
	{
		auto* Component = Pawn->FindComponentByClass<UHALDynamicCameraComponent>();
		auto* Local = NewObject<UVehicleLocalConfig>(Component);
		auto* Config = NewObject<UHALCameraConfig>(Local);
		Config->bReadyForUse = true; Config->BaseCamera.FieldOfView = FOV; Config->BaseCamera.TargetArmLength = Distance;
		Config->BaseCamera.BoomTransform = FTransform(FRotator(-75,0,0));
		Local->CameraProfile = Config; Component->Configure(Local); return Config;
	}
};
#endif
