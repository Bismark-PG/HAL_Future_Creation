// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcadeVehicleMovementComponent.h"

#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "VehicleWallEscapeMath.h"

UArcadeVehicleMovementComponent::UArcadeVehicleMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UArcadeVehicleMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!UpdatedPrimitive)
	{
		UpdatedPrimitive = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
	}

	if (!UpdatedPrimitive)
	{
		UE_LOG(LogTemp, Error, TEXT("%s requires a primitive component to drive."), *GetNameSafe(this));
		SetComponentTickEnabled(false);
		return;
	}

	if (!UpdatedPrimitive->IsSimulatingPhysics())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s will not move because %s is not simulating physics."),
			*GetNameSafe(this),
			*GetNameSafe(UpdatedPrimitive));
	}
}

void UArcadeVehicleMovementComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!UpdatedPrimitive || !UpdatedPrimitive->IsSimulatingPhysics() || DeltaTime <= 0.0f)
	{
		bGrounded = false;
		ForwardSpeed = 0.0f;
		ResetWallEscape();
		return;
	}

	FVector GroundNormal = FVector::UpVector;
	bGrounded = UpdateGroundContact(GroundNormal);

	const FVector Velocity = UpdatedPrimitive->GetPhysicsLinearVelocity();
	const FVector ForwardDirection = FVector::VectorPlaneProject(
		UpdatedPrimitive->GetForwardVector(), GroundNormal).GetSafeNormal();
	const FVector RightDirection = FVector::CrossProduct(GroundNormal, ForwardDirection).GetSafeNormal();

	ForwardSpeed = FVector::DotProduct(Velocity, ForwardDirection);

	if (!bGrounded || ForwardDirection.IsNearlyZero() || RightDirection.IsNearlyZero())
	{
		ResetWallEscape();
		return;
	}

	const float LateralSpeed = FVector::DotProduct(Velocity, RightDirection);
	UpdateWallEscape(GroundNormal, ForwardDirection, Velocity, DeltaTime);

	ApplyLongitudinalForces(ForwardDirection, ForwardSpeed);
	ApplyLateralGrip(RightDirection, LateralSpeed);
	ApplySteering(GroundNormal, ForwardSpeed, DeltaTime);
}

void UArcadeVehicleMovementComponent::SetUpdatedPrimitive(UPrimitiveComponent* InPrimitive)
{
	UpdatedPrimitive = InPrimitive;
	ResetWallEscape();
}

void UArcadeVehicleMovementComponent::SetInputCommand(const FVehicleInputCmd& InInputCommand)
{
	InputCommand = InInputCommand;
	InputCommand.Sanitize();
}

void UArcadeVehicleMovementComponent::ResetInputCommand()
{
	InputCommand.Reset();
	ResetWallEscape();
}

void UArcadeVehicleMovementComponent::ResetWallEscape()
{
	bWallEscapeActive = false;
	WallEscapeBlend = 0.0f;
	WallEscapeNormals.Reset();
}

void UArcadeVehicleMovementComponent::UpdateWallEscape(
	const FVector& GroundNormal, const FVector& ForwardDirection,
	const FVector& Velocity, float DeltaTime)
{
	// This is not landing assistance or an emergency flip system.
	if (!bEnableWallEscape || FVector::DotProduct(UpdatedPrimitive->GetUpVector(), GroundNormal) < 0.5f)
	{
		ResetWallEscape();
		return;
	}

	const float PlanarSpeed = FVector::VectorPlaneProject(Velocity, GroundNormal).Size();
	const bool bEligible = VehicleWallEscape::IsEligible(InputCommand, PlanarSpeed, bWallEscapeActive,
		WallEscapeEnterSpeed, WallEscapeExitSpeed, WallEscapeMinimumSteering);
	TArray<FVector> NewNormals;
	UWorld* World = GetWorld();
	if (bEligible && World)
	{
		const FVector Start = UpdatedPrimitive->GetComponentLocation();
		const FVector End = Start + ForwardDirection * FMath::Clamp(WallEscapeProbeDistance, 1.0f, 100.0f);
		FComponentQueryParams Params(SCENE_QUERY_STAT(VehicleWallEscapeProbe), GetOwner());
		Params.bIgnoreTouches = true;
		Params.bFindInitialOverlaps = true;

		// A supporting floor (or a car) can be the first blocking hit at time zero.
		// Retry ignoring each encountered blocker so it cannot hide the actual wall.
		// Bound query cost, and retain at most two distinct walls for a corner.
		for (int32 Attempt = 0; Attempt < 4 && NewNormals.Num() < 2; ++Attempt)
		{
			TArray<FHitResult> Hits;
			World->ComponentSweepMulti(Hits, UpdatedPrimitive, Start, End,
				UpdatedPrimitive->GetComponentQuat(), Params);
			bool bFoundBlocker = false;
			for (const FHitResult& Hit : Hits)
			{
				UPrimitiveComponent* Other = Hit.GetComponent();
				if (!Hit.bBlockingHit || !Other)
				{
					continue;
				}
				bFoundBlocker = true;
				Params.AddIgnoredComponent(Other);
				FVector Normal = Hit.ImpactNormal.GetSafeNormal();
				if (Normal.IsNearlyZero())
				{
					Normal = Hit.Normal.GetSafeNormal();
				}
				// Static walls only: do not weaken car-to-car pushing or ball physics.
				// Reject support surfaces and steeply upward/downward-facing bevels.
				if (Other->GetCollisionObjectType() != ECC_WorldStatic
					|| Normal.IsNearlyZero() || FMath::Abs(FVector::DotProduct(Normal, GroundNormal)) > 0.35f)
				{
					continue;
				}
				Normal = FVector::VectorPlaneProject(Normal, GroundNormal).GetSafeNormal();
				if (FVector::DotProduct(ForwardDirection, Normal) >= -0.15f)
				{
					continue;
				}
				const bool bDuplicate = NewNormals.ContainsByPredicate([&Normal](const FVector& Existing)
				{
					return FVector::DotProduct(Existing, Normal) > 0.99f;
				});
				if (!bDuplicate && NewNormals.Num() < 2)
				{
					NewNormals.Add(Normal);
				}
			}
			if (!bFoundBlocker)
			{
				break;
			}
		}
		if (bDrawWallEscapeDebug)
		{
			DrawDebugLine(World, Start, End, FColor::Cyan, false, 0.0f, 0, 3.0f);
		}
	}

	bWallEscapeActive = !NewNormals.IsEmpty();
	if (bWallEscapeActive)
	{
		WallEscapeNormals = MoveTemp(NewNormals);
	}
	WallEscapeBlend = VehicleWallEscape::UpdateBlend(WallEscapeBlend, bWallEscapeActive,
		DeltaTime, WallEscapeBlendInTime, WallEscapeBlendOutTime);
	if (WallEscapeBlend <= 0.0f)
	{
		WallEscapeNormals.Reset();
	}
	if (bDrawWallEscapeDebug && World)
	{
		const FVector Start = UpdatedPrimitive->GetComponentLocation();
		for (const FVector& Normal : WallEscapeNormals)
		{
			DrawDebugDirectionalArrow(World, Start, Start + Normal * 100.0f, 15.0f,
				bWallEscapeActive ? FColor::Green : FColor::Orange, false, 0.0f, 0, 3.0f);
		}
	}
}

bool UArcadeVehicleMovementComponent::UpdateGroundContact(FVector& OutGroundNormal) const
{
	UWorld* World = GetWorld();
	if (!World || !UpdatedPrimitive)
	{
		return false;
	}

	const FVector TraceStart = UpdatedPrimitive->GetComponentLocation();
	const float TraceLength = UpdatedPrimitive->Bounds.BoxExtent.Z + GroundTraceExtraDistance;
	const FVector TraceEnd = TraceStart - (FVector::UpVector * TraceLength);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArcadeVehicleGroundTrace), false, GetOwner());
	FHitResult GroundHit;
	const bool bHit = World->LineTraceSingleByChannel(
		GroundHit,
		TraceStart,
		TraceEnd,
		GroundTraceChannel,
		QueryParams);

	if (bDrawGroundDebug)
	{
		const FColor TraceColor = bHit ? FColor::Green : FColor::Red;
		DrawDebugLine(World, TraceStart, TraceEnd, TraceColor, false, 0.0f, 0, 2.0f);
		if (bHit)
		{
			DrawDebugPoint(World, GroundHit.ImpactPoint, 10.0f, FColor::Yellow, false, 0.0f);
		}
	}

	if (!bHit)
	{
		return false;
	}

	OutGroundNormal = GroundHit.ImpactNormal.GetSafeNormal();
	if (OutGroundNormal.IsNearlyZero())
	{
		OutGroundNormal = FVector::UpVector;
	}
	return true;
}

void UArcadeVehicleMovementComponent::ApplyLongitudinalForces(
	const FVector& ForwardDirection,
	float CurrentForwardSpeed)
{
	// Brake wins when both face buttons are held, producing deterministic input.
	const bool bBrakeHeld = InputCommand.Brake > KINDA_SMALL_NUMBER;
	const bool bThrottleHeld = InputCommand.Throttle > KINDA_SMALL_NUMBER && !bBrakeHeld;

	float RequestedAcceleration = 0.0f;
	if (bBrakeHeld)
	{
		if (CurrentForwardSpeed > ReverseEngageSpeed)
		{
			RequestedAcceleration = -BrakeDeceleration * InputCommand.Brake;
		}
		else if (CurrentForwardSpeed > -MaxReverseSpeed)
		{
			RequestedAcceleration = -ReverseAcceleration * InputCommand.Brake;
		}
	}
	else if (bThrottleHeld)
	{
		if (CurrentForwardSpeed < -ReverseEngageSpeed)
		{
			RequestedAcceleration = BrakeDeceleration * InputCommand.Throttle;
		}
		else if (CurrentForwardSpeed < MaxForwardSpeed)
		{
			RequestedAcceleration = ForwardAcceleration * InputCommand.Throttle;
		}
	}

	FVector DriveAcceleration = ForwardDirection * RequestedAcceleration;
	if (bThrottleHeld && CurrentForwardSpeed >= -ReverseEngageSpeed && RequestedAcceleration > 0.0f)
	{
		DriveAcceleration = VehicleWallEscape::ConstrainThrottle(DriveAcceleration,
			WallEscapeNormals, WallEscapeIntoWallThrottleScale, WallEscapeBlend);
	}
	// Do not suppress braking, reversing, coasting drag or speed-limit corrections.
	float ResistanceAcceleration = -CurrentForwardSpeed * CoastingDragRate;

	if (CurrentForwardSpeed > MaxForwardSpeed)
	{
		ResistanceAcceleration -= (CurrentForwardSpeed - MaxForwardSpeed) * OverspeedCorrectionRate;
	}
	else if (CurrentForwardSpeed < -MaxReverseSpeed)
	{
		ResistanceAcceleration -= (CurrentForwardSpeed + MaxReverseSpeed) * OverspeedCorrectionRate;
	}

	UpdatedPrimitive->AddForce(DriveAcceleration + ForwardDirection * ResistanceAcceleration, NAME_None, true);
}

void UArcadeVehicleMovementComponent::ApplyLateralGrip(
	const FVector& RightDirection,
	float CurrentLateralSpeed)
{
	const float GripRate = InputCommand.bHandbrake ? HandbrakeGripRate : LateralGripRate;
	const float GripAcceleration = FMath::Clamp(
		-CurrentLateralSpeed * GripRate,
		-MaxLateralGripAcceleration,
		MaxLateralGripAcceleration);

	UpdatedPrimitive->AddForce(RightDirection * GripAcceleration, NAME_None, true);
}

void UArcadeVehicleMovementComponent::ApplySteering(
	const FVector& GroundNormal,
	float CurrentForwardSpeed,
	float DeltaTime)
{
	const float AbsoluteSpeed = FMath::Abs(CurrentForwardSpeed);
	float SteeringAuthority = FMath::Clamp(AbsoluteSpeed / FMath::Max(FullSteeringSpeed, 1.0f), 0.0f, 1.0f);

	// Apply the floor before high-speed fading so fast drifting keeps its existing behavior.
	if (InputCommand.bHandbrake && FMath::Abs(InputCommand.Steering) > KINDA_SMALL_NUMBER)
	{
		SteeringAuthority = FMath::Max(
			SteeringAuthority,
			FMath::Clamp(HandbrakeMinimumSteeringAuthority, 0.0f, 1.0f));
	}

	if (AbsoluteSpeed > HighSpeedSteeringStart && MaxForwardSpeed > HighSpeedSteeringStart)
	{
		const float HighSpeedAlpha = FMath::Clamp(
			(AbsoluteSpeed - HighSpeedSteeringStart) / (MaxForwardSpeed - HighSpeedSteeringStart),
			0.0f,
			1.0f);
		SteeringAuthority *= FMath::Lerp(1.0f, HighSpeedSteeringScale, HighSpeedAlpha);
	}

	// A small wall rebound should not invert a stationary handbrake pivot.
	const float ReverseSteeringThreshold = InputCommand.bHandbrake
		? -FMath::Max(0.0f, HandbrakeReverseSteeringSpeed)
		: 0.0f;
	const float TravelDirection = CurrentForwardSpeed < ReverseSteeringThreshold ? -1.0f : 1.0f;
	const float SteeringMultiplier = InputCommand.bHandbrake ? HandbrakeSteeringMultiplier : 1.0f;
	const float DampingRate = InputCommand.bHandbrake ? HandbrakeYawDampingRate : YawDampingRate;
	const float CurrentYawRate = FVector::DotProduct(
		UpdatedPrimitive->GetPhysicsAngularVelocityInRadians(),
		GroundNormal);

	const float SteeringAcceleration =
		InputCommand.Steering
		* TravelDirection
		* SteeringAngularAcceleration
		* SteeringAuthority
		* SteeringMultiplier;
	const float DampingAcceleration = -CurrentYawRate * DampingRate;
	const float EscapeAcceleration = VehicleWallEscape::YawAcceleration(
		InputCommand.Steering * FMath::Max(0.0f, WallEscapeTargetYawRate), CurrentYawRate,
		WallEscapeYawResponseRate, WallEscapeMaxYawAcceleration, DeltaTime);
	const float YawAcceleration = FMath::Lerp(
		SteeringAcceleration + DampingAcceleration, EscapeAcceleration, WallEscapeBlend);

	UpdatedPrimitive->AddTorqueInRadians(
		GroundNormal * YawAcceleration,
		NAME_None,
		true);
}
