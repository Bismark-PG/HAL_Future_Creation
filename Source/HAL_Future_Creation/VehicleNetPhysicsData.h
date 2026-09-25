// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Physics/NetworkPhysicsComponent.h"
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

	/** Network requests are rejected rather than silently clamped on the server. */
	bool IsValidContinuousInput() const
	{
		return InputSequence != 0 && !Cmd.bLaunch
			&& FMath::IsFinite(Cmd.Steering) && Cmd.Steering >= -1.0f && Cmd.Steering <= 1.0f
			&& FMath::IsFinite(Cmd.Throttle) && Cmd.Throttle >= 0.0f && Cmd.Throttle <= 1.0f
			&& FMath::IsFinite(Cmd.Brake) && Cmd.Brake >= 0.0f && Cmd.Brake <= 1.0f;
	}

	/** Half-range comparison remains ordered when a uint32 sequence wraps. */
	static bool IsNewerSequence(uint32 Candidate, uint32 Previous)
	{
		return Candidate != 0 && Candidate != Previous && uint32(Candidate - Previous) < 0x80000000u;
	}
};

/** One launch press, never consumed as a held continuous input. Zero ball sequence means unavailable until ball-state replication. */
USTRUCT()
struct HAL_FUTURE_CREATION_API FVehicleLaunchRequest
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 LaunchSequence = 0;

	UPROPERTY()
	int32 ClientPhysicsFrame = INDEX_NONE;

	UPROPERTY()
	uint32 ExpectedBallStateSequence = 0;
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

/** UE 5.8 Network Physics frame history. Continuous controls only; launch keeps its idempotent RPC. */
USTRUCT()
struct HAL_FUTURE_CREATION_API FVehiclePhysicsHistoryInput : public FNetworkPhysicsData
{
	GENERATED_BODY()

	UPROPERTY()
	FVehicleNetInputData Input;

	virtual void ApplyData(UActorComponent* NetworkComponent) const override;
	virtual void BuildData(const UActorComponent* NetworkComponent) override;
	virtual void InterpolateData(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData) override;
	virtual void MergeData(const FNetworkPhysicsData& FromData) override;
	virtual void DecayData(float DecayAmount) override;
	virtual void ValidateData(const UActorComponent* NetworkComponent) override;
	virtual bool CompareData(const FNetworkPhysicsData& PredictedData) override;
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FVehiclePhysicsHistoryInput> : public TStructOpsTypeTraitsBase2<FVehiclePhysicsHistoryInput>
{
	enum { WithNetSerializer = true };
};

/** Chaos rewinds the rigid body itself; this history also restores movement's wall-escape memory. */
USTRUCT()
struct HAL_FUTURE_CREATION_API FVehiclePhysicsHistoryState : public FNetworkPhysicsData
{
	GENERATED_BODY()

	UPROPERTY()
	FVehicleNetStateData State;

	virtual void ApplyData(UActorComponent* NetworkComponent) const override;
	virtual void BuildData(const UActorComponent* NetworkComponent) override;
	virtual void InterpolateData(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData) override;
	virtual bool CompareData(const FNetworkPhysicsData& PredictedData) override;
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FVehiclePhysicsHistoryState> : public TStructOpsTypeTraitsBase2<FVehiclePhysicsHistoryState>
{
	enum { WithNetSerializer = true };
};

struct FVehiclePhysicsHistoryTraits
{
	using InputsType = FVehiclePhysicsHistoryInput;
	using StatesType = FVehiclePhysicsHistoryState;
};

namespace VehicleNetworkPhysics
{
	/** UE records local inputs on normal steps but only applies history on remote/server or replay steps. */
	inline bool ShouldUseLocalPendingInput(bool bUsingHistory, bool bOwnsLocalInput, bool bIsResimming)
	{
		return !bUsingHistory || (bOwnsLocalInput && !bIsResimming);
	}
}
