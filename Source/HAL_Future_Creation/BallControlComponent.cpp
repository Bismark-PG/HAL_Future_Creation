// Copyright Epic Games, Inc. All Rights Reserved.

#include "BallControlComponent.h"
#include "VehicleConfigurationTypes.h"
#include "VehicleConfigurationApplication.h"
#include "UObject/UnrealType.h"

#include "BasicBallActor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "TimerManager.h"

UBallControlComponent::UBallControlComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBallControlComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!VehicleBody || !ControlPoint || !BallConstraint)
	{
		UE_LOG(LogTemp, Error, TEXT("%s is missing required vehicle components."), *GetNameSafe(this));
		return;
	}

	// An unconfigured physics mesh must not acquire balls from a stationary, invalid vehicle.
	if (!VehicleBody->IsSimulatingPhysics())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s requires a simulating vehicle body; ball acquisition is disabled."), *GetNameSafe(this));
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(
			AcquisitionTimerHandle,
			this,
			&ThisClass::EvaluateBallControl,
			FMath::Max(0.01f, AcquisitionCheckInterval),
			true);
	}
}

void UBallControlComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AcquisitionTimerHandle);
	}

	if (GetOwner() && GetOwner()->HasAuthority() && IsValid(HeldBall))
	{
		ReleaseHeldBall(FVector::ZeroVector, false);
	}
	else
	{
		ReleaseConstraint();
		HeldBall = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UBallControlComponent::SetVehicleComponents(
	UPrimitiveComponent* InVehicleBody,
	USceneComponent* InControlPoint,
	UPhysicsConstraintComponent* InBallConstraint)
{
	VehicleBody = InVehicleBody;
	ControlPoint = InControlPoint;
	BallConstraint = InBallConstraint;
}

bool UBallControlComponent::LaunchHeldBall()
{
	AActor* Owner = GetOwner();
	if (!Owner
		|| !Owner->HasAuthority()
		|| !IsValid(HeldBall)
		|| !VehicleBody
		|| HeldBall->GetBallState() != EBasicBallState::Controlled
		|| HeldBall->GetControlledBy() != Owner)
	{
		return false;
	}

	ABasicBallActor* BallToLaunch = HeldBall;
	const FVector LaunchDirection = VehicleBody->GetForwardVector().GetSafeNormal();
	const FVector InitialVelocity =
		VehicleBody->GetPhysicsLinearVelocity() + (LaunchDirection * LaunchSpeedIncrement);

	ReleaseConstraint();
	if (!BallToLaunch->LaunchFromControl(Owner, Owner, InitialVelocity))
	{
		ConfigureConstraint(BallToLaunch);
		return false;
	}

	HeldBall = nullptr;

	const float CurrentForwardSpeed = FVector::DotProduct(
		VehicleBody->GetPhysicsLinearVelocity(),
		LaunchDirection);
	const float MinimumRecoil = FMath::Max(0.0f, BaseRecoilDeltaSpeed);
	const float MaximumRecoil = FMath::Max(MinimumRecoil, MaxRecoilDeltaSpeed);
	const float SpeedScaledRecoil =
		FMath::Max(CurrentForwardSpeed, 0.0f) * FMath::Max(0.0f, MovingRecoilFraction);
	const float RecoilDeltaSpeed = FMath::Clamp(
		FMath::Max(MinimumRecoil, SpeedScaledRecoil),
		MinimumRecoil,
		MaximumRecoil);

	// Velocity change keeps recoil consistent if later vehicles use different masses.
	VehicleBody->AddImpulse(-LaunchDirection * RecoilDeltaSpeed, NAME_None, true);
	return true;
}

void UBallControlComponent::HandleVehicleCollision(const FVector& NormalImpulse, const FHitResult& Hit)
{
	if (!GetOwner()->HasAuthority()
		|| !IsValid(HeldBall)
		|| NormalImpulse.SizeSquared() < FMath::Square(DropCollisionImpulseThreshold))
	{
		return;
	}

	FVector ReleaseDirection = NormalImpulse.GetSafeNormal();
	if (ReleaseDirection.IsNearlyZero())
	{
		ReleaseDirection = Hit.ImpactNormal.GetSafeNormal();
	}
	ReleaseHeldBall(ReleaseDirection, true);
}

void UBallControlComponent::EvaluateBallControl()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	if (!IsValid(HeldBall) && bConstraintActive)
	{
		ReleaseConstraint();
	}

	if (IsValid(HeldBall))
	{
		const bool bControlStateValid =
			HeldBall->GetBallState() == EBasicBallState::Controlled
			&& HeldBall->GetControlledBy() == GetOwner();
		if (!bControlStateValid)
		{
			ReleaseConstraint();
			HeldBall = nullptr;
		}
		else if (FVector::DistSquared(HeldBall->GetActorLocation(), ControlPoint->GetComponentLocation())
			> FMath::Square(MaxControlledDistance))
		{
			ReleaseHeldBall(FVector::ZeroVector, false);
		}
	}

	ABasicBallActor* BestCandidate = nullptr;
	if (!IsValid(HeldBall))
	{
		BestCandidate = FindBestCandidate();
		if (BestCandidate)
		{
			AcquireBall(BestCandidate);
		}
	}

	if (bDrawAcquisitionDebug)
	{
		DrawAcquisitionDebug(BestCandidate);
	}
}

ABasicBallActor* UBallControlComponent::FindBestCandidate() const
{
	UWorld* World = GetWorld();
	if (!World || !VehicleBody || !ControlPoint)
	{
		return nullptr;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BallAcquisitionOverlap), false, GetOwner());

	World->OverlapMultiByObjectType(
		Overlaps,
		VehicleBody->GetComponentLocation(),
		FQuat::Identity,
		ObjectQuery,
		FCollisionShape::MakeSphere(AcquisitionRadius),
		QueryParams);

	const FVector VehicleLocation = VehicleBody->GetComponentLocation();
	const FVector VehicleForward = VehicleBody->GetForwardVector().GetSafeNormal();
	const FVector ControlLocation = ControlPoint->GetComponentLocation();

	ABasicBallActor* BestCandidate = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	float BestAlignment = -1.0f;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		ABasicBallActor* Candidate = Cast<ABasicBallActor>(Overlap.GetActor());
		if (!Candidate || !Candidate->CanBeControlledBy(GetOwner()))
		{
			continue;
		}

		const FVector ToCandidate = Candidate->GetActorLocation() - VehicleLocation;
		const FVector CandidateDirection = ToCandidate.GetSafeNormal();
		const float Alignment = FVector::DotProduct(VehicleForward, CandidateDirection);
		if (Alignment < 0.0f || !HasLineOfSightTo(Candidate))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(Candidate->GetActorLocation(), ControlLocation);
		const bool bIsCloser = DistanceSquared < BestDistanceSquared - 1.0f;
		const bool bSameDistanceBetterAngle =
			FMath::IsNearlyEqual(DistanceSquared, BestDistanceSquared, 1.0f)
			&& Alignment > BestAlignment + KINDA_SMALL_NUMBER;
		const bool bSameScoreStableName =
			FMath::IsNearlyEqual(DistanceSquared, BestDistanceSquared, 1.0f)
			&& FMath::IsNearlyEqual(Alignment, BestAlignment)
			&& (!BestCandidate || Candidate->GetPathName() < BestCandidate->GetPathName());

		if (bIsCloser || bSameDistanceBetterAngle || bSameScoreStableName)
		{
			BestCandidate = Candidate;
			BestDistanceSquared = DistanceSquared;
			BestAlignment = Alignment;
		}
	}

	return BestCandidate;
}

bool UBallControlComponent::HasLineOfSightTo(const ABasicBallActor* Candidate) const
{
	if (!Candidate || !ControlPoint)
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BallAcquisitionLineOfSight), false, GetOwner());
	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		ControlPoint->GetComponentLocation(),
		Candidate->GetActorLocation(),
		LineOfSightTraceChannel,
		QueryParams);

	return !bHit || Hit.GetActor() == Candidate;
}

bool UBallControlComponent::AcquireBall(ABasicBallActor* Candidate)
{
	if (!Candidate || !Candidate->BeginControl(GetOwner()))
	{
		return false;
	}

	HeldBall = Candidate;
	ConfigureConstraint(Candidate);
	if (!bConstraintActive)
	{
		Candidate->ReleaseFromControl(GetOwner(), DropReacquireLockDuration);
		HeldBall = nullptr;
		return false;
	}
	return true;
}

void UBallControlComponent::ConfigureConstraint(ABasicBallActor* Ball)
{
	if (!BallConstraint || !VehicleBody || !ControlPoint || !Ball || !Ball->GetPhysicsRoot())
	{
		bConstraintActive = false;
		return;
	}

	BallConstraint->SetWorldLocation(ControlPoint->GetComponentLocation());
	BallConstraint->SetWorldRotation(VehicleBody->GetComponentRotation());
	BallConstraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Free, 0.0f);
	BallConstraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Free, 0.0f);
	BallConstraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Free, 0.0f);
	BallConstraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Free, 0.0f);
	BallConstraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Free, 0.0f);
	BallConstraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Free, 0.0f);
	BallConstraint->SetDisableCollision(true);
	BallConstraint->SetLinearPositionDrive(true, true, true);
	BallConstraint->SetLinearVelocityDrive(true, true, true);
	BallConstraint->SetLinearDriveAccelerationMode(true);
	BallConstraint->SetLinearDriveParams(
		ControlPositionStrength,
		ControlVelocityStrength,
		ControlMaxForce);
	BallConstraint->SetLinearPositionTarget(FVector::ZeroVector);
	BallConstraint->SetLinearVelocityTarget(FVector::ZeroVector);
	BallConstraint->SetConstrainedComponents(
		VehicleBody,
		NAME_None,
		Ball->GetPhysicsRoot(),
		NAME_None);

	const FVector VehicleLocalControlPoint =
		VehicleBody->GetComponentTransform().InverseTransformPosition(ControlPoint->GetComponentLocation());
	BallConstraint->SetConstraintReferencePosition(EConstraintFrame::Frame1, VehicleLocalControlPoint);
	BallConstraint->SetConstraintReferencePosition(EConstraintFrame::Frame2, FVector::ZeroVector);
	Ball->GetPhysicsRoot()->WakeAllRigidBodies();
	bConstraintActive = true;
}

void UBallControlComponent::ReleaseConstraint()
{
	if (!BallConstraint || !bConstraintActive)
	{
		return;
	}

	BallConstraint->SetDisableCollision(false);
	BallConstraint->BreakConstraint();
	bConstraintActive = false;
}

void UBallControlComponent::ReleaseHeldBall(
	const FVector& ReleaseDirection,
	const bool bApplyReleaseImpulse)
{
	ABasicBallActor* BallToRelease = HeldBall;
	if (!IsValid(BallToRelease))
	{
		ReleaseConstraint();
		HeldBall = nullptr;
		return;
	}

	ReleaseConstraint();
	if (BallToRelease->ReleaseFromControl(GetOwner(), DropReacquireLockDuration)
		&& bApplyReleaseImpulse)
	{
		const FVector HorizontalDirection =
			FVector::VectorPlaneProject(ReleaseDirection, FVector::UpVector).GetSafeNormal();
		BallToRelease->AddReleaseImpulse(
			(HorizontalDirection * DropBallImpulse) + (FVector::UpVector * DropBallUpwardImpulse));
	}
	HeldBall = nullptr;
}

void UBallControlComponent::DrawAcquisitionDebug(const ABasicBallActor* BestCandidate) const
{
	UWorld* World = GetWorld();
	if (!World || !VehicleBody)
	{
		return;
	}

	const FVector Origin = VehicleBody->GetComponentLocation();
	const FVector ForwardEnd = Origin + (VehicleBody->GetForwardVector() * AcquisitionRadius);
	DrawDebugSphere(World, Origin, AcquisitionRadius, 32, FColor::Cyan, false, AcquisitionCheckInterval, 0, 1.0f);
	DrawDebugLine(World, Origin, ForwardEnd, FColor::Blue, false, AcquisitionCheckInterval, 0, 3.0f);

	if (BestCandidate)
	{
		DrawDebugLine(
			World,
			Origin,
			BestCandidate->GetActorLocation(),
			FColor::Green,
			false,
			AcquisitionCheckInterval,
			0,
			4.0f);
	}
}

bool UBallControlComponent::ApplyConfiguration(const FBallControlConfig& Config)
{
	FString Error;
	if (HasBegunPlay() || !Config.Validate(Error)) { return false; }
	AcquisitionRadius = Config.Acquisition.AcquisitionRadius;
	AcquisitionCheckInterval = Config.Acquisition.AcquisitionCheckInterval;
	LineOfSightTraceChannel = Config.Acquisition.LineOfSightTraceChannel;
	MaxControlledDistance = Config.Control.MaxControlledDistance;
	ControlPositionStrength = Config.Control.ControlPositionStrength;
	ControlVelocityStrength = Config.Control.ControlVelocityStrength;
	ControlMaxForce = Config.Control.ControlMaxForce;
	LaunchSpeedIncrement = Config.Launch.LaunchSpeedIncrement;
	BaseRecoilDeltaSpeed = Config.Launch.BaseRecoilDeltaSpeed;
	MovingRecoilFraction = Config.Launch.MovingRecoilFraction;
	MaxRecoilDeltaSpeed = Config.Launch.MaxRecoilDeltaSpeed;
	DropCollisionImpulseThreshold = Config.ForcedRelease.DropCollisionImpulseThreshold;
	DropReacquireLockDuration = Config.ForcedRelease.DropReacquireLockDuration;
	DropBallImpulse = Config.ForcedRelease.DropBallImpulse;
	DropBallUpwardImpulse = Config.ForcedRelease.DropBallUpwardImpulse;
	bDrawAcquisitionDebug = Config.Debug.bDrawAcquisitionDebug;
	return true;
}

#if WITH_EDITOR
bool UBallControlComponent::CanEditChange(const FProperty* Property) const
{
	if (Property && Property->GetOwnerClass() == StaticClass() && VehicleConfiguration::UsesDefinition(GetOwner())) { return false; }
	return Super::CanEditChange(Property);
}
#endif
