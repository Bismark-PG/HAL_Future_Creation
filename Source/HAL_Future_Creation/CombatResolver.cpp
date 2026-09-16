// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatResolver.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "VehicleHealthComponent.h"

bool FCombatResolver::ResolveVehicleHit(
	const FVehicleHitContext& Hit,
	const float Damage,
	const float AdditionalImpulse)
{
	if (!IsValid(Hit.SourceActor) || !IsValid(Hit.TargetActor)
		|| Hit.SourceActor == Hit.TargetActor || Hit.TargetActor == Hit.InstigatorActor
		|| !Hit.SourceActor->HasAuthority() || !Hit.TargetActor->HasAuthority())
	{
		return false;
	}

	UVehicleHealthComponent* Health = Hit.TargetActor->FindComponentByClass<UVehicleHealthComponent>();
	if (!Health || !Health->ApplyResolvedDamage(Damage))
	{
		return false;
	}

	UPrimitiveComponent* Body = Hit.TargetPhysicsBody;
	if (!IsValid(Body) || Body->GetOwner() != Hit.TargetActor || !Body->IsSimulatingPhysics())
	{
		Body = Cast<UPrimitiveComponent>(Hit.TargetActor->GetRootComponent());
	}

	if (IsValid(Body) && Body->IsSimulatingPhysics() && FMath::IsFinite(AdditionalImpulse)
		&& AdditionalImpulse > 0.0f)
	{
		FVector PushDirection = (-Hit.ImpactNormal).GetSafeNormal();
		if (PushDirection.IsNearlyZero())
		{
			PushDirection = (Hit.TargetActor->GetActorLocation()
				- Hit.SourceActor->GetActorLocation()).GetSafeNormal();
		}
		if (!PushDirection.IsNearlyZero())
		{
			Body->AddImpulseAtLocation(PushDirection * AdditionalImpulse, Hit.ImpactPoint);
		}
	}
	return true;
}
