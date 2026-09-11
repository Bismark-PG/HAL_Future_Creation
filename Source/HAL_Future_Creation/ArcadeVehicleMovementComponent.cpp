// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcadeVehicleMovementComponent.h"

#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

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
		return;
	}

	const float LateralSpeed = FVector::DotProduct(Velocity, RightDirection);

	ApplyLongitudinalForces(ForwardDirection, ForwardSpeed);
	ApplyLateralGrip(RightDirection, LateralSpeed);
	ApplySteering(GroundNormal, ForwardSpeed);
}

void UArcadeVehicleMovementComponent::SetUpdatedPrimitive(UPrimitiveComponent* InPrimitive)
{
	UpdatedPrimitive = InPrimitive;
}

void UArcadeVehicleMovementComponent::SetInputCommand(const FVehicleInputCmd& InInputCommand)
{
	InputCommand = InInputCommand;
	InputCommand.Sanitize();
}

void UArcadeVehicleMovementComponent::ResetInputCommand()
{
	InputCommand.Reset();
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

	RequestedAcceleration -= CurrentForwardSpeed * CoastingDragRate;

	if (CurrentForwardSpeed > MaxForwardSpeed)
	{
		RequestedAcceleration -= (CurrentForwardSpeed - MaxForwardSpeed) * OverspeedCorrectionRate;
	}
	else if (CurrentForwardSpeed < -MaxReverseSpeed)
	{
		RequestedAcceleration -= (CurrentForwardSpeed + MaxReverseSpeed) * OverspeedCorrectionRate;
	}

	UpdatedPrimitive->AddForce(ForwardDirection * RequestedAcceleration, NAME_None, true);
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
	float CurrentForwardSpeed)
{
	const float AbsoluteSpeed = FMath::Abs(CurrentForwardSpeed);
	float SteeringAuthority = FMath::Clamp(AbsoluteSpeed / FMath::Max(FullSteeringSpeed, 1.0f), 0.0f, 1.0f);

	if (AbsoluteSpeed > HighSpeedSteeringStart && MaxForwardSpeed > HighSpeedSteeringStart)
	{
		const float HighSpeedAlpha = FMath::Clamp(
			(AbsoluteSpeed - HighSpeedSteeringStart) / (MaxForwardSpeed - HighSpeedSteeringStart),
			0.0f,
			1.0f);
		SteeringAuthority *= FMath::Lerp(1.0f, HighSpeedSteeringScale, HighSpeedAlpha);
	}

	const float TravelDirection = CurrentForwardSpeed < 0.0f ? -1.0f : 1.0f;
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

	UpdatedPrimitive->AddTorqueInRadians(
		GroundNormal * (SteeringAcceleration + DampingAcceleration),
		NAME_None,
		true);
}
