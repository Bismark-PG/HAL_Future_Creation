#pragma once

#if WITH_DEV_AUTOMATION_TESTS
#include "../HALVehiclePresentationComponent.h"
#include "../HALVehiclePresentationConfig.h"
#include "../TestVehiclePawn.h"
#include "../VehicleDefinition.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "Misc/ScopeLock.h"
#include "Components/StaticMeshComponent.h"

/** Headless data-path probe only: no imported rig or Blueprint/CDO/asset mutation. */
struct FHALPresentationTestAccess
{
	/** Isolate the fallback fixture from whichever rig the team has saved on its player. */
	static bool SelectNoRigFixture(ATestVehiclePawn* Pawn)
	{
		if (!Pawn || !Pawn->Definition) { return false; }
		auto* Fixture = DuplicateObject<UVehicleDefinition>(Pawn->Definition, Pawn);
		Fixture->Presentation = nullptr;
		Pawn->Definition = Fixture;
		return true;
	}
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
	static UHALVehiclePresentationComponent* EnableChassisProbe(ATestVehiclePawn* Pawn)
	{
		auto* Result = EnableProbe(Pawn);
		Result->Config = DuplicateObject<UHALVehiclePresentationConfig>(Result->Config, Result);
		Result->Config->Chassis = FHALChassisPresentationConfig();
		Result->Config->Chassis.bEnabled = true;
		Result->ResetPose();
		return Result;
	}
	static void PublishSupport(ATestVehiclePawn* Pawn, bool bDriving, bool bRecovering = false)
	{
		auto* Movement = Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>();
		FScopeLock Lock(&Movement->PendingPhysicsDataLock);
		Movement->bShouldRunPhysics.Store(true);
		Movement->PublishedStability.bValid = true;
		Movement->PublishedStability.bNearGround = bDriving;
		Movement->PublishedStability.bDrivingAllowed = bDriving;
		Movement->PublishedStability.Recovery.bActive = bRecovering;
	}
	static void SetInput(ATestVehiclePawn* Pawn, float Steering, float Throttle = 0)
	{
		Pawn->CurrentInputCommand.Reset();
		Pawn->CurrentInputCommand.Steering = Steering;
		Pawn->CurrentInputCommand.Throttle = Throttle;
		Pawn->PushInputCommand();
	}
	static UStaticMeshComponent* Body(ATestVehiclePawn* Pawn) { return Pawn->CollisionRoot; }
	static void Reset(UHALVehiclePresentationComponent* Component) { Component->ResetPose(); }
	static void DisableChassis(UHALVehiclePresentationComponent* Component) { Component->Config->Chassis.bEnabled = false; }
};
#endif
