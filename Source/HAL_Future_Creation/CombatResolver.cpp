// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatResolver.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "VehicleHealthComponent.h"

namespace
{
	FVector GetPushDirection(const FVehicleHitContext& Hit)
	{
		FVector PushDirection = FVector::VectorPlaneProject(-Hit.ImpactNormal, FVector::UpVector).GetSafeNormal();
		if (PushDirection.IsNearlyZero())
		{
			PushDirection = FVector::VectorPlaneProject(
				Hit.TargetActor->GetActorLocation() - Hit.SourceActor->GetActorLocation(),
				FVector::UpVector).GetSafeNormal();
		}
		return PushDirection;
	}

	float CalculateImpactSpeed(
		const FVehicleHitContext& Hit,
		const FVector& PushDirection,
		const float StrengthMultiplier)
	{
		const FVector RelativeVelocity = Hit.SourceVelocity - Hit.TargetVelocityAtImpactPoint;
		const float RelativeNormalSpeed = FMath::Abs(FVector::DotProduct(RelativeVelocity, PushDirection));
		const float ImpulseEquivalentSpeed = Hit.SourceMass > KINDA_SMALL_NUMBER
			? Hit.NormalImpulse.Size() / Hit.SourceMass
			: 0.0f;
		return FMath::Max(RelativeNormalSpeed, ImpulseEquivalentSpeed)
			* FMath::Max(0.0f, StrengthMultiplier);
	}

	bool IsTargetGrounded(
		const UPrimitiveComponent& Body,
		const FVehicleHitContext& Hit,
		const float ExtraDistance)
	{
		const UWorld* World = Body.GetWorld();
		if (!World)
		{
			return false;
		}

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VehicleKnockbackGroundProbe), false);
		QueryParams.AddIgnoredActor(Hit.TargetActor);
		QueryParams.AddIgnoredActor(Hit.SourceActor);
		const FVector Start = Body.Bounds.Origin;
		const float TraceDistance = Body.Bounds.BoxExtent.Z + FMath::Max(0.0f, ExtraDistance);
		return World->LineTraceTestByChannel(
			Start,
			Start - (FVector::UpVector * TraceDistance),
			ECC_Visibility,
			QueryParams);
	}

	void ApplyTierOutcome(
		UPrimitiveComponent& Body,
		const FVector& PushDirection,
		const FVehicleKnockbackTierDefinition& Tier,
		const float MaxAngularSpeed,
		const bool bTargetGrounded)
	{
		const FVector UpDirection = FVector::UpVector;
		const FVector CurrentVelocity = Body.GetPhysicsLinearVelocity();
		const float CurrentVerticalSpeed = FVector::DotProduct(CurrentVelocity, UpDirection);
		float VerticalDeltaSpeed = FMath::Max(
			0.0f,
			FMath::Max(0.0f, Tier.TargetVerticalSpeed) - CurrentVerticalSpeed);
		if (bTargetGrounded && Tier.TargetVerticalSpeed <= KINDA_SMALL_NUMBER)
		{
			VerticalDeltaSpeed = -FMath::Max(0.0f, CurrentVerticalSpeed);
		}
		const FVector VelocityChange =
			(PushDirection * FMath::Max(0.0f, Tier.HorizontalDeltaSpeed))
			+ (UpDirection * VerticalDeltaSpeed);
		if (!VelocityChange.IsNearlyZero())
		{
			Body.AddImpulse(VelocityChange, NAME_None, true);
		}

		const FVector CurrentAngularVelocity = Body.GetPhysicsAngularVelocityInRadians();
		const float PreservedYawSpeed = FVector::DotProduct(CurrentAngularVelocity, UpDirection);
		FVector DesiredAngularVelocity = UpDirection * PreservedYawSpeed;
		bool bApplyAuthoredRotation = bTargetGrounded && Tier.TargetVerticalSpeed <= KINDA_SMALL_NUMBER;

		if (Tier.AirborneFlipTurns > 0.0f && Tier.TargetVerticalSpeed > KINDA_SMALL_NUMBER)
		{
			bApplyAuthoredRotation = true;
			const float ExpectedAirTime = (2.0f * Tier.TargetVerticalSpeed) / 980.0f;
			const float RequestedFlipSpeed =
				(2.0f * PI * Tier.AirborneFlipTurns) / FMath::Max(0.1f, ExpectedAirTime);
			const float FlipSpeed = FMath::Min(FMath::Max(0.0f, MaxAngularSpeed), RequestedFlipSpeed);
			const FVector FlipAxis = FVector::CrossProduct(UpDirection, PushDirection).GetSafeNormal();
			DesiredAngularVelocity += FlipAxis * FlipSpeed;
		}

		if (bApplyAuthoredRotation)
		{
			Body.AddAngularImpulseInRadians(
				DesiredAngularVelocity - CurrentAngularVelocity,
				NAME_None,
				true);
		}
	}
}

FVehicleHitResolution FCombatResolver::ResolveVehicleHit(
	const FVehicleHitContext& Hit,
	const float Damage,
	const float KnockbackStrengthMultiplier)
{
	FVehicleHitResolution Resolution;
	if (!IsValid(Hit.SourceActor) || !IsValid(Hit.TargetActor)
		|| Hit.SourceActor == Hit.TargetActor || Hit.TargetActor == Hit.InstigatorActor
		|| !Hit.SourceActor->HasAuthority() || !Hit.TargetActor->HasAuthority())
	{
		return Resolution;
	}

	UVehicleHealthComponent* Health = Hit.TargetActor->FindComponentByClass<UVehicleHealthComponent>();
	if (!Health || !Health->ApplyResolvedDamage(Damage))
	{
		return Resolution;
	}

	UPrimitiveComponent* Body = Hit.TargetPhysicsBody;
	if (!IsValid(Body) || Body->GetOwner() != Hit.TargetActor || !Body->IsSimulatingPhysics())
	{
		Body = Cast<UPrimitiveComponent>(Hit.TargetActor->GetRootComponent());
	}

	const FVector PushDirection = GetPushDirection(Hit);
	const UVehicleKnockbackSettings* Settings = GetDefault<UVehicleKnockbackSettings>();
	if (IsValid(Body) && Body->IsSimulatingPhysics() && !PushDirection.IsNearlyZero() && Settings)
	{
		Resolution.ImpactSpeed = CalculateImpactSpeed(Hit, PushDirection, KnockbackStrengthMultiplier);
		Resolution.KnockbackTier = Settings->SelectTier(Resolution.ImpactSpeed);
		const bool bTargetGrounded = IsTargetGrounded(
			*Body,
			Hit,
			Settings->GroundProbeExtraDistance);
		ApplyTierOutcome(
			*Body,
			PushDirection,
			Settings->GetTierDefinition(Resolution.KnockbackTier),
			Settings->MaxAirborneAngularSpeed,
			bTargetGrounded);
	}
	Resolution.bResolved = true;
	return Resolution;
}
