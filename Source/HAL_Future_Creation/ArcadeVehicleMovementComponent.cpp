// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcadeVehicleMovementComponent.h"
#include "VehicleConfigurationTypes.h"
#include "VehicleConfigurationApplication.h"
#include "UObject/UnrealType.h"

#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeLock.h"
#include "PBDRigidsSolver.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsEngine/BodyInstance.h"
#include "GameFramework/Pawn.h"
#include "VehicleWallEscapeMath.h"
#include "Chaos/SimCallbackObject.h"
#include "VehicleStabilityRules.h"

// UE 5.8 deliberately omits frozen-game-thread callbacks (including the generic
// ActorComponent AsyncPhysicsTick) during rewind. Keep its normal-step path and
// supply only the missing replay steps, after Network Physics restored inputs/actions.
class FVehicleRewindCallback : public Chaos::TSimCallbackObject<Chaos::FSimCallbackNoInput,
	Chaos::FSimCallbackNoOutput, Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::Rewind>
{
public:
	TAtomic<UArcadeVehicleMovementComponent*> Movement{nullptr};
	virtual FName GetFNameForStatId() const override { return TEXT("HALVehicleRewind"); }
	virtual void OnPreSimulate_Internal() override
	{
		if (!GetSolver()->IsResimming()) { return; }
		if (auto* Component = Movement.Load())
		{
			Component->AsyncPhysicsTickComponent(GetDeltaTime_Internal(), GetSimTime_Internal());
		}
	}
};

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarHALVehicleHistoryLog(
	TEXT("hal.VehicleHistoryLog"), 0,
	TEXT("Network Physics input source, at most once per 60 physics frames: 0=off, 1=on."));
static TAutoConsoleVariable<int32> CVarHALImpactPhysicsLog(
	TEXT("hal.ImpactPhysicsLog"), 0,
	TEXT("Log consumed impact actions, physics frame, current Internal V/W and queued deltas. 0=off, 1=on."));
#endif

UArcadeVehicleMovementComponent::UArcadeVehicleMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	RecoveryRules.bEnabled = false; // Legacy/manual fixtures until a world snapshot is loaded.
}

void UArcadeVehicleMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	if (auto* Shared = GetWorld()->GetSubsystem<UVehicleStabilitySubsystem>(); Shared && Shared->InitializeRules())
	{ RecoveryRules = Shared->GetRecoveryRules(); }

	if (!UpdatedPrimitive)
	{
		UpdatedPrimitive = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
	}

	if (!UpdatedPrimitive)
	{
		UE_LOG(LogTemp, Error, TEXT("%s requires a primitive component to drive."), *GetNameSafe(this));
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
		return;
	}
	const FBoxSphereBounds Bounds = UpdatedPrimitive->CalcBounds(FTransform(UpdatedPrimitive->GetComponentScale()));
	LocalBodyCenter = Bounds.Origin; LocalBodyExtent = Bounds.BoxExtent;
	// Existing Blueprint templates can have Auto Activate disabled; Chaos requires
	// every registered async-physics component to be active at callback time.
	if (!IsActive())
	{
		Activate(true);
	}
	RefreshPhysicsTick();
}

void UArcadeVehicleMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bShouldRunPhysics.Store(false);
	SetAsyncPhysicsTickEnabled(false);
	UnregisterRewindTick();
	Super::EndPlay(EndPlayReason);
}

void UArcadeVehicleMovementComponent::OnUnregister()
{
	UnregisterRewindTick();
	Super::OnUnregister();
}

void UArcadeVehicleMovementComponent::RegisterRewindTick()
{
	if (RewindTick || !HasBegunPlay() || !bUseNetworkPhysicsHistory.Load()) { return; }
	if (auto* World = GetWorld(); World && World->GetPhysicsScene())
	{
		RewindTickSolver = World->GetPhysicsScene()->GetSolver();
		RewindTick = RewindTickSolver->CreateAndRegisterSimCallbackObject_External<FVehicleRewindCallback>();
		RewindTick->Movement.Store(this);
	}
}

void UArcadeVehicleMovementComponent::UnregisterRewindTick()
{
	if (!RewindTick) { return; }
	// The solver can retain this callback in rewind history after removal. Revoke
	// its component pointer, then finish any in-flight call before component teardown.
	RewindTick->Movement.Store(nullptr);
	RewindTickSolver->WaitOnPendingTasks_External();
	RewindTickSolver->UnregisterAndFreeSimCallbackObject_External(RewindTick);
	RewindTick = nullptr; RewindTickSolver = nullptr;
}

void UArcadeVehicleMovementComponent::Activate(bool bReset)
{
	Super::Activate(bReset);
	if (HasBegunPlay() && IsActive() && UpdatedPrimitive && UpdatedPrimitive->IsSimulatingPhysics())
	{
		RefreshPhysicsTick();
	}
}

void UArcadeVehicleMovementComponent::Deactivate()
{
	bShouldRunPhysics.Store(false);
	SetAsyncPhysicsTickEnabled(false);
	Super::Deactivate();
}

void UArcadeVehicleMovementComponent::AsyncPhysicsTickComponent(float DeltaTime, float SimTime)
{
	Super::AsyncPhysicsTickComponent(DeltaTime, SimTime);
	if (!bShouldRunPhysics.Load()) { return; }
	int32 CurrentPhysicsFrame = INDEX_NONE;
	bool bIsResimming = false;
	Chaos::FPBDRigidsSolver* MovementSolver = nullptr;
	if (const UWorld* World = GetWorld())
	{
		if (const FPhysScene_Chaos* Scene = static_cast<const FPhysScene_Chaos*>(World->GetPhysicsScene()))
		{
			if (Chaos::FPhysicsSolver* Solver = Scene->GetSolver())
			{
				auto* RigidSolver = static_cast<Chaos::FPBDRigidsSolver*>(Solver);
				MovementSolver = RigidSolver;
				CurrentPhysicsFrame = RigidSolver->GetCurrentFrame();
				bIsResimming = static_cast<Chaos::FPhysicsSolverBase*>(Solver)->IsResimming();
			}
		}
	}
	if (bIsResimming && !bWasResimulating && bUseNetworkPhysicsHistory.Load())
	{
		++ResimulationRunCount;
	}
	bWasResimulating = bIsResimming;
	FVehicleNetInputData StepInput;
	const bool bUseLocalPendingInput = VehicleNetworkPhysics::ShouldUseLocalPendingInput(
		bUseNetworkPhysicsHistory.Load(), bOwnsLocalInput.Load(), bIsResimming);
	if (!bUseLocalPendingInput)
	{
		StepInput = AppliedHistoryInput;
		if (bHistoryInputBlocked.Load()) { StepInput.Cmd.Reset(); }
	}
	else
	{
		FScopeLock Lock(&PendingPhysicsDataLock);
		StepInput.Cmd = PendingInputCommand;
		StepInput.InputSequence = PendingNetworkInputSequence;
	}
	if (StepInput.InputSequence == 0) { StepInput.InputSequence = ++NextInputSequence; }
	StepInput.PhysicsFrame = CurrentPhysicsFrame;
	StepInput.Sanitize();
	if (bUseNetworkPhysicsHistory.Load() && bHistoryInputBlocked.Load()) { StepInput.Cmd.Reset(); }
	LastPhysicsInput = StepInput;
	LastPhysicsFrame.Store(StepInput.PhysicsFrame);
#if !UE_BUILD_SHIPPING
	if (bUseNetworkPhysicsHistory.Load() && CVarHALVehicleHistoryLog.GetValueOnAnyThread() > 0
		&& CurrentPhysicsFrame >= 0 && CurrentPhysicsFrame % 60 == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Vehicle history step: %s Frame=%d Source=%s Resim=%d Blocked=%d Throttle=%.2f Brake=%.2f Steering=%.2f"),
			*GetNameSafe(GetOwner()), CurrentPhysicsFrame, bUseLocalPendingInput ? TEXT("Local") : TEXT("History"),
			bIsResimming ? 1 : 0, bHistoryInputBlocked.Load() ? 1 : 0,
			StepInput.Cmd.Throttle, StepInput.Cmd.Brake, StepInput.Cmd.Steering);
	}
	if (!bLoggedFirstPhysicsStep)
	{
		bLoggedFirstPhysicsStep = true;
		UE_LOG(LogTemp, Log, TEXT("Vehicle physics step: %s DeltaTime=%.6f Frame=%d"),
			*GetNameSafe(GetOwner()), DeltaTime, StepInput.PhysicsFrame);
	}
#endif
	PrepareMovementFrame(MovementSolver, CurrentPhysicsFrame, bIsResimming);
	SimulateVehicleStep(StepInput, DeltaTime);
}

void UArcadeVehicleMovementComponent::SimulateVehicleStep(const FVehicleNetInputData& StepInput, float DeltaTime)
{
	InputCommand = StepInput.Cmd;
	// Use this physics frame's authority state, including during resimulation. Never read GAS here.
	if (CurrentMovement.bControlsBlocked) { InputCommand.Reset(); }

	if (!UpdatedPrimitive || !UpdatedPrimitive->IsSimulatingPhysics() || DeltaTime <= 0.0f)
	{
		bGrounded = false;
		ForwardSpeed = 0.0f;
		{ FScopeLock Lock(&PendingPhysicsDataLock); PublishedPresentationInput = FHALVehiclePresentationInput(); }
		ResetWallEscape();
		return;
	}
	FBodyInstance* Body = UpdatedPrimitive->GetBodyInstance();
	FBodyInstanceAsyncPhysicsTickHandle PhysicsHandle = Body
		? Body->GetBodyInstanceAsyncPhysicsTickHandle() : FBodyInstanceAsyncPhysicsTickHandle();
	if (!PhysicsHandle.IsValid())
	{
		{ FScopeLock Lock(&PendingPhysicsDataLock); PublishedPresentationInput = FHALVehiclePresentationInput(); }
		return;
	}
	FVector RecoilDeltaVelocity;
	{
		FScopeLock Lock(&PendingPhysicsDataLock);
		RecoilDeltaVelocity = PendingRecoilDeltaVelocity;
		PendingRecoilDeltaVelocity = FVector::ZeroVector;
	}
	if (!RecoilDeltaVelocity.IsNearlyZero())
	{
		FPhysicsInterface::AddVelocity_AssumesLocked(Body->GetPhysicsActor(), RecoilDeltaVelocity, true);
	}
	const FTransform BodyTransform(PhysicsHandle->R(), PhysicsHandle->X());
	GroundProbeRotation = BodyTransform.GetRotation();
	{
		FScopeLock Lock(&PendingPhysicsDataLock);
		if (bPendingRecovery) { RecoveryPhysics = PendingRecovery; bPendingRecovery = false; }
		if (bPendingBounceBack) { BounceBackPhysics=PendingBounceBack; bPendingBounceBack=false; }
	}

	// Replace horizontal motion on the Internal body at its center. No angular impulse,
	// no teleport, no GAS access. Chaos still resolves contacts and gravity normally.
	const bool bBouncing = BounceBackPhysics.bActive;
	if (bBouncing)
	{
		const FVector V=PhysicsHandle->V();
		FVector Target=FVector::ZeroVector;
		if (BounceBackPhysics.Elapsed < BounceBackPhysics.Duration)
		{ Target=BounceBackPhysics.StepVelocity(DeltaTime); }
		else { BounceBackPhysics.bActive=false; }
		FPhysicsInterface::AddVelocity_AssumesLocked(Body->GetPhysicsActor(),FVector(Target.X-V.X,Target.Y-V.Y,0),true);
		InputCommand.Reset();
	}
	FVector GroundNormal = FVector::UpVector;
	bGrounded = UpdateGroundContact(BodyTransform.GetLocation(), GroundNormal);
	// Recovery permission has its own support fallback. Never turn these samples into
	// wheel contact or ordinary driving support, and never extend their vertical reach.
	FVector RecoveryGroundNormal = GroundNormal;
	bool bRecoveryGrounded = bGrounded;
	if (RecoveryRules.bEnabled && !RecoveryRules.GroundIsValid(bGrounded,GroundNormal))
	{ bRecoveryGrounded = UpdateRecoverySupport(BodyTransform.GetLocation(),RecoveryGroundNormal); }
	if (RecoveryRules.bEnabled)
	{
		const bool bPoseValid = RecoveryRules.GroundIsValid(bGrounded,GroundNormal)
			&& RecoveryRules.CanDrive(bPoseDriveAllowed,GroundProbeRotation.GetUpVector(),GroundNormal) && !RecoveryPhysics.bActive;
		if (!bPoseValid) { bPoseDriveAllowed = false; PoseStableSeconds = 0; }
		else
		{
			PoseStableSeconds = FMath::Min(RecoveryRules.DriveStableSeconds, PoseStableSeconds+DeltaTime);
			bPoseDriveAllowed = PoseStableSeconds+KINDA_SMALL_NUMBER >= RecoveryRules.DriveStableSeconds;
		}
		if (RecoveryPhysics.bActive && !CurrentMovement.bControlsBlocked && !bBouncing)
		{
			// A finite Internal-step guard also stops assistance if the GT is temporarily stalled.
			const float Remaining = FMath::Max(0.f,RecoveryRules.MaximumExecutionSeconds-RecoveryPhysics.ElapsedSeconds);
			const float Step = FMath::Min(DeltaTime,Remaining);
			if (Step > 0)
			{
				const FVector Acceleration = HALVehicleRecovery::AngularAcceleration(RecoveryRules,RecoveryPhysics,
					GroundProbeRotation.GetUpVector(),PhysicsHandle->W(),DeltaTime) * (Step/DeltaTime);
				FPhysicsInterface::AddTorque_AssumesLocked(Body->GetPhysicsActor(),Acceleration,false,true,true);
				RecoveryPhysics.ElapsedSeconds += Step;
			}
			if (RecoveryPhysics.ElapsedSeconds+KINDA_SMALL_NUMBER >= RecoveryRules.MaximumExecutionSeconds) { RecoveryPhysics.bActive = false; }
		}
	}
	else { bPoseDriveAllowed = bGrounded; }

	const FVector Velocity = PhysicsHandle->V();
	const FVector ForwardDirection = FVector::VectorPlaneProject(
		BodyTransform.GetRotation().GetForwardVector(), GroundNormal).GetSafeNormal();
	const FVector RightDirection = FVector::CrossProduct(GroundNormal, ForwardDirection).GetSafeNormal();

	ForwardSpeed = FVector::DotProduct(Velocity, ForwardDirection);
	{
		FScopeLock Lock(&PendingPhysicsDataLock);
		PublishedPresentationInput.Steering = FMath::IsFinite(InputCommand.Steering)
			&& bPoseDriveAllowed ? FMath::Clamp(InputCommand.Steering, -1.f, 1.f) : 0;
		PublishedPresentationInput.PhysicsFrame = StepInput.PhysicsFrame;
		PublishedPresentationInput.bGrounded = bGrounded;
		PublishedPresentationInput.bValid = true;
		PublishedPresentationInput.TireSkidInputs = bPoseDriveAllowed && bMovementDriveAllowed && !CurrentMovement.bControlsBlocked
			? HALVehiclePresentation::TireSkidInputs(InputCommand.Brake, InputCommand.Throttle, InputCommand.bHandbrake) : 0;
		PublishedStability.PhysicsFrame = StepInput.PhysicsFrame;
		PublishedStability.bValid = true;
		PublishedStability.bNearGround = bRecoveryGrounded;
		PublishedStability.bDrivingAllowed = bPoseDriveAllowed;
		PublishedStability.bRecoveryHeld = InputCommand.bSelfRight;
		PublishedStability.Up = GroundProbeRotation.GetUpVector();
		PublishedStability.Forward = GroundProbeRotation.GetForwardVector();
		PublishedStability.GroundNormal = RecoveryGroundNormal;
		PublishedStability.Speed = PhysicsHandle->V().Size();
		PublishedStability.AngularSpeed = PhysicsHandle->W().Size();
		PublishedStability.Recovery = RecoveryPhysics;
		PublishedBounceBack = BounceBackPhysics;
		PublishedStability.PoseStableSeconds = PoseStableSeconds;
	}

	if (bBouncing || !bGrounded || !bPoseDriveAllowed || CurrentMovement.bControlsBlocked || ForwardDirection.IsNearlyZero() || RightDirection.IsNearlyZero())
	{
		ResetWallEscape();
		return;
	}

	const float LateralSpeed = FVector::DotProduct(Velocity, RightDirection);
	UpdateWallEscape(BodyTransform, GroundNormal, ForwardDirection, Velocity, DeltaTime);

	ApplyLongitudinalForces(*Body, ForwardDirection, ForwardSpeed);
	ApplyLateralGrip(*Body, RightDirection, LateralSpeed);
	ApplySteering(*Body, GroundNormal, ForwardSpeed,
		FVector::DotProduct(PhysicsHandle->W(), GroundNormal), DeltaTime);
}

void UArcadeVehicleMovementComponent::SetUpdatedPrimitive(UPrimitiveComponent* InPrimitive)
{
	UpdatedPrimitive = InPrimitive;
	ResetWallEscape();
}

FHALVehiclePresentationInput UArcadeVehicleMovementComponent::GetPresentationInput() const
{
	if (!bShouldRunPhysics.Load()) { return FHALVehiclePresentationInput(); }
	FScopeLock Lock(&PendingPhysicsDataLock);
	return PublishedPresentationInput;
}

FVehicleStabilityObservation UArcadeVehicleMovementComponent::GetStabilityObservation() const
{
	if (!bShouldRunPhysics.Load()) { return {}; }
	FScopeLock Lock(&PendingPhysicsDataLock); return PublishedStability;
}

bool UArcadeVehicleMovementComponent::QueueRecovery(const FVehicleRecoveryPhysicsState& Value)
{
	check(IsInGameThread());
	if (!GetOwner()->HasAuthority() || !RecoveryRules.bEnabled || !Value.Sequence || !Value.IsValid()
		|| !UpdatedPrimitive || !UpdatedPrimitive->IsSimulatingPhysics()) { return false; }
	if (bUseNetworkPhysicsHistory.Load())
	{
		auto* History = GetOwner()->FindComponentByClass<UNetworkPhysicsComponent>();
		if (!History) { return false; }
		FVehicleRecoveryAction Action; Action.Value = Value;
		History->EnqueueImmediateAction_External(Action,uint32(0),true);
	}
	else { FScopeLock Lock(&PendingPhysicsDataLock); PendingRecovery = Value; bPendingRecovery = true; }
	if (Value.bActive) { UpdatedPrimitive->WakeAllRigidBodies(); }
	return true;
}

void UArcadeVehicleMovementComponent::SetInputCommand(const FVehicleInputCmd& InInputCommand)
{
	FScopeLock Lock(&PendingPhysicsDataLock);
	PendingInputCommand = InInputCommand;
	PendingInputCommand.Sanitize();
	PendingNetworkInputSequence = 0;
}

void UArcadeVehicleMovementComponent::SetNetworkInput(const FVehicleNetInputData& InInputData)
{
	FScopeLock Lock(&PendingPhysicsDataLock);
	PendingInputCommand = InInputData.Cmd;
	PendingInputCommand.Sanitize();
	PendingInputCommand.bLaunch = false;
	PendingNetworkInputSequence = InInputData.InputSequence;
}

void UArcadeVehicleMovementComponent::EnableNetworkPhysicsHistory()
{
	bUseNetworkPhysicsHistory.Store(true);
	RefreshPhysicsTick();
}

void UArcadeVehicleMovementComponent::SetHistoryInputBlocked(bool bBlocked)
{
	bHistoryInputBlocked.Store(bBlocked);
}

void UArcadeVehicleMovementComponent::RefreshPhysicsTick()
{
	const AActor* Owner = GetOwner();
	const APawn* Pawn = Cast<APawn>(Owner);
	bOwnsLocalInput.Store(Pawn && Pawn->IsLocallyControlled());
	const bool bEnable = Owner && IsActive() && UpdatedPrimitive && UpdatedPrimitive->IsSimulatingPhysics()
		&& (Owner->HasAuthority() || (bUseNetworkPhysicsHistory.Load() && Pawn && Pawn->IsLocallyControlled()));
	bShouldRunPhysics.Store(bEnable);
	if (bEnable) { RegisterRewindTick(); }
	SetAsyncPhysicsTickEnabled(bEnable);
}

void UArcadeVehicleMovementComponent::BuildHistoryInput(FVehicleNetInputData& OutInput, int32 PhysicsFrame) const
{
	FScopeLock Lock(&PendingPhysicsDataLock);
	OutInput.Cmd = PendingInputCommand;
	if (bHistoryInputBlocked.Load()) { OutInput.Cmd.Reset(); }
	OutInput.Cmd.bLaunch = false;
	OutInput.InputSequence = ++NextInputSequence;
	if (OutInput.InputSequence == 0) { OutInput.InputSequence = ++NextInputSequence; }
	OutInput.PhysicsFrame = PhysicsFrame;
	OutInput.Sanitize();
}

void UArcadeVehicleMovementComponent::ApplyHistoryInput(const FVehicleNetInputData& InInput, int32 PhysicsFrame)
{
	AppliedHistoryInput = InInput;
	if (bHistoryInputBlocked.Load()) { AppliedHistoryInput.Cmd.Reset(); }
	AppliedHistoryInput.PhysicsFrame = PhysicsFrame;
	AppliedHistoryInput.Sanitize();
}

void UArcadeVehicleMovementComponent::BuildHistoryState(FVehicleNetStateData& OutState, int32 PhysicsFrame) const
{
	const FBodyInstance* Body = UpdatedPrimitive ? UpdatedPrimitive->GetBodyInstance() : nullptr;
	FBodyInstanceAsyncPhysicsTickHandle Handle = Body
		? Body->GetBodyInstanceAsyncPhysicsTickHandle() : FBodyInstanceAsyncPhysicsTickHandle();
	if (!Handle.IsValid()) { return; }
	OutState.PhysicsFrame = PhysicsFrame;
	OutState.Position = Handle->X();
	OutState.Rotation = Handle->R();
	OutState.LinearVelocity = Handle->V();
	OutState.AngularVelocityRadians = Handle->W();
	OutState.bWallEscapeActive = bWallEscapeActive;
	OutState.WallEscapeBlend = WallEscapeBlend;
	OutState.WallEscapeNormals = WallEscapeNormals;
	OutState.MovementSnapshot = CurrentMovement;
	OutState.MovementConsumedScales = CurrentMovementScales;
	OutState.MovementTimeline = MovementAuthorityTimeline;
	OutState.bPoseDriveAllowed = bPoseDriveAllowed;
	OutState.PoseStableSeconds = PoseStableSeconds;
	OutState.Recovery = RecoveryPhysics;
	OutState.BounceBack = BounceBackPhysics;
}

void UArcadeVehicleMovementComponent::ApplyHistoryState(const FVehicleNetStateData& InState)
{
	ApplyPendingMovementConfiguration();
	FBodyInstance* Body = UpdatedPrimitive ? UpdatedPrimitive->GetBodyInstance() : nullptr;
	auto* Solver = Body ? Body->GetPhysicsActor()->GetSolver<Chaos::FPhysicsSolver>() : nullptr;
	ImportMovementTimeline(InState.MovementTimeline, Solver, Solver ? Solver->GetCurrentFrame() : INDEX_NONE, false);
	if (InState.MovementSnapshot.IsValid() && InState.MovementSnapshot.Epoch == MovementEpoch)
	{ CurrentMovement = InState.MovementSnapshot; CurrentMovementScales = InState.MovementConsumedScales; }
	// Chaos owns the rigid-body rewind. The custom history restores only movement memory.
	bWallEscapeActive = InState.bWallEscapeActive;
	WallEscapeBlend = FMath::Clamp(InState.WallEscapeBlend, 0.0f, 1.0f);
	WallEscapeNormals = InState.WallEscapeNormals;
	if (InState.Recovery.IsValid()) { RecoveryPhysics = InState.Recovery; }
	if (InState.BounceBack.IsValid()) { BounceBackPhysics = InState.BounceBack; }
	bPoseDriveAllowed = InState.bPoseDriveAllowed;
	PoseStableSeconds = FMath::Clamp(InState.PoseStableSeconds,0.f,RecoveryRules.DriveStableSeconds);
}

void UArcadeVehicleMovementComponent::ResetInputCommand()
{
	FScopeLock Lock(&PendingPhysicsDataLock);
	PendingInputCommand.Reset();
	PendingNetworkInputSequence = 0;
}

void UArcadeVehicleMovementComponent::QueueRecoil(const FVector& DeltaVelocity)
{
	if (!DeltaVelocity.ContainsNaN())
	{
		FScopeLock Lock(&PendingPhysicsDataLock);
		PendingRecoilDeltaVelocity += DeltaVelocity;
	}
}

void UArcadeVehicleMovementComponent::ApplyAction_Internal(
	const TInstancedStruct<FNetworkPhysicsActionPayload>& ActionInstance)
{
	if (const auto* Bounce = ActionInstance.GetPtr<FVehicleBounceBackAction>())
	{
		if (bShouldRunPhysics.Load() && Bounce->Value.IsValid() && Bounce->Value.bActive
			&& FVehicleNetInputData::IsNewerSequence(Bounce->Value.Sequence,BounceBackPhysics.Sequence))
		{ BounceBackPhysics = Bounce->Value; }
		return;
	}
	if (const auto* Recovery = ActionInstance.GetPtr<FVehicleRecoveryAction>())
	{
		if (bShouldRunPhysics.Load() && RecoveryRules.bEnabled && Recovery->Value.IsValid()
			&& FVehicleNetInputData::IsNewerSequence(Recovery->Value.Sequence,RecoveryPhysics.Sequence))
		{ RecoveryPhysics = Recovery->Value; }
		return;
	}
	if (const auto* Parameters = ActionInstance.GetPtr<FVehicleMovementParameterAction>())
	{
		ApplyMovementAction(*Parameters);
		return;
	}
	if (const auto* Impact = ActionInstance.GetPtr<FVehicleImpactAction>())
	{
		if (!bShouldRunPhysics.Load() || !Impact->IsValid() || !UpdatedPrimitive) { return; }
		FBodyInstance* Body = UpdatedPrimitive->GetBodyInstance();
		FBodyInstanceAsyncPhysicsTickHandle PhysicsHandle = Body
			? Body->GetBodyInstanceAsyncPhysicsTickHandle() : FBodyInstanceAsyncPhysicsTickHandle();
		if (PhysicsHandle.IsValid())
		{
			const auto& Handle = Body->GetPhysicsActor();
			// This callback runs in Chaos: AssumesLocked V/W getters and SetAngularVelocity use the GT API.
			// Resolve against the same Internal frame and enqueue both impulses on that Internal body.
			const FVector BeforeV = PhysicsHandle->V();
			const FVector BeforeW = PhysicsHandle->W();
			const FVector DeltaV = Impact->ResolveLinearDelta(BeforeV);
			const FVector DeltaW = Impact->bSetPlanarAngularVelocity
				? Impact->ResolveAngularVelocity(BeforeW) - BeforeW : FVector::ZeroVector;
			FPhysicsInterface::AddVelocity_AssumesLocked(Handle, DeltaV, true);
			if (Impact->bSetPlanarAngularVelocity)
			{
				FPhysicsInterface::AddAngularVelocityInRadians_AssumesLocked(Handle, DeltaW, true);
			}
#if !UE_BUILD_SHIPPING
			if (CVarHALImpactPhysicsLog.GetValueOnAnyThread() > 0)
			{
				auto* Solver = Handle->GetSolver<Chaos::FPhysicsSolver>();
				UE_LOG(LogTemp, Log, TEXT("Vehicle impact physics: %s Event=%u Frame=%d BeforeV=%s BeforeW=%s QueuedDV=%s QueuedDW=%s"),
					*GetNameSafe(GetOwner()), Impact->EventSequence, Solver ? Solver->GetCurrentFrame() : INDEX_NONE,
					*BeforeV.ToCompactString(), *BeforeW.ToCompactString(), *DeltaV.ToCompactString(), *DeltaW.ToCompactString());
			}
#endif
		}
		return;
	}
	const FVehicleRecoilAction* Recoil = ActionInstance.GetPtr<FVehicleRecoilAction>();
	if (!bShouldRunPhysics.Load() || !Recoil || Recoil->LaunchSequence == 0
		|| Recoil->DeltaVelocity.ContainsNaN() || Recoil->DeltaVelocity.IsNearlyZero()
		|| !UpdatedPrimitive) { return; }
	FBodyInstance* Body = UpdatedPrimitive->GetBodyInstance();
	if (Body && Body->GetBodyInstanceAsyncPhysicsTickHandle().IsValid())
	{
		FPhysicsInterface::AddVelocity_AssumesLocked(Body->GetPhysicsActor(), Recoil->DeltaVelocity, true);
	}
}

bool UArcadeVehicleMovementComponent::QueueBounceBack(const FVector& Direction, float Distance, float Duration)
{
	if (!GetOwner()->HasAuthority() || !UpdatedPrimitive || !UpdatedPrimitive->IsSimulatingPhysics()) { return false; }
	FVehicleBounceBackAction Action;
	Action.Value.Direction=Direction; Action.Value.Distance=Distance; Action.Value.Duration=Duration;
	Action.Value.Sequence=++NextBounceBackSequence;
	if (!Action.Value.Sequence) { Action.Value.Sequence=++NextBounceBackSequence; }
	Action.Value.bActive=true;
	if (!Action.Value.IsValid()) { return false; }
	if (bUseNetworkPhysicsHistory.Load())
	{
		auto* History=GetOwner()->FindComponentByClass<UNetworkPhysicsComponent>();
		if (!History) { return false; }
		History->EnqueueImmediateAction_External(Action,uint32(0),true);
	}
	else
	{
		FScopeLock Lock(&PendingPhysicsDataLock);
		PendingBounceBack=Action.Value; bPendingBounceBack=true;
	}
	return true;
}

bool UArcadeVehicleMovementComponent::QueueImpact(FVehicleImpactAction& Action)
{
	if (!GetOwner()->HasAuthority() || !Action.IsValid() || !UpdatedPrimitive || !UpdatedPrimitive->IsSimulatingPhysics()) { return false; }
	if (bUseNetworkPhysicsHistory.Load())
	{
		UNetworkPhysicsComponent* History = GetOwner()->FindComponentByClass<UNetworkPhysicsComponent>();
		if (!History) { return false; }
		// 0 is the engine's owner-source identity; recoil uses nonzero launch sequence IDs.
		// Do not hash this unreplicated component: dynamic Pawn names are not network-stable.
		History->EnqueueImmediateAction_External(Action, uint32(0), true);
	}
	else
	{
		UpdatedPrimitive->AddImpulse(Action.ResolveLinearDelta(UpdatedPrimitive->GetPhysicsLinearVelocity()), NAME_None, true);
		if (Action.bSetPlanarAngularVelocity)
		{
			UpdatedPrimitive->SetPhysicsAngularVelocityInRadians(
				Action.ResolveAngularVelocity(UpdatedPrimitive->GetPhysicsAngularVelocityInRadians()));
		}
	}
	return true;
}

bool UArcadeVehicleMovementComponent::CaptureNetState(FVehicleNetStateData& OutState) const
{
	const FBodyInstance* Body = UpdatedPrimitive ? UpdatedPrimitive->GetBodyInstance() : nullptr;
	if (!Body || !UpdatedPrimitive->IsSimulatingPhysics())
	{
		return false;
	}
	const FTransform BodyTransform = Body->GetUnrealWorldTransform();
	OutState.PhysicsFrame = GetLastPhysicsFrame();
	OutState.Position = BodyTransform.GetLocation();
	OutState.Rotation = BodyTransform.GetRotation();
	OutState.LinearVelocity = Body->GetUnrealWorldVelocity();
	OutState.AngularVelocityRadians = Body->GetUnrealWorldAngularVelocityInRadians();
	OutState.bWallEscapeActive = bWallEscapeActive;
	OutState.WallEscapeBlend = WallEscapeBlend;
	OutState.WallEscapeNormals = WallEscapeNormals;
	{
		FScopeLock Lock(&PendingPhysicsDataLock);
		OutState.PendingRecoilDeltaVelocity = PendingRecoilDeltaVelocity;
		OutState.Recovery = PublishedStability.Recovery;
		OutState.BounceBack = PublishedBounceBack;
		OutState.bPoseDriveAllowed = PublishedStability.bDrivingAllowed;
		OutState.PoseStableSeconds = PublishedStability.PoseStableSeconds;
	}
	return true;
}

bool UArcadeVehicleMovementComponent::RestoreNetState(const FVehicleNetStateData& State)
{
	if (!UpdatedPrimitive || !UpdatedPrimitive->IsSimulatingPhysics()
		|| State.Position.ContainsNaN() || State.Rotation.ContainsNaN() || !State.Rotation.IsNormalized()
		|| State.LinearVelocity.ContainsNaN() || State.AngularVelocityRadians.ContainsNaN()
		|| State.PendingRecoilDeltaVelocity.ContainsNaN() || !State.Recovery.IsValid() || !State.BounceBack.IsValid()
		|| !FMath::IsFinite(State.PoseStableSeconds) || State.PoseStableSeconds < 0 || State.PoseStableSeconds > 1
		|| !FMath::IsFinite(State.WallEscapeBlend) || State.WallEscapeBlend < 0.0f
		|| State.WallEscapeBlend > 1.0f || State.WallEscapeNormals.Num() > 2)
	{
		return false;
	}
	for (const FVector& Normal : State.WallEscapeNormals)
	{
		if (Normal.ContainsNaN() || !Normal.IsNormalized()) { return false; }
	}
	UpdatedPrimitive->SetWorldLocationAndRotation(State.Position, State.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	UpdatedPrimitive->SetPhysicsLinearVelocity(State.LinearVelocity);
	UpdatedPrimitive->SetPhysicsAngularVelocityInRadians(State.AngularVelocityRadians);
	bWallEscapeActive = State.bWallEscapeActive;
	WallEscapeBlend = State.WallEscapeBlend;
	WallEscapeNormals = State.WallEscapeNormals;
	RecoveryPhysics = State.Recovery;
	BounceBackPhysics = State.BounceBack;
	bPoseDriveAllowed = State.bPoseDriveAllowed;
	PoseStableSeconds = State.PoseStableSeconds;
	{
		FScopeLock Lock(&PendingPhysicsDataLock);
		PendingRecoilDeltaVelocity = State.PendingRecoilDeltaVelocity;
	}
	return true;
}

void UArcadeVehicleMovementComponent::ResetWallEscape()
{
	bWallEscapeActive = false;
	WallEscapeBlend = 0.0f;
	WallEscapeNormals.Reset();
}

void UArcadeVehicleMovementComponent::UpdateWallEscape(
	const FTransform& BodyTransform, const FVector& GroundNormal, const FVector& ForwardDirection,
	const FVector& Velocity, float DeltaTime)
{
	// This is not landing assistance or an emergency flip system.
	if (!bEnableWallEscape || FVector::DotProduct(BodyTransform.GetRotation().GetUpVector(), GroundNormal) < 0.5f)
	{
		ResetWallEscape();
		return;
	}

	const float PlanarSpeed = FVector::VectorPlaneProject(Velocity, GroundNormal).Size();
	const bool bEligible = VehicleWallEscape::IsEligible(InputCommand, PlanarSpeed, bWallEscapeActive,
		WallEscapeEnterSpeed, WallEscapeExitSpeed, WallEscapeMinimumSteering);
	TArray<FVector> NewNormals;
	UWorld* World = GetWorld();
	if (bEligible && World)
	{
		const FVector Start = BodyTransform.GetLocation();
		const FVector End = Start + ForwardDirection * FMath::Clamp(WallEscapeProbeDistance, 1.0f, 100.0f);
		FComponentQueryParams Params(SCENE_QUERY_STAT(VehicleWallEscapeProbe), GetOwner());
		Params.bIgnoreTouches = true;
		Params.bFindInitialOverlaps = true;

		// A supporting floor (or a car) can be the first blocking hit at time zero.
		// Retry ignoring each encountered blocker so it cannot hide the actual wall.
		// Bound query cost, and retain at most two distinct walls for a corner.
		for (int32 Attempt = 0; Attempt < 4 && NewNormals.Num() < 2; ++Attempt)
		{
			TArray<FHitResult> Hits;
			World->ComponentSweepMulti(Hits, UpdatedPrimitive, Start, End,
				BodyTransform.GetRotation(), Params);
			bool bFoundBlocker = false;
			for (const FHitResult& Hit : Hits)
			{
				UPrimitiveComponent* Other = Hit.GetComponent();
				if (!Hit.bBlockingHit || !Other)
				{
					continue;
				}
				bFoundBlocker = true;
				Params.AddIgnoredComponent(Other);
				FVector Normal = Hit.ImpactNormal.GetSafeNormal();
				if (Normal.IsNearlyZero())
				{
					Normal = Hit.Normal.GetSafeNormal();
				}
				// Static walls only: do not weaken car-to-car pushing or ball physics.
				// Reject support surfaces and steeply upward/downward-facing bevels.
				if (Other->GetCollisionObjectType() != ECC_WorldStatic
					|| Normal.IsNearlyZero() || FMath::Abs(FVector::DotProduct(Normal, GroundNormal)) > 0.35f)
				{
					continue;
				}
				Normal = FVector::VectorPlaneProject(Normal, GroundNormal).GetSafeNormal();
				if (FVector::DotProduct(ForwardDirection, Normal) >= -0.15f)
				{
					continue;
				}
				const bool bDuplicate = NewNormals.ContainsByPredicate([&Normal](const FVector& Existing)
				{
					return FVector::DotProduct(Existing, Normal) > 0.99f;
				});
				if (!bDuplicate && NewNormals.Num() < 2)
				{
					NewNormals.Add(Normal);
				}
			}
			if (!bFoundBlocker)
			{
				break;
			}
		}
		if (bDrawWallEscapeDebug && IsInGameThread())
		{
			DrawDebugLine(World, Start, End, FColor::Cyan, false, 0.0f, 0, 3.0f);
		}
	}

	bWallEscapeActive = !NewNormals.IsEmpty();
	if (bWallEscapeActive)
	{
		WallEscapeNormals = MoveTemp(NewNormals);
	}
	WallEscapeBlend = VehicleWallEscape::UpdateBlend(WallEscapeBlend, bWallEscapeActive,
		DeltaTime, WallEscapeBlendInTime, WallEscapeBlendOutTime);
	if (WallEscapeBlend <= 0.0f)
	{
		WallEscapeNormals.Reset();
	}
	if (bDrawWallEscapeDebug && World && IsInGameThread())
	{
		const FVector Start = BodyTransform.GetLocation();
		for (const FVector& Normal : WallEscapeNormals)
		{
			DrawDebugDirectionalArrow(World, Start, Start + Normal * 100.0f, 15.0f,
				bWallEscapeActive ? FColor::Green : FColor::Orange, false, 0.0f, 0, 3.0f);
		}
	}
}

bool UArcadeVehicleMovementComponent::UpdateGroundContact(const FVector& BodyLocation, FVector& OutGroundNormal) const
{
	UWorld* World = GetWorld();
	if (!World || !UpdatedPrimitive)
	{
		return false;
	}

	const FVector TraceStart = BodyLocation + (RecoveryRules.bEnabled ? GroundProbeRotation.RotateVector(LocalBodyCenter) : FVector::ZeroVector);
	const float ExtentZ = RecoveryRules.bEnabled
		? FMath::Abs(GroundProbeRotation.GetAxisX().Z)*LocalBodyExtent.X
			+ FMath::Abs(GroundProbeRotation.GetAxisY().Z)*LocalBodyExtent.Y
			+ FMath::Abs(GroundProbeRotation.GetAxisZ().Z)*LocalBodyExtent.Z : UpdatedPrimitive->Bounds.BoxExtent.Z;
	const float TraceLength = ExtentZ + (RecoveryRules.bEnabled ? RecoveryRules.SupportDistance : GroundTraceExtraDistance);
	const FVector TraceEnd = TraceStart - (FVector::UpVector * TraceLength);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArcadeVehicleGroundTrace), false, GetOwner());
	FHitResult GroundHit;
	const bool bHit = World->LineTraceSingleByChannel(
		GroundHit,
		TraceStart,
		TraceEnd,
		GroundTraceChannel,
		QueryParams);

	if (bDrawGroundDebug && IsInGameThread())
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

bool UArcadeVehicleMovementComponent::UpdateRecoverySupport(const FVector& BodyLocation, FVector& OutGroundNormal) const
{
	const UWorld* World = GetWorld();
	if (!World || !UpdatedPrimitive) { return false; }
	const FVector Center = BodyLocation + GroundProbeRotation.RotateVector(LocalBodyCenter);
	const float ExtentZ = FMath::Abs(GroundProbeRotation.GetAxisX().Z)*LocalBodyExtent.X
		+ FMath::Abs(GroundProbeRotation.GetAxisY().Z)*LocalBodyExtent.Y
		+ FMath::Abs(GroundProbeRotation.GetAxisZ().Z)*LocalBodyExtent.Z;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(VehicleRecoverySupport),false,GetOwner());
	// Sample inside the projected proxy, in a fixed order. A center ray can hit a wall
	// before the floor or miss a narrow supporting ledge when the car rests on its side.
	for (const FVector& LocalOffset : {FVector(LocalBodyExtent.X*.5,0,0),FVector(0,LocalBodyExtent.Y*.5,0),FVector(0,0,LocalBodyExtent.Z*.5)})
	{
		FVector Offset = GroundProbeRotation.RotateVector(LocalOffset); Offset.Z = 0;
		if (Offset.IsNearlyZero()) { continue; }
		for (const float Sign : {1.f,-1.f})
		{
			const FVector Start = Center + Offset*Sign;
			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit,Start,Start-FVector::UpVector*(ExtentZ+RecoveryRules.SupportDistance),GroundTraceChannel,Params)
				&& !Hit.bStartPenetrating && RecoveryRules.GroundIsValid(true,Hit.ImpactNormal.GetSafeNormal()))
			{ OutGroundNormal = Hit.ImpactNormal.GetSafeNormal(); return true; }
		}
	}
	return false;
}

void UArcadeVehicleMovementComponent::ApplyLongitudinalForces(
	FBodyInstance& Body,
	const FVector& ForwardDirection,
	float CurrentForwardSpeed)
{
	// Brake wins when both face buttons are held, producing deterministic input.
	const bool bBrakeHeld = InputCommand.Brake > KINDA_SMALL_NUMBER;
	const bool bThrottleHeld = InputCommand.Throttle > KINDA_SMALL_NUMBER && !bBrakeHeld;
	const float EffectiveForwardAcceleration = ForwardAcceleration * CurrentMovementScales.X;
	const float EffectiveReverseAcceleration = ReverseAcceleration * CurrentMovementScales.X;
	const float EffectiveMaxForwardSpeed = MaxForwardSpeed * CurrentMovementScales.Y;
	const float EffectiveMaxReverseSpeed = MaxReverseSpeed * CurrentMovementScales.Y;

	float RequestedAcceleration = 0.0f;
	if (bBrakeHeld)
	{
		if (CurrentForwardSpeed > ReverseEngageSpeed)
		{
			RequestedAcceleration = -BrakeDeceleration * InputCommand.Brake;
		}
		else if (bMovementDriveAllowed && CurrentForwardSpeed > -EffectiveMaxReverseSpeed)
		{
			RequestedAcceleration = -EffectiveReverseAcceleration * InputCommand.Brake;
		}
	}
	else if (bThrottleHeld)
	{
		if (CurrentForwardSpeed < -ReverseEngageSpeed)
		{
			RequestedAcceleration = BrakeDeceleration * InputCommand.Throttle;
		}
		else if (bMovementDriveAllowed && CurrentForwardSpeed < EffectiveMaxForwardSpeed)
		{
			RequestedAcceleration = EffectiveForwardAcceleration * InputCommand.Throttle;
		}
	}

	FVector DriveAcceleration = ForwardDirection * RequestedAcceleration;
	if (bThrottleHeld && CurrentForwardSpeed >= -ReverseEngageSpeed && RequestedAcceleration > 0.0f)
	{
		DriveAcceleration = VehicleWallEscape::ConstrainThrottle(DriveAcceleration,
			WallEscapeNormals, WallEscapeIntoWallThrottleScale, WallEscapeBlend);
	}
	// Base drag/braking remain independent. An unknown replay frame cannot
	// impose a guessed speed cap; known zero scales still use normal corrections.
	float ResistanceAcceleration = -CurrentForwardSpeed * CoastingDragRate;

	if (bMovementDriveAllowed && CurrentForwardSpeed > EffectiveMaxForwardSpeed)
	{
		ResistanceAcceleration -= (CurrentForwardSpeed - EffectiveMaxForwardSpeed) * OverspeedCorrectionRate;
	}
	else if (bMovementDriveAllowed && CurrentForwardSpeed < -EffectiveMaxReverseSpeed)
	{
		ResistanceAcceleration -= (CurrentForwardSpeed + EffectiveMaxReverseSpeed) * OverspeedCorrectionRate;
	}

	FPhysicsInterface::AddForce_AssumesLocked(Body.GetPhysicsActor(),
		DriveAcceleration + ForwardDirection * ResistanceAcceleration, false, true, true);
}

void UArcadeVehicleMovementComponent::ApplyLateralGrip(
	FBodyInstance& Body,
	const FVector& RightDirection,
	float CurrentLateralSpeed)
{
	const float GripRate = (InputCommand.bHandbrake ? HandbrakeGripRate : LateralGripRate) * CurrentMovementScales.Z;
	const float GripLimit = MaxLateralGripAcceleration * CurrentMovementScales.Z;
	const float GripAcceleration = FMath::Clamp(
		-CurrentLateralSpeed * GripRate,
		-GripLimit,
		GripLimit);

	FPhysicsInterface::AddForce_AssumesLocked(Body.GetPhysicsActor(),
		RightDirection * GripAcceleration, false, true, true);
}

void UArcadeVehicleMovementComponent::ApplySteering(
	FBodyInstance& Body,
	const FVector& GroundNormal,
	float CurrentForwardSpeed,
	float CurrentYawRate,
	float DeltaTime)
{
	const float AbsoluteSpeed = FMath::Abs(CurrentForwardSpeed);
	float SteeringAuthority = FMath::Clamp(AbsoluteSpeed / FMath::Max(FullSteeringSpeed, 1.0f), 0.0f, 1.0f);

	// Apply the floor before high-speed fading so fast drifting keeps its existing behavior.
	if (InputCommand.bHandbrake && FMath::Abs(InputCommand.Steering) > KINDA_SMALL_NUMBER)
	{
		SteeringAuthority = FMath::Max(
			SteeringAuthority,
			FMath::Clamp(HandbrakeMinimumSteeringAuthority, 0.0f, 1.0f));
	}

	if (AbsoluteSpeed > HighSpeedSteeringStart && MaxForwardSpeed > HighSpeedSteeringStart)
	{
		const float HighSpeedAlpha = FMath::Clamp(
			(AbsoluteSpeed - HighSpeedSteeringStart) / (MaxForwardSpeed - HighSpeedSteeringStart),
			0.0f,
			1.0f);
		SteeringAuthority *= FMath::Lerp(1.0f, HighSpeedSteeringScale, HighSpeedAlpha);
	}

	// A small wall rebound should not invert a stationary handbrake pivot.
	const float ReverseSteeringThreshold = InputCommand.bHandbrake
		? -FMath::Max(0.0f, HandbrakeReverseSteeringSpeed)
		: 0.0f;
	const float TravelDirection = CurrentForwardSpeed < ReverseSteeringThreshold ? -1.0f : 1.0f;
	const float SteeringMultiplier = InputCommand.bHandbrake ? HandbrakeSteeringMultiplier : 1.0f;
	const float DampingRate = InputCommand.bHandbrake ? HandbrakeYawDampingRate : YawDampingRate;

	const float SteeringAcceleration =
		InputCommand.Steering
		* TravelDirection
		* SteeringAngularAcceleration
		* SteeringAuthority
		* SteeringMultiplier;
	const float DampingAcceleration = -CurrentYawRate * DampingRate;
	const float EscapeAcceleration = VehicleWallEscape::YawAcceleration(
		InputCommand.Steering * FMath::Max(0.0f, WallEscapeTargetYawRate), CurrentYawRate,
		WallEscapeYawResponseRate, WallEscapeMaxYawAcceleration, DeltaTime);
	const float YawAcceleration = FMath::Lerp(
		SteeringAcceleration + DampingAcceleration, EscapeAcceleration, WallEscapeBlend);

	FPhysicsInterface::AddTorque_AssumesLocked(Body.GetPhysicsActor(),
		GroundNormal * YawAcceleration, false, true, true);
}

bool UArcadeVehicleMovementComponent::ApplyConfiguration(const FArcadeVehicleConfig& Config)
{
	FString Error;
	if (HasBegunPlay() || !Config.Validate(Error)) { return false; }
	ForwardAcceleration = Config.Drive.ForwardAcceleration;
	ReverseAcceleration = Config.Drive.ReverseAcceleration;
	BrakeDeceleration = Config.Drive.BrakeDeceleration;
	MaxForwardSpeed = Config.Drive.MaxForwardSpeed;
	MaxReverseSpeed = Config.Drive.MaxReverseSpeed;
	ReverseEngageSpeed = Config.Drive.ReverseEngageSpeed;
	CoastingDragRate = Config.Drive.CoastingDragRate;
	OverspeedCorrectionRate = Config.Drive.OverspeedCorrectionRate;
	LateralGripRate = Config.Grip.LateralGripRate;
	HandbrakeGripRate = Config.Grip.HandbrakeGripRate;
	MaxLateralGripAcceleration = Config.Grip.MaxLateralGripAcceleration;
	SteeringAngularAcceleration = Config.Steering.SteeringAngularAcceleration;
	FullSteeringSpeed = Config.Steering.FullSteeringSpeed;
	HighSpeedSteeringStart = Config.Steering.HighSpeedSteeringStart;
	HighSpeedSteeringScale = Config.Steering.HighSpeedSteeringScale;
	HandbrakeSteeringMultiplier = Config.Steering.HandbrakeSteeringMultiplier;
	HandbrakeMinimumSteeringAuthority = Config.Steering.HandbrakeMinimumSteeringAuthority;
	HandbrakeReverseSteeringSpeed = Config.Steering.HandbrakeReverseSteeringSpeed;
	YawDampingRate = Config.Steering.YawDampingRate;
	HandbrakeYawDampingRate = Config.Steering.HandbrakeYawDampingRate;
	bEnableWallEscape = Config.WallEscape.bEnableWallEscape;
	WallEscapeEnterSpeed = Config.WallEscape.WallEscapeEnterSpeed;
	WallEscapeExitSpeed = Config.WallEscape.WallEscapeExitSpeed;
	WallEscapeProbeDistance = Config.WallEscape.WallEscapeProbeDistance;
	WallEscapeMinimumSteering = Config.WallEscape.WallEscapeMinimumSteering;
	WallEscapeIntoWallThrottleScale = Config.WallEscape.WallEscapeIntoWallThrottleScale;
	WallEscapeTargetYawRate = Config.WallEscape.WallEscapeTargetYawRate;
	WallEscapeYawResponseRate = Config.WallEscape.WallEscapeYawResponseRate;
	WallEscapeMaxYawAcceleration = Config.WallEscape.WallEscapeMaxYawAcceleration;
	WallEscapeBlendInTime = Config.WallEscape.WallEscapeBlendInTime;
	WallEscapeBlendOutTime = Config.WallEscape.WallEscapeBlendOutTime;
	bDrawWallEscapeDebug = Config.Debug.bDrawWallEscapeDebug;
	GroundTraceExtraDistance = Config.Ground.GroundTraceExtraDistance;
	GroundTraceChannel = Config.Ground.GroundTraceChannel;
	bDrawGroundDebug = Config.Debug.bDrawGroundDebug;
	return true;
}

#if WITH_EDITOR
bool UArcadeVehicleMovementComponent::CanEditChange(const FProperty* Property) const
{
	if (Property && Property->GetOwnerClass() == StaticClass() && VehicleConfiguration::UsesDefinition(GetOwner())) { return false; }
	return Super::CanEditChange(Property);
}
#endif
