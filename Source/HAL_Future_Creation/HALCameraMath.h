#pragma once
#include "CoreMinimal.h"
#include "Camera/CameraTypes.h"

class UHALCameraConfig;
struct FHALCameraTarget
{
	FVector Position = FVector::ZeroVector;
	float Weight = 0, ZoomInfluence = 0;
	FString Label;
};
struct FHALCameraState
{
	FVector AutoFocus = FVector::ZeroVector, PreviousOwn = FVector::ZeroVector;
	FVector2D ManualOffset = FVector2D::ZeroVector;
	float Distance = 0, IdleSeconds = 0;
	bool bSeeded = false;
	uint8 InputDevice = 0; // 0 none, 1 mouse, 2 gamepad
};
struct FHALCameraFrame
{
	FMinimalViewInfo View;
	FVector AutoGoal = FVector::ZeroVector, AutoFocus = FVector::ZeroVector, Focus = FVector::ZeroVector;
	FVector2D OwnScreen = FVector2D(.5,.5), ManualOffset = FVector2D::ZeroVector;
	FVector2D Viewport = FVector2D::ZeroVector, SoftSize = FVector2D(.4,.4), HardSize = FVector2D(.6,.6);
	TArray<FHALCameraTarget> Targets;
	float DesiredDistance = 0, Distance = 0, IdleSeconds = 0;
	uint8 InputDevice = 0;
	bool bHardLimited = false, bManualLimited = false, bReturning = false, bDynamic = false, bValid = false;
	int32 MainBallCandidates = 0;
};
namespace HALCamera
{
	HAL_FUTURE_CREATION_API FMinimalViewInfo View(const UHALCameraConfig& Config, const FVector& Focus, float Distance, float Aspect);
	HAL_FUTURE_CREATION_API bool Project(const FMinimalViewInfo& View, const FVector& Point, FVector2D& Screen);
	HAL_FUTURE_CREATION_API FVector ClampOwn(const UHALCameraConfig& Config, const FVector& Focus, float Distance,
		float Aspect, const FVector& Own, const FVector2D& FrameSize, bool& bLimited);
	HAL_FUTURE_CREATION_API float RequiredDistance(const UHALCameraConfig& Config, const FVector& Focus, const FVector& Point, float Aspect);
	HAL_FUTURE_CREATION_API FHALCameraFrame Advance(const UHALCameraConfig& Config, const FVector& Own,
		const TArray<FHALCameraTarget>& Targets, const FVector2D& Viewport, float DeltaSeconds,
		const FVector2D& MouseDelta, const FVector2D& Stick, bool bAcceptInput, bool bDynamic, FHALCameraState& State);
}
