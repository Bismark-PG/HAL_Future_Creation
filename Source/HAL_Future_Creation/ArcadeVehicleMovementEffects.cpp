#include "ArcadeVehicleMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeLock.h"
#include "PBDRigidsSolver.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/PhysicsSettings.h"

static TAutoConsoleVariable<int32> CVarHALMovementEffectLog(TEXT("hal.MovementEffectLog"), 0,
	TEXT("Movement Epoch/revision, actual physics frame, values, replay and recovery. 0=off,1=on."));

void UArcadeVehicleMovementComponent::ConfigureMovementEffects(FGuid Epoch, UNetworkPhysicsComponent* History, bool bAuthority, int32 PlayerId)
{
	FScopeLock Lock(&PendingPhysicsDataLock);
	MovementPlayerId.Store(PlayerId);
	if (MovementEpochExternal == Epoch && MovementHistory == History && bMovementAuthority.Load() == bAuthority) { return; }
	MovementEpochExternal = Epoch;
	MovementHistory = History;
	bMovementAuthority.Store(bAuthority);
	bMovementTickOffsetReady.Store(bAuthority);
	bMovementResetPending = true;
	bStandaloneMovementPending = false;
}
FHALMovementStatus UArcadeVehicleMovementComponent::GetMovementEffectStatus() const
{
	FScopeLock Lock(&PendingPhysicsDataLock);
	return PublishedMovementStatus;
}
bool UArcadeVehicleMovementComponent::QueueMovementParameters(const FHALMovementSnapshot& Value)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Value.IsValidValues()) { return false; }
	UNetworkPhysicsComponent* History;
	FVehicleMovementParameterAction Action;
	Action.Value = Value;
	Action.FixedDeltaSeconds = GetDefault<UPhysicsSettings>()->AsyncFixedTimeStepSize;
	{
		FScopeLock Lock(&PendingPhysicsDataLock);
		if (Value.Epoch != MovementEpochExternal) { return false; }
		if (Value.bHasAreaState && Value.Revision > 1)
		{
			const auto& Previous = PublishedMovementStatus.Definition;
			// Coalesce GT changes until the previous command has an actual authority frame.
			// Never manufacture an origin from a stale render-frame value.
			if (!Previous.IsValid() || Previous.Epoch != Value.Epoch || Previous.Revision != Value.Revision - 1) { return false; }
			Action.Previous = Previous;
		}
		History = MovementHistory;
		if (!bUseNetworkPhysicsHistory.Load())
		{
			PendingStandaloneMovement = Action;
			bStandaloneMovementPending = true;
			return true;
		}
	}
	if (!History) { return false; }
	History->EnqueueImmediateAction_External(Action, uint32(0), true);
	return true;
}
void UArcadeVehicleMovementComponent::ApplyPendingMovementConfiguration()
{
	FScopeLock Lock(&PendingPhysicsDataLock);
	if (!bMovementResetPending) { return; }
	bMovementResetPending = false;
	MovementEpoch = MovementEpochExternal;
	MovementAuthorityTimeline = {};
	MovementActions.Reset(); UsedMovementFrames.Reset(); ProcessedMovementStates.Reset();
	CurrentMovement = {};
	BounceBackPhysics = {}; PendingBounceBack = {}; PublishedBounceBack = {}; bPendingBounceBack=false;
	BounceAuthorityFrame.Store(INDEX_NONE); BounceAuthoritySequence.Store(0);
	PendingBouncePredictionFrame=INDEX_NONE; PredictedBounceCount=0;
	CurrentMovementScales = FVector::OneVector;
	MovementRewindRequests = 0; MovementLastRewind = INDEX_NONE;
	LatestMovementAuthorityFrame = INDEX_NONE;
	bMovementRecovery = false; bMovementDriveAllowed = true;
	PublishedMovementStatus = {};
}
void UArcadeVehicleMovementComponent::ApplyMovementAction(const FVehicleMovementParameterAction& Action)
{
	ApplyPendingMovementConfiguration();
	if (!Action.Value.IsValidValues() || Action.Value.Epoch != MovementEpoch || !UpdatedPrimitive) { return; }
	if (Action.Value.bHasAreaState && (!FMath::IsFinite(Action.FixedDeltaSeconds) || Action.FixedDeltaSeconds <= 0
		|| Action.FixedDeltaSeconds > 1 || (Action.Value.Revision > 1 && (!Action.Previous.IsValid()
			|| Action.Previous.Epoch != Action.Value.Epoch || Action.Previous.Revision != Action.Value.Revision - 1)))) { return; }
	FBodyInstance* Body = UpdatedPrimitive->GetBodyInstance();
	auto* Solver = Body ? Body->GetPhysicsActor()->GetSolver<Chaos::FPhysicsSolver>() : nullptr;
	if (!Solver) { return; }
	const int32 Frame = Solver->GetCurrentFrame() + UE::NetworkPhysicsUtils::GetNetworkPhysicsTickOffset_Internal(Solver);
	if (Action.ServerFrame != INDEX_NONE && Action.ServerFrame != Frame) { return; }
	if (Action.Value.bHasAreaState && Action.Previous.IsValid() && Action.Previous.EffectiveServerFrame > Frame) { return; }
	FHALMovementSnapshot Value = Action.Value;
	Value.Stamp(&Action.Previous, Frame, Action.FixedDeltaSeconds);
	for (const auto& Existing : MovementActions)
	{
		if (Existing.Revision == Value.Revision)
		{
			if (!Existing.Equals(Value) && CVarHALMovementEffectLog.GetValueOnAnyThread())
			{
				UE_LOG(LogTemp, Warning, TEXT("Movement rejected conflicting revision: Player=%d Revision=%u Frame=%d"), MovementPlayerId.Load(), Value.Revision, Frame);
			}
			return; // Absolute duplicate; never accumulate scales or block other historical revisions.
		}
		if (Existing.EffectiveServerFrame == Frame && Existing.Revision > Value.Revision) { return; }
	}
	MovementActions.RemoveAll([Frame](const auto& Existing) { return Existing.EffectiveServerFrame == Frame; });
	MovementActions.Add(Value);
	MovementActions.Sort([](const auto& A, const auto& B) { return A.EffectiveServerFrame < B.EffectiveServerFrame; });
	while (MovementActions.Num() > MovementWindowFrames + 1) { MovementActions.RemoveAt(0); }
	if (bMovementAuthority.Load()) { MovementAuthorityTimeline.Append(Value, MovementWindowFrames); }
	if (CVarHALMovementEffectLog.GetValueOnAnyThread())
	{
		UE_LOG(LogTemp, Log, TEXT("Movement action: Player=%d Epoch=%s Revision=%u Frame=%d TargetAccel=%.4f TargetSpeed=%.4f TargetGrip=%.4f RecoveryFrames=%d/%d/%d ControlsBlocked=%d"),
			MovementPlayerId.Load(), *Value.Epoch.ToString(), Value.Revision, Frame, Value.AccelerationScale, Value.SpeedScale, Value.GripScale,
			Value.AreaAcceleration.DurationFrames, Value.AreaSpeed.DurationFrames, Value.AreaGrip.DurationFrames, Value.bControlsBlocked);
	}
}
bool UArcadeVehicleMovementComponent::RequestMovementRewind(Chaos::FPBDRigidsSolver* Solver, int32 LocalFrame, int32 RequestedLocalFrame, bool bRequireCheckpoint)
{
	if (!Solver || !MovementHistory || !bMovementTickOffsetReady.Load()) { return false; }
	auto* Rewind = Solver->GetRewindData();
	const auto& InputHistory = MovementHistory->GetInputHistory_Internal();
	const auto& StateHistory = MovementHistory->GetStateHistory_Internal();
	if (!Rewind || !InputHistory || !StateHistory || !Rewind->IsFrameWithinRewindHistory(RequestedLocalFrame)) { return false; }
	const int32 Frame = Rewind->FindValidResimFrame(RequestedLocalFrame);
	if (Frame == INDEX_NONE || Frame >= LocalFrame) { return false; }
	FVehiclePhysicsHistoryInput Input;
	FVehiclePhysicsHistoryState State;
	if (!InputHistory->ExtractData(Frame, false, &Input, true) || !StateHistory->ExtractData(Frame, false, &State, true)) { return false; }
	const int32 ServerFrame = Frame + UE::NetworkPhysicsUtils::GetNetworkPhysicsTickOffset_Internal(Solver);
	FHALMovementSnapshot AtFrame;
	const bool bCovered = MovementAuthorityTimeline.Query(ServerFrame, AtFrame);
	const bool bAuthorityAnchor = State.bReceivedData && State.State.MovementSnapshot.Epoch == MovementEpoch && State.State.MovementSnapshot.IsValid();
	if ((!bCovered && !bAuthorityAnchor) || (bRequireCheckpoint && !bAuthorityAnchor)) { return false; }
	FBodyInstance* Body = UpdatedPrimitive ? UpdatedPrimitive->GetBodyInstance() : nullptr;
	const auto Handle = Body ? Body->GetBodyInstanceAsyncPhysicsTickHandle() : FBodyInstanceAsyncPhysicsTickHandle();
	if (!Handle.IsValid()) { return false; }
	Rewind->RequestResimulation(Frame, Body->GetPhysicsActor()->GetHandle_LowLevel());
	++MovementRewindRequests; MovementLastRewind = Frame;
	if (CVarHALMovementEffectLog.GetValueOnAnyThread())
	{
		UE_LOG(LogTemp, Log, TEXT("Movement rewind: Player=%d Requested=%d Accepted=%d Current=%d Coverage=%d..%d"),
			MovementPlayerId.Load(), RequestedLocalFrame, Frame, LocalFrame, MovementAuthorityTimeline.StartFrame, MovementAuthorityTimeline.EndFrame);
	}
	return true;
}
void UArcadeVehicleMovementComponent::ImportMovementTimeline(const FHALMovementTimeline& Timeline, Chaos::FPBDRigidsSolver* Solver, int32 LocalFrame, bool bRequestRewind)
{
	if (!Timeline.Validate() || Timeline.IsEmpty() || Timeline.Epoch != MovementEpoch || bMovementAuthority.Load()) { return; }
	if (!MovementAuthorityTimeline.Merge(Timeline, MovementWindowFrames)) { return; }
	LatestMovementAuthorityFrame = FMath::Max(LatestMovementAuthorityFrame, Timeline.EndFrame);
	if (!bRequestRewind || !Solver || static_cast<Chaos::FPhysicsSolverBase*>(Solver)->IsResimming() || !bMovementTickOffsetReady.Load()) { return; }
	const int32 Offset = UE::NetworkPhysicsUtils::GetNetworkPhysicsTickOffset_Internal(Solver);
	int32 FirstDifference = MAX_int32;
	for (const auto& Used : UsedMovementFrames)
	{
		FHALMovementSnapshot Authority;
		if (Used.bMapped && Used.Epoch == MovementEpoch && Used.LocalFrame < LocalFrame
			&& Timeline.Query(Used.LocalFrame + Offset, Authority)
			&& (!Authority.Equals(Used.Value) || !Authority.EvaluateScales(Used.LocalFrame + Offset).Equals(Used.ConsumedScales,1.e-4)))
		{
			FirstDifference = FMath::Min(FirstDifference, Used.LocalFrame);
		}
	}
	if (FirstDifference != MAX_int32)
	{
		bMovementRecovery = !RequestMovementRewind(Solver, LocalFrame, FirstDifference);
	}
	if (bMovementRecovery)
	{
		// A later full checkpoint repairs a missed/evicted start, without moving that old event to now.
		bMovementRecovery = !RequestMovementRewind(Solver, LocalFrame, Timeline.EndFrame + 1 - Offset, true);
		if (!bMovementRecovery)
		{
			// This received checkpoint supersedes unrecoverable earlier predictions.
			// Do not request the same obsolete prefix again for every incoming packet.
			for (auto& Used : UsedMovementFrames)
			{
				if (Used.LocalFrame < MovementLastRewind) { Used.bMapped = false; }
			}
		}
	}
}
void UArcadeVehicleMovementComponent::CollectMovementAuthority(Chaos::FPBDRigidsSolver* Solver, int32 LocalFrame)
{
	if (!MovementHistory || bMovementAuthority.Load() || !MovementEpoch.IsValid() || !bMovementTickOffsetReady.Load()) { return; }
	const auto& History = MovementHistory->GetStateHistory_Internal();
	if (!History) { return; }
	const int32 Latest = History->GetLatestFrame();
	const int32 Earliest = FMath::Max(History->GetEarliestFrame(), Latest - MovementWindowFrames + 1);
	int32 Imported = 0;
	for (int32 Frame = Latest; Frame >= Earliest && Imported < 4; --Frame)
	{
		if (ProcessedMovementStates.Contains(Frame)) { continue; }
		FVehiclePhysicsHistoryState Data;
		if (History->ExtractData(Frame, false, &Data, true) && Data.bReceivedData && Data.State.MovementTimeline.Epoch == MovementEpoch)
		{
			ImportMovementTimeline(Data.State.MovementTimeline, Solver, LocalFrame, true);
			ProcessedMovementStates.Add(Frame); ++Imported;
		}
	}
	for (auto It = ProcessedMovementStates.CreateIterator(); It; ++It) { if (*It < Earliest) { It.RemoveCurrent(); } }
}
void UArcadeVehicleMovementComponent::PrepareMovementFrame(Chaos::FPBDRigidsSolver* Solver, int32 LocalFrame, bool bReplay)
{
	ApplyPendingMovementConfiguration();
	const int32 Offset = Solver ? UE::NetworkPhysicsUtils::GetNetworkPhysicsTickOffset_Internal(Solver) : 0;
	const int32 ServerFrame = LocalFrame >= 0 ? LocalFrame + Offset : INDEX_NONE;
	if (MovementHistory && bUseNetworkPhysicsHistory.Load())
	{
		const auto& History = MovementHistory->GetStateHistory_Internal();
		if (History)
		{
			const int32 Frames = History->GetHistorySize();
			if (Frames > FHALMovementTimeline::MaximumEntries - 2)
			{
				if (!bMovementRecovery)
				{
					UE_LOG(LogTemp, Error, TEXT("Movement bridge disabled: history size %d exceeds supported 254 frames."), Frames);
				}
				bMovementRecovery = true; CurrentMovement = {}; CurrentMovementScales = FVector::OneVector; bMovementDriveAllowed = !bReplay;
				FScopeLock Lock(&PendingPhysicsDataLock);
				PublishedMovementStatus = {};
				PublishedMovementStatus.bRecovery = true;
				PublishedMovementStatus.HistoryFrames = Frames;
				return;
			}
			MovementWindowFrames = FMath::Max(Frames, 2);
		}
	}
	if (!bReplay)
	{
		FVehicleMovementParameterAction Standalone;
		{
			FScopeLock Lock(&PendingPhysicsDataLock);
			if (bStandaloneMovementPending) { Standalone = PendingStandaloneMovement; bStandaloneMovementPending = false; }
		}
		if (Standalone.Value.IsValidValues() && Standalone.Value.Epoch == MovementEpoch)
		{
			Standalone.Value.Stamp(&Standalone.Previous, ServerFrame, Standalone.FixedDeltaSeconds);
			MovementAuthorityTimeline.Append(Standalone.Value, MovementWindowFrames);
		}
	}
	if (bMovementAuthority.Load() && !bReplay) { MovementAuthorityTimeline.ExtendTo(ServerFrame, MovementWindowFrames); }
	if (!bReplay) { CollectMovementAuthority(Solver, LocalFrame); }
	if (!bMovementAuthority.Load() && bMovementTickOffsetReady.Load() && LatestMovementAuthorityFrame >= 0
		&& int64(ServerFrame) - LatestMovementAuthorityFrame > MovementWindowFrames)
	{
		bMovementRecovery = true;
	}
	FHALMovementSnapshot Selected;
	bool bKnown = MovementAuthorityTimeline.Query(ServerFrame, Selected);
	// Outside the received future boundary this is explicitly prediction, using the last known value.
	if (!bKnown && !MovementAuthorityTimeline.IsEmpty() && ServerFrame > MovementAuthorityTimeline.EndFrame)
	{
		MovementAuthorityTimeline.Query(MovementAuthorityTimeline.EndFrame, Selected);
	}
	for (const auto& Value : MovementActions)
	{
		if (Value.EffectiveServerFrame <= ServerFrame && Value.Revision > Selected.Revision)
		{
			Selected = Value;
			bKnown = Value.EffectiveServerFrame == ServerFrame;
		}
	}
	bMovementDriveAllowed = true;
	if (bReplay && !Selected.IsValid())
	{
		const auto* Saved = UsedMovementFrames.FindByPredicate([&](const auto& Used) { return Used.LocalFrame == LocalFrame && Used.Epoch == MovementEpoch; });
		if (Saved && Saved->Value.IsValid()) { Selected = Saved->Value; }
		else { bMovementDriveAllowed = false; bMovementRecovery = true; }
	}
	CurrentMovement = Selected;
	CurrentMovementScales = Selected.EvaluateScales(ServerFrame);
	if (LocalFrame >= 0)
	{
		if (UsedMovementFrames.Num() != MovementWindowFrames) { UsedMovementFrames.SetNum(MovementWindowFrames); }
		auto& Used = UsedMovementFrames[LocalFrame % MovementWindowFrames];
		Used.LocalFrame = LocalFrame; Used.Epoch = MovementEpoch; Used.Value = Selected; Used.bMapped = bMovementTickOffsetReady.Load();
		Used.ConsumedScales = CurrentMovementScales;
	}
	FHALMovementStatus Status;
	Status.Definition = Selected; Status.Value = Selected;
	Status.Value.AccelerationScale = CurrentMovementScales.X; Status.Value.SpeedScale = CurrentMovementScales.Y; Status.Value.GripScale = CurrentMovementScales.Z;
	Status.LocalFrame = LocalFrame; Status.ServerFrame = ServerFrame;
	Status.HistoryFrames = MovementWindowFrames; Status.RewindRequests = MovementRewindRequests; Status.LastRewindFrame = MovementLastRewind;
	Status.bReady = Selected.IsValid() && bMovementTickOffsetReady.Load() && !bMovementRecovery;
	Status.bAuthoritative = bKnown; Status.bReplay = bReplay; Status.bRecovery = bMovementRecovery;
	const auto Recovering = [ServerFrame](const FHALMovementAreaChannel& Channel)
	{ return Channel.DurationFrames > 0 && int64(ServerFrame) < int64(Channel.StartFrame) + Channel.DurationFrames; };
	Status.bAreaRecovering = Selected.bHasAreaState && (Recovering(Selected.AreaAcceleration) || Recovering(Selected.AreaSpeed) || Recovering(Selected.AreaGrip));
	{
		FScopeLock Lock(&PendingPhysicsDataLock);
		PublishedMovementStatus = Status;
	}
	if (CVarHALMovementEffectLog.GetValueOnAnyThread() && (bReplay || (LocalFrame >= 0 && LocalFrame % 60 == 0)))
	{
		UE_LOG(LogTemp, Log, TEXT("Movement frame: Player=%d Epoch=%s Revision=%u ServerFrame=%d LocalFrame=%d Accel=%.4f Speed=%.4f Grip=%.4f Ready=%d Replay=%d Recovery=%d Window=%d Rewinds=%d Authority=%d ControlsBlocked=%d"),
			MovementPlayerId.Load(), *Selected.Epoch.ToString(), Selected.Revision, ServerFrame, LocalFrame,
			CurrentMovementScales.X, CurrentMovementScales.Y, CurrentMovementScales.Z, Status.bReady, bReplay, bMovementRecovery, MovementWindowFrames, MovementRewindRequests, bKnown, Selected.bControlsBlocked);
	}
}
