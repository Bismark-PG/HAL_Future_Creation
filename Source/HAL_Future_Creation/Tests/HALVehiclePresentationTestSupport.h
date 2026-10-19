#pragma once

#if WITH_DEV_AUTOMATION_TESTS
#include "../HALVehiclePresentationComponent.h"
#include "../HALVehiclePresentationConfig.h"
#include "../TestVehiclePawn.h"
#include "Components/StaticMeshComponent.h"

/** Headless data-path probe only: no imported rig or Blueprint/CDO/asset mutation. */
struct FHALPresentationTestAccess
{
	static UHALVehiclePresentationComponent* EnableProbe(ATestVehiclePawn* Pawn)
	{
		auto* Result = Pawn->VehiclePresentation.Get();
		if (Result->bConfigured) { return Result; }
		Result->Body = Pawn->CollisionRoot;
		Result->Fallback = Pawn->VisualMesh;
		Result->Mesh = Pawn->SkeletalVisual;
		Result->Config = NewObject<UHALVehiclePresentationConfig>(Result);
		for (auto& Wheel : Result->Config->Wheels) { Wheel.RadiusCm = 30; }
		Result->bFallbackWasVisible = Pawn->VisualMesh->IsVisible();
		Result->bFallbackWasHiddenInGame = Pawn->VisualMesh->bHiddenInGame;
		Result->bConfigured = true;
		Result->ResetPose();
		Result->SetComponentTickEnabled(true);
		return Result;
	}
	static void SetInput(ATestVehiclePawn* Pawn, float Steering)
	{
		Pawn->CurrentInputCommand.Reset();
		Pawn->CurrentInputCommand.Steering = Steering;
		Pawn->PushInputCommand();
	}
	static UStaticMeshComponent* Body(ATestVehiclePawn* Pawn) { return Pawn->CollisionRoot; }
	static void Reset(UHALVehiclePresentationComponent* Component) { Component->ResetPose(); }
};
#endif
