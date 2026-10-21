// Copyright Epic Games, Inc. All Rights Reserved.

#include "VehicleNetPhysicsData.h"
#include "ArcadeVehicleMovementComponent.h"

#include "Serialization/Archive.h"

void FVehiclePhysicsHistoryInput::ApplyData(UActorComponent* NetworkComponent) const
{
	if (UArcadeVehicleMovementComponent* Movement = Cast<UArcadeVehicleMovementComponent>(NetworkComponent))
	{
		Movement->ApplyHistoryInput(Input, LocalFrame);
	}
}

void FVehiclePhysicsHistoryInput::BuildData(const UActorComponent* NetworkComponent)
{
	if (const UArcadeVehicleMovementComponent* Movement = Cast<UArcadeVehicleMovementComponent>(NetworkComponent))
	{
		Movement->BuildHistoryInput(Input, LocalFrame);
	}
}

void FVehiclePhysicsHistoryInput::InterpolateData(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData)
{
	const auto& Min = static_cast<const FVehiclePhysicsHistoryInput&>(MinData);
	const auto& Max = static_cast<const FVehiclePhysicsHistoryInput&>(MaxData);
	const float Alpha = Max.LocalFrame != Min.LocalFrame
		? FMath::Clamp(float(LocalFrame - Min.LocalFrame) / float(Max.LocalFrame - Min.LocalFrame), 0.0f, 1.0f) : 1.0f;
	Input = Max.Input;
	Input.Cmd.Throttle = FMath::Lerp(Min.Input.Cmd.Throttle, Max.Input.Cmd.Throttle, Alpha);
	Input.Cmd.Brake = FMath::Lerp(Min.Input.Cmd.Brake, Max.Input.Cmd.Brake, Alpha);
	Input.Cmd.Steering = FMath::Lerp(Min.Input.Cmd.Steering, Max.Input.Cmd.Steering, Alpha);
	Input.Cmd.bHandbrake = Alpha < 0.5f ? Min.Input.Cmd.bHandbrake : Max.Input.Cmd.bHandbrake;
	Input.Cmd.bSelfRight = Alpha < 1.f ? Min.Input.Cmd.bSelfRight : Max.Input.Cmd.bSelfRight;
	Input.PhysicsFrame = LocalFrame;
}

void FVehiclePhysicsHistoryInput::MergeData(const FNetworkPhysicsData& FromData)
{
	const auto& Older = static_cast<const FVehiclePhysicsHistoryInput&>(FromData);
	Input.Cmd.Throttle = (Input.Cmd.Throttle + Older.Input.Cmd.Throttle) * 0.5f;
	Input.Cmd.Brake = (Input.Cmd.Brake + Older.Input.Cmd.Brake) * 0.5f;
	Input.Cmd.Steering = (Input.Cmd.Steering + Older.Input.Cmd.Steering) * 0.5f;
	Input.Cmd.bHandbrake |= Older.Input.Cmd.bHandbrake;
	Input.Cmd.bSelfRight &= Older.Input.Cmd.bSelfRight; // Missing release must not become a fresh held press.
}

void FVehiclePhysicsHistoryInput::DecayData(float DecayAmount)
{
	const float Scale = 1.0f - FMath::Clamp(DecayAmount, 0.0f, 1.0f);
	Input.Cmd.Throttle *= Scale;
	Input.Cmd.Brake *= Scale;
	Input.Cmd.Steering *= Scale;
	if (Scale <= 0.0f) { Input.Cmd.bHandbrake = false; Input.Cmd.bSelfRight = false; }
}

void FVehiclePhysicsHistoryInput::ValidateData(const UActorComponent* NetworkComponent)
{
	if (!Input.IsValidContinuousInput())
	{
		Input.Cmd.Reset();
		Input.InputSequence = 0;
	}
}

bool FVehiclePhysicsHistoryInput::CompareData(const FNetworkPhysicsData& PredictedData)
{
	const auto& Predicted = static_cast<const FVehiclePhysicsHistoryInput&>(PredictedData);
	return FMath::IsNearlyEqual(Input.Cmd.Throttle, Predicted.Input.Cmd.Throttle, 0.01f)
		&& FMath::IsNearlyEqual(Input.Cmd.Brake, Predicted.Input.Cmd.Brake, 0.01f)
		&& FMath::IsNearlyEqual(Input.Cmd.Steering, Predicted.Input.Cmd.Steering, 0.01f)
		&& Input.Cmd.bHandbrake == Predicted.Input.Cmd.bHandbrake && Input.Cmd.bSelfRight == Predicted.Input.Cmd.bSelfRight;
}

bool FVehiclePhysicsHistoryInput::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	SerializeFrames(Ar);
	Ar << Input.Cmd.Throttle << Input.Cmd.Brake << Input.Cmd.Steering << Input.InputSequence;
	uint8 Handbrake = Input.Cmd.bHandbrake ? 1 : 0;
	Ar << Handbrake;
	uint8 SelfRight = Input.Cmd.bSelfRight ? 1 : 0;
	Ar << SelfRight;
	if (Ar.IsLoading())
	{
		Input.Cmd.bHandbrake = Handbrake != 0;
		Input.Cmd.bSelfRight = SelfRight != 0;
		Input.Cmd.bLaunch = false;
		Input.Cmd.bPrimarySkill = false;
		Input.PhysicsFrame = LocalFrame;
	}
	bOutSuccess = !Ar.IsError();
	return bOutSuccess;
}

void FVehiclePhysicsHistoryState::ApplyData(UActorComponent* NetworkComponent) const
{
	if (UArcadeVehicleMovementComponent* Movement = Cast<UArcadeVehicleMovementComponent>(NetworkComponent))
	{
		Movement->ApplyHistoryState(State);
	}
}

void FVehiclePhysicsHistoryState::BuildData(const UActorComponent* NetworkComponent)
{
	if (const UArcadeVehicleMovementComponent* Movement = Cast<UArcadeVehicleMovementComponent>(NetworkComponent))
	{
		Movement->BuildHistoryState(State, LocalFrame);
	}
}

void FVehiclePhysicsHistoryState::InterpolateData(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData)
{
	const auto& Min = static_cast<const FVehiclePhysicsHistoryState&>(MinData);
	const auto& Max = static_cast<const FVehiclePhysicsHistoryState&>(MaxData);
	const float Alpha = Max.LocalFrame != Min.LocalFrame
		? FMath::Clamp(float(LocalFrame - Min.LocalFrame) / float(Max.LocalFrame - Min.LocalFrame), 0.0f, 1.0f) : 1.0f;
	State = Alpha < 0.5f ? Min.State : Max.State;
	State.Position = FMath::Lerp(Min.State.Position, Max.State.Position, Alpha);
	State.Rotation = FQuat::Slerp(Min.State.Rotation, Max.State.Rotation, Alpha);
	State.LinearVelocity = FMath::Lerp(Min.State.LinearVelocity, Max.State.LinearVelocity, Alpha);
	State.AngularVelocityRadians = FMath::Lerp(Min.State.AngularVelocityRadians, Max.State.AngularVelocityRadians, Alpha);
	State.PhysicsFrame = LocalFrame;
	State.Recovery = LocalFrame < Max.LocalFrame ? Min.State.Recovery : Max.State.Recovery;
	// Movement parameters are discrete. Retain the timeline covering this frame;
	// never Lerp a multiplier or select a revision by nearest endpoint.
	const int32 ServerTarget = ServerFrame;
	FHALMovementSnapshot Value;
	State.MovementTimeline = Min.State.MovementTimeline;
	State.MovementTimeline.Merge(Max.State.MovementTimeline, FHALMovementTimeline::MaximumEntries - 2);
	if (State.MovementTimeline.Query(ServerTarget - 1, Value)) { State.MovementSnapshot = Value; }
	else { State.MovementSnapshot = {}; }
}

bool FVehiclePhysicsHistoryState::CompareData(const FNetworkPhysicsData& PredictedData)
{
	const auto& Predicted = static_cast<const FVehiclePhysicsHistoryState&>(PredictedData);
	if (State.bPoseDriveAllowed != Predicted.State.bPoseDriveAllowed
		|| !FMath::IsNearlyEqual(State.PoseStableSeconds,Predicted.State.PoseStableSeconds,.025f)
		|| !State.Recovery.Equals(Predicted.State.Recovery)) { return false; }
	// Chaos compares and rewinds the rigid body using the Settings Data Asset thresholds.
	// This comparison covers movement memory which Chaos does not know about.
	if (!State.MovementSnapshot.Equals(Predicted.State.MovementSnapshot)) { return false; }
	const auto& A = State.MovementTimeline;
	const auto& B = Predicted.State.MovementTimeline;
	if (!A.IsEmpty() && !B.IsEmpty() && A.Epoch == B.Epoch)
	{
		// Compare only shared known coverage. Sliding the window or filling unknown
		// frames alone is not a mismatch; a missed old boost remains a mismatch after expiry.
		for (int64 Frame = FMath::Max(A.StartFrame, B.StartFrame); Frame <= FMath::Min(A.EndFrame, B.EndFrame); ++Frame)
		{
			FHALMovementSnapshot Actual, Used;
			if (A.Query(int32(Frame), Actual) && B.Query(int32(Frame), Used) && !Actual.Equals(Used)) { return false; }
		}
	}
	if (State.bWallEscapeActive != Predicted.State.bWallEscapeActive
		|| !FMath::IsNearlyEqual(State.WallEscapeBlend, Predicted.State.WallEscapeBlend, 0.02f)
		|| State.WallEscapeNormals.Num() != Predicted.State.WallEscapeNormals.Num()) { return false; }
	for (int32 Index = 0; Index < State.WallEscapeNormals.Num(); ++Index)
	{
		if (!State.WallEscapeNormals[Index].Equals(Predicted.State.WallEscapeNormals[Index], 0.02f)) { return false; }
	}
	return true;
}

bool FVehiclePhysicsHistoryState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	SerializeFrames(Ar);
	Ar << State.Position << State.Rotation << State.LinearVelocity << State.AngularVelocityRadians;
	Ar << State.bPoseDriveAllowed << State.PoseStableSeconds;
	State.Recovery.Serialize(Ar);
	if (!FMath::IsFinite(State.PoseStableSeconds) || State.PoseStableSeconds < 0 || State.PoseStableSeconds > 1.1f) { Ar.SetError(); }
	State.MovementSnapshot.Serialize(Ar);
	if ((!State.MovementSnapshot.IsUnset() && !State.MovementSnapshot.IsValid()) || !State.MovementTimeline.Serialize(Ar))
	{
		Ar.SetError(); bOutSuccess = false; return false;
	}
	uint8 WallActive = State.bWallEscapeActive ? 1 : 0;
	Ar << WallActive << State.WallEscapeBlend;
	uint8 Count = Ar.IsSaving() ? uint8(FMath::Min(State.WallEscapeNormals.Num(), 2)) : 0;
	Ar << Count;
	if (Ar.IsLoading()) { State.WallEscapeNormals.SetNum(Count <= 2 ? Count : 0); }
	for (FVector& Normal : State.WallEscapeNormals) { Ar << Normal; }
	if (Ar.IsLoading())
	{
		State.bWallEscapeActive = WallActive != 0;
		State.PhysicsFrame = LocalFrame;
	}
	bOutSuccess = !Ar.IsError() && Count <= 2;
	return bOutSuccess;
}
