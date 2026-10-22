#pragma once

#if WITH_DEV_AUTOMATION_TESTS
#include "../HALVehiclePresentationComponent.h"
#include "../HALVehiclePresentationConfig.h"
#include "../TestVehiclePawn.h"
#include "../VehicleDefinition.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../VehicleRecoveryComponent.h"
#include "Misc/ScopeLock.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UnrealType.h"

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
	/** Runtime instance copies only. Actual rig caching stays intact unless a synthetic probe is requested. */
	static UHALVehiclePresentationComponent* EnableSuspensionProbe(ATestVehiclePawn* Pawn, bool bSynthetic = false)
	{
		auto* Result = EnableProbe(Pawn);
		Result->Config = DuplicateObject<UHALVehiclePresentationConfig>(Result->Config, Result);
		Result->Config->Suspension = FHALVisualSuspensionConfig();
		Result->Config->Suspension.bEnabled = true;
		Result->Config->Suspension.ResponseSeconds = 0;
		for (auto& Wheel : Result->Config->Wheels) { Wheel.Suspension = FHALWheelSuspensionTravel(); }
		if (bSynthetic)
		{
			Result->Mesh->SetRelativeTransform(FTransform::Identity);
			const FVector Centers[] = { {100,-100,0}, {100,100,0}, {-100,-100,0}, {-100,100,0} };
			for (int32 Index = 0; Index < 4; ++Index)
			{
				Result->ReferenceWheelCenters[Index] = Centers[Index];
				Result->ReferenceSuspensions[Index] = FTransform(FRotator(0,0,90), Centers[Index]);
			}
			for (auto& Wheel : Result->Config->Wheels) { Wheel.RadiusCm = 30; }
		}
		Result->ResetPose();
		return Result;
	}
	static FVector WheelCenter(UHALVehiclePresentationComponent* Component, EHALVehicleWheel Wheel)
	{ return Component->Mesh->GetComponentTransform().TransformPosition(Component->ReferenceWheelCenters[uint8(Wheel)]); }
	static float WheelRadius(UHALVehiclePresentationComponent* Component, EHALVehicleWheel Wheel)
	{
		for (const auto& Config : Component->Config->Wheels) { if (Config.Wheel == Wheel) { return Config.RadiusCm; } }
		return 0;
	}
	static void SetSuspension(UHALVehiclePresentationComponent* Component, bool bEnabled, float Response = 0)
	{ Component->Config->Suspension.bEnabled = bEnabled; Component->Config->Suspension.ResponseSeconds = Response; }
	static void SetRecoveryExecuting(ATestVehiclePawn* Pawn, bool bExecuting)
	{
		auto* Recovery = Pawn->FindComponentByClass<UVehicleRecoveryComponent>();
		auto* Property = FindFProperty<FStructProperty>(Recovery->GetClass(), TEXT("Status"));
		Property->ContainerPtrToValuePtr<FVehicleRecoveryStatus>(Recovery)->Phase = bExecuting ? EVehicleRecoveryPhase::Executing : EVehicleRecoveryPhase::Idle;
	}
};
#endif
