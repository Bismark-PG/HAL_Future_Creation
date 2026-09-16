// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UPrimitiveComponent;

/** Standardized vehicle hit input; the source may be a ball or a future arena hazard. */
struct FVehicleHitContext
{
	AActor* SourceActor = nullptr;
	AActor* TargetActor = nullptr;
	AActor* InstigatorActor = nullptr;
	UPrimitiveComponent* TargetPhysicsBody = nullptr;
	FVector ImpactPoint = FVector::ZeroVector;
	FVector ImpactNormal = FVector::ZeroVector;
	FVector NormalImpulse = FVector::ZeroVector;
	FVector SourceVelocity = FVector::ZeroVector;
	FVector TargetVelocityAtImpactPoint = FVector::ZeroVector;
	float SourceMass = 0.0f;
};
