// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VehicleInputCmd.h"
#include "VehicleNetPhysicsData.generated.h"

/** One continuous input sample for a Chaos physics frame. Launch remains a separate edge event. */
USTRUCT()
struct HAL_FUTURE_CREATION_API FVehicleNetInputData
{
	GENERATED_BODY()

	UPROPERTY()
	FVehicleInputCmd Cmd;

	UPROPERTY()
	uint32 InputSequence = 0;

	/** Local solver frame until Network Physics maps it to the server timeline in phase 4. */
	UPROPERTY()
	int32 PhysicsFrame = INDEX_NONE;

	void Sanitize()
	{
		Cmd.Sanitize();
		Cmd.bLaunch = false;
	}
};

/** Rigid body and movement state needed to compare/restore a predicted vehicle frame. */
USTRUCT()
struct HAL_FUTURE_CREATION_API FVehicleNetStateData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 PhysicsFrame = INDEX_NONE;

	UPROPERTY()
	FVector Position = FVector::ZeroVector;

	UPROPERTY()
	FQuat Rotation = FQuat::Identity;

	UPROPERTY()
	FVector LinearVelocity = FVector::ZeroVector;

	UPROPERTY()
	FVector AngularVelocityRadians = FVector::ZeroVector;

	/** Grounded and forward speed are recomputed each step, so they are not history. */
	UPROPERTY()
	bool bWallEscapeActive = false;

	UPROPERTY()
	float WallEscapeBlend = 0.0f;

	/** Up to two static wall normals are retained during blend-out. */
	UPROPERTY()
	TArray<FVector> WallEscapeNormals;

	/** A launch accepted between physics frames is applied once on the next step. */
	UPROPERTY()
	FVector PendingRecoilDeltaVelocity = FVector::ZeroVector;
};
