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
	/** Stable for this launch even after the instigator Pawn has left the match. */
	int32 InstigatorPlayerId = INDEX_NONE;
	/** Monotonic per source Actor; pair with its network identity for future GAS deduplication. */
	uint32 ServerHitEventSequence = 0;
	UPrimitiveComponent* TargetPhysicsBody = nullptr;
	FVector ImpactPoint = FVector::ZeroVector;
	FVector ImpactNormal = FVector::ZeroVector;
	FVector NormalImpulse = FVector::ZeroVector;
	FVector SourceVelocity = FVector::ZeroVector;
	FVector TargetVelocityAtImpactPoint = FVector::ZeroVector;
	float SourceMass = 0.0f;
};
