#pragma once

#if WITH_DEV_AUTOMATION_TESTS
#include "../HALVehiclePresentationComponent.h"
#include "../HALVehiclePresentationConfig.h"
#include "../TestVehiclePawn.h"
#include "../VehicleDefinition.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../VehicleRecoveryComponent.h"
#include "../BallControlComponent.h"
#include "../BasicBallActor.h"
#include "InputActionValue.h"
#include "Misc/ScopeLock.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UnrealType.h"

/** Headless data-path probe only: no imported rig or Blueprint/CDO/asset mutation. */
struct FHALPresentationTestAccess
{
	/** Isolate visual-path fixtures from the team's saved optional static fallback. */
	static bool SelectVisualFixture(ATestVehiclePawn* Pawn, bool bWithoutRig, bool bWithoutStatic, bool bInvalidStaticTransform = false)
	{
		if (!Pawn || !Pawn->Definition) { return false; }
		auto* Fixture = DuplicateObject<UVehicleDefinition>(Pawn->Definition, Pawn);
		if (bWithoutRig) { Fixture->Presentation = nullptr; }
		Fixture->Visual = {};
		if (!bWithoutStatic) { Fixture->Visual.Mesh = Fixture->CollisionMesh; }
		if (bInvalidStaticTransform) { Fixture->Visual.RelativeTransform.SetScale3D(FVector::ZeroVector); }
		Pawn->Definition = Fixture;
		return true;
	}
	static bool SelectNoRigFixture(ATestVehiclePawn* Pawn) { return SelectVisualFixture(Pawn, true, false); }
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
	static void SetInput(ATestVehiclePawn* Pawn, float Steering, float Throttle = 0, bool bHandbrake = false)
	{
		Pawn->CurrentInputCommand.Reset();
		Pawn->CurrentInputCommand.Steering = Steering;
		Pawn->CurrentInputCommand.Throttle = Throttle;
		Pawn->CurrentInputCommand.bHandbrake = bHandbrake;
		Pawn->PushInputCommand();
	}
	static UStaticMeshComponent* Body(ATestVehiclePawn* Pawn) { return Pawn->CollisionRoot; }
	static UHALVehiclePresentationComponent* EnableTireSkidProbe(ATestVehiclePawn* Pawn, UNiagaraSystem* System)
	{
		auto* Result = EnableSuspensionProbe(Pawn, true);
		Result->Config->Suspension.bEnabled = false; // Skids must work independently of visual suspension.
		Result->Config->TireSkid.bEnabled = true;
		Result->Config->TireSkid.System = System;
		FString Error;
		Result->bTireSkidReady = Result->Config->TireSkid.Validate(Error);
		return Result;
	}
	static void PublishTireSkidInput(ATestVehiclePawn* Pawn, uint8 Inputs)
	{
		Pawn->CurrentInputCommand.Reset();
		Pawn->CurrentInputCommand.bHandbrake = (Inputs & 1) != 0;
		Pawn->CurrentInputCommand.Brake = (Inputs & 2) ? 1 : 0;
		Pawn->CurrentInputCommand.Throttle = (Inputs & 4) ? 1 : 0;
		Pawn->PushInputCommand();
		auto* Movement = Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>();
		FScopeLock Lock(&Movement->PendingPhysicsDataLock);
		Movement->PublishedPresentationInput.bValid = true;
		Movement->PublishedPresentationInput.TireSkidInputs = Inputs;
	}
	static UNiagaraComponent* TireSkid(UHALVehiclePresentationComponent* Component, int32 Slot) { return Component->TireSkidComponents[Slot]; }
	static int32 FadingTireSkids(UHALVehiclePresentationComponent* Component) { return Component->FadingTireSkids.Num(); }
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
	static UHALVehiclePresentationComponent* EnableLaunchProbe(ATestVehiclePawn* Pawn)
	{
		auto* Result = EnableProbe(Pawn);
		Result->Config = DuplicateObject<UHALVehiclePresentationConfig>(Result->Config, Result);
		Result->Config->LaunchRebound = FHALLaunchReboundConfig(); Result->Config->LaunchRebound.bEnabled = true;
		Result->Config->Chassis = FHALChassisPresentationConfig();
		Result->ResetPose();
		return Result;
	}
	static const FHALLaunchVisualState& LaunchState(UHALVehiclePresentationComponent* Component) { return Component->LaunchState; }
	static UHALVehiclePresentationComponent* EnableImpactProbe(ATestVehiclePawn* Pawn)
	{
		auto* Result = EnableProbe(Pawn);
		Result->Config = DuplicateObject<UHALVehiclePresentationConfig>(Result->Config, Result);
		Result->Config->ImpactRebound = FHALImpactReboundConfig(); Result->Config->ImpactRebound.bEnabled = true;
		Result->Config->Chassis = FHALChassisPresentationConfig();
		Result->Config->LaunchRebound = FHALLaunchReboundConfig();
		Result->ImpactState.StartedCount = 0; // Fixture starts after isolated poses; retain duplicate watermark.
		Result->ResetPose();
		return Result;
	}
	static const FHALImpactVisualState& ImpactState(UHALVehiclePresentationComponent* Component) { return Component->ImpactState; }
	static void ReceiveImpact(UHALVehiclePresentationComponent* Component, const FHALImpactVisualEvent& Event) { Component->ConsumeImpactRebound(Event); }
	static void StartLaunch(ATestVehiclePawn* Pawn) { Pawn->OnLaunchStarted(FInputActionValue(true)); }
	static bool GiveBall(ATestVehiclePawn* Pawn, ABasicBallActor* Ball)
	{
		if (!Ball || !Ball->BeginControl(Pawn)) { return false; }
		// Capture fixture only: actual request/launch/RPC/recoil paths below remain production code.
		auto* Control = Pawn->FindComponentByClass<UBallControlComponent>();
		FindFProperty<FObjectProperty>(Control->GetClass(), TEXT("HeldBall"))->SetObjectPropertyValue_InContainer(Control, Ball);
		return true;
	}
	static FBallRepState SetStaleControlledView(ABasicBallActor* Ball, ATestVehiclePawn* Pawn)
	{
		auto* View = FindFProperty<FStructProperty>(Ball->GetClass(), TEXT("RepState"))->ContainerPtrToValuePtr<FBallRepState>(Ball);
		const FBallRepState Previous = *View; View->State = EBasicBallState::Controlled; View->Holder = Pawn;
		return Previous; // Client fixture: emulate a stale controlled snapshot, never an authority setter.
	}
	static void RestoreBallView(ABasicBallActor* Ball, const FBallRepState& View)
	{ *FindFProperty<FStructProperty>(Ball->GetClass(), TEXT("RepState"))->ContainerPtrToValuePtr<FBallRepState>(Ball) = View; }
};
#endif
