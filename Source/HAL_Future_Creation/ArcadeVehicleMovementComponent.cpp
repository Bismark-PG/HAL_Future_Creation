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
#include "PhysicsEngine/PhysicsSettings.h"
#include "GameFramework/Pawn.h"
#include "VehicleWallEscapeMath.h"
#include "Chaos/SimCallbackObject.h"
#include "Chaos/Rotation.h"
#include "Framework/Threading.h"
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
	TEXT("Log History impacts and non-History GT queue/PT consumption plus bounded Internal samples. 0=off, 1=on."));
static TAutoConsoleVariable<int32> CVarHALSideShiftArcLog(TEXT("hal.SideShiftArcLog"),0,
	TEXT("Log captured arc basis, remaining span and initial/gameplay momentum. 0=off, 1=on."));
#endif

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarHALHookPhysicsLog(TEXT("hal.HookPhysicsLog"),0,
	TEXT("Hook Internal-body hold/release frame, effective velocity and sleeping state. 0=off,1=on."));
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
	{ FScopeLock Lock(&PendingPhysicsDataLock); PendingImpacts.Reset(); bHasPendingImpacts.Store(false); }
#if !UE_BUILD_SHIPPING
	{ FScopeLock Lock(&PendingPhysicsDataLock); ImpactDiagnosticTraces.Reset(); bHasImpactDiagnosticTraces.Store(false); }
#endif
	Super::EndPlay(EndPlayReason);
}

void UArcadeVehicleMovementComponent::OnUnregister()
{
	{ FScopeLock Lock(&PendingPhysicsDataLock); PendingImpacts.Reset(); bHasPendingImpacts.Store(false); }
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
	{ FScopeLock Lock(&PendingPhysicsDataLock); PendingImpacts.Reset(); bHasPendingImpacts.Store(false); }
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
	if (!bIsResimming && !bUseNetworkPhysicsHistory.Load()) { ConsumeNonHistoryImpacts_Internal(CurrentPhysicsFrame); }
#if !UE_BUILD_SHIPPING
	SampleImpactDiagnostics_Internal(CurrentPhysicsFrame);
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
	if (CurrentMovement.bHandbrakeBlocked) { InputCommand.bHandbrake = false; }

	if (!UpdatedPrimitive || !UpdatedPrimitive->IsSimulatingPhysics() || DeltaTime <= 0.0f)
	{
		bGrounded = false;
		ForwardSpeed = 0.0f;
		{ FScopeLock Lock(&PendingPhysicsDataLock); PublishedPresentationInput = FHALVehiclePresentationInput(); PublishedAcceptedDrivingInput.Reset(); }
		ResetWallEscape();
		return;
	}
	FBodyInstance* Body = UpdatedPrimitive->GetBodyInstance();
	FBodyInstanceAsyncPhysicsTickHandle PhysicsHandle = Body
		? Body->GetBodyInstanceAsyncPhysicsTickHandle() : FBodyInstanceAsyncPhysicsTickHandle();
	if (!PhysicsHandle.IsValid())
	{
		{ FScopeLock Lock(&PendingPhysicsDataLock); PublishedPresentationInput = FHALVehiclePresentationInput(); PublishedAcceptedDrivingInput.Reset(); }
		return;
	}
	const auto& Overdrive = CurrentMovement.Overdrive;
	const bool bCharging = Overdrive.IsDriving(CurrentMovementServerFrame);
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
	// The historical hook command owns only bounded planar forces and a one-frame release.
	// Contacts, gravity and angular momentum remain Chaos-owned, including resimulation.
	const auto& HookMotion=CurrentMovement.Hook;
	if(HookMotion.IsSwinging(CurrentMovementServerFrame))
	{
		const FVector Offset=PhysicsHandle->R().RotateVector(PhysicsHandle->CenterOfMass());
		const FVector Angular=PhysicsHandle->W()+PhysicsHandle->AngularImpulseVelocity();
		// X is the particle/root origin; V is the COM velocity.
		const FVector RootP=PhysicsHandle->X();
		const FVector RootV=PhysicsHandle->V()+PhysicsHandle->LinearImpulseVelocity()-FVector::CrossProduct(Angular,Offset);
		const FVector Delta=HookMotion.SwingDelta(RootP,RootV,CurrentMovementServerFrame,DeltaTime);
		if(!Delta.IsNearlyZero())
		{
			if(PhysicsHandle->ObjectState()==Chaos::EObjectStateType::Sleeping) { PhysicsHandle->SetObjectState(Chaos::EObjectStateType::Dynamic,true); }
			FPhysicsInterface::AddVelocity_AssumesLocked(Body->GetPhysicsActor(),Delta,true);
		}
#if !UE_BUILD_SHIPPING
		if(CVarHALHookPhysicsLog.GetValueOnAnyThread()&&(CurrentMovementServerFrame==HookMotion.StartFrame||CurrentMovementServerFrame==HookMotion.EndFrame-1))
		{ UE_LOG(LogTemp,Log,TEXT("Hook physics swing Player=%d Frame=%d Start=%d End=%d Position=%s Velocity=%s Delta=%s"),MovementPlayerId.Load(),CurrentMovementServerFrame,HookMotion.StartFrame,HookMotion.EndFrame,*RootP.ToString(),*RootV.ToString(),*Delta.ToString()); }
#endif
		InputCommand.Reset();
	}
	const FVector HookRelease=HookMotion.ReleaseDelta(PhysicsHandle->V()+PhysicsHandle->LinearImpulseVelocity(),CurrentMovementServerFrame);
	if(!HookRelease.IsNearlyZero())
	{
#if !UE_BUILD_SHIPPING
		if(CVarHALHookPhysicsLog.GetValueOnAnyThread())
		{ UE_LOG(LogTemp,Log,TEXT("Hook physics release Player=%d Activation=%s Frame=%d Start=%d Internal=%d Sleeping=%d Before=%s Delta=%s Target=%s"),
			MovementPlayerId.Load(),*HookMotion.Activation.ToString(),CurrentMovementServerFrame,HookMotion.StartFrame,Chaos::IsInPhysicsThreadContext(),
			PhysicsHandle->ObjectState()==Chaos::EObjectStateType::Sleeping,*(PhysicsHandle->V()+PhysicsHandle->LinearImpulseVelocity()).ToString(),*HookRelease.ToString(),*HookMotion.ThrowVelocity.ToString()); }
#endif
		if(PhysicsHandle->ObjectState()==Chaos::EObjectStateType::Sleeping) { PhysicsHandle->SetObjectState(Chaos::EObjectStateType::Dynamic,true); }
		FPhysicsInterface::AddVelocity_AssumesLocked(Body->GetPhysicsActor(),HookRelease,true);
	}
	if(HookMotion.IsHolding(CurrentMovementServerFrame))
	{
		const FVector Offset=PhysicsHandle->R().RotateVector(PhysicsHandle->CenterOfMass());
		const FVector RootV=PhysicsHandle->V()+PhysicsHandle->LinearImpulseVelocity()-FVector::CrossProduct(PhysicsHandle->W()+PhysicsHandle->AngularImpulseVelocity(),Offset);
		const FVector A=HookMotion.Acceleration(PhysicsHandle->X(),RootV,CurrentMovementServerFrame);
		// Explicitly select the Internal particle; the default External access asserts
		// on the PT. Read its mass here rather than querying an External body.
		FPhysicsInterface::AddForce_AssumesLocked(Body->GetPhysicsActor(),A*PhysicsHandle->M(),false,false,true);
		InputCommand.Reset();
	}
	GroundProbeRotation = BodyTransform.GetRotation();
	{
		FScopeLock Lock(&PendingPhysicsDataLock);
		if (bPendingRecovery) { RecoveryPhysics = PendingRecovery; bPendingRecovery = false; }
		if (bPendingBounceBack)
		{
			BounceBackPhysics=PendingBounceBack;
			if (bPendingBounceHasEntry) { BounceBackPhysics.Distance=FVehicleBounceBackState::DistanceFromEntry(
				BounceBackPhysics.Distance,BodyTransform.GetLocation(),PendingBounceEntry,BounceBackPhysics.Direction); }
			bPendingBounceBack=false;
		}
	}

	if (bCharging && Overdrive.bStunned)
	{
		// Resolve pending actions first so a same-frame electric-wall bounce cannot undo the stop.
		if (CurrentMovementServerFrame == Overdrive.StunFrame)
		{
			const FVector V = PhysicsHandle->V();
			FPhysicsInterface::AddVelocity_AssumesLocked(Body->GetPhysicsActor(), FVector(-V.X, -V.Y, 0), true);
		}
		BounceBackPhysics.bActive = false; // Retain the authored sequence for prediction acknowledgement.
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
	if (bCharging && !CurrentMovement.bControlsBlocked && bMovementDriveAllowed && !RecoveryPhysics.bActive)
	{
		InputCommand.Throttle = InputCommand.Brake > KINDA_SMALL_NUMBER ? 0.f : 1.f;
		InputCommand.bHandbrake = false;
	}
	const FVector ForwardDirection = FVector::VectorPlaneProject(
		BodyTransform.GetRotation().GetForwardVector(), GroundNormal).GetSafeNormal();
	const FVector RightDirection = FVector::CrossProduct(GroundNormal, ForwardDirection).GetSafeNormal();

	ForwardSpeed = FVector::DotProduct(Velocity, ForwardDirection);
	// The pump keeps running when driver input is blocked. Apply force at the COM, including during replay.
	const auto& WaterJet = CurrentMovement.WaterJet;
	if (WaterJet.IsActive(CurrentMovementServerFrame) && GroundProbeRotation.GetUpVector().Z > .35f && !RecoveryPhysics.bActive)
	{
		const FVector Heading = GroundProbeRotation.GetForwardVector().GetSafeNormal2D();
		const float Recoil = WaterJet.RecoilAt(FVector::DotProduct(Velocity, Heading), DeltaTime);
		if (Recoil > 0)
		{
			if (PhysicsHandle->ObjectState() == Chaos::EObjectStateType::Sleeping) { PhysicsHandle->SetObjectState(Chaos::EObjectStateType::Dynamic, true); }
			FPhysicsInterface::AddForce_AssumesLocked(Body->GetPhysicsActor(), -Heading * Recoil, false, true, true);
		}
	}
	{
		FScopeLock Lock(&PendingPhysicsDataLock);
		PublishedPresentationInput.Steering = FMath::IsFinite(InputCommand.Steering)
			&& bPoseDriveAllowed ? FMath::Clamp(InputCommand.Steering, -1.f, 1.f) : 0;
		PublishedPresentationInput.PhysicsFrame = StepInput.PhysicsFrame;
		PublishedPresentationInput.bGrounded = bGrounded;
		PublishedPresentationInput.bValid = true;
		PublishedAcceptedDrivingInput = bPoseDriveAllowed && bMovementDriveAllowed && !CurrentMovement.bControlsBlocked
			? InputCommand : FVehicleInputCmd();
		PublishedPresentationInput.TireSkidInputs = bPoseDriveAllowed && bMovementDriveAllowed && !CurrentMovement.bControlsBlocked
			? HALVehiclePresentation::TireSkidInputs(InputCommand.Brake, InputCommand.Throttle, InputCommand.bHandbrake) : 0;
		PublishedStability.PhysicsFrame = StepInput.PhysicsFrame;
		PublishedStability.bValid = true;
		PublishedStability.bGrounded = bGrounded;
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
		PublishedContact.bValid=true;
		PublishedContact.Position=BodyTransform.GetLocation();
		PublishedContact.Rotation=BodyTransform.GetRotation();
		PublishedContact.Velocity=PhysicsHandle->V();
		PublishedContact.AngularVelocity=PhysicsHandle->W();
		PublishedStability.PoseStableSeconds = PoseStableSeconds;
	}

	if (bBouncing || !bGrounded || !bPoseDriveAllowed || CurrentMovement.bControlsBlocked || ForwardDirection.IsNearlyZero() || RightDirection.IsNearlyZero())
	{
		SideShiftTurn.Cancel(CurrentMovement.SideShift.Activation);
		ResetWallEscape();
		return;
	}

	const float LateralSpeed = FVector::DotProduct(Velocity, RightDirection);
	UpdateWallEscape(BodyTransform, GroundNormal, ForwardDirection, Velocity, DeltaTime);

	const auto& Shift=CurrentMovement.SideShift;
	const FVector EffectiveV=PhysicsHandle->V()+PhysicsHandle->LinearImpulseVelocity();
	const FVector COMOffset=GroundProbeRotation.RotateVector(PhysicsHandle->CenterOfMass());
	const FVector EffectiveW=PhysicsHandle->W()+PhysicsHandle->AngularImpulseVelocity();
#if !UE_BUILD_SHIPPING
	const bool HadArc=SideShiftTurn.Activation==Shift.Activation&&SideShiftTurn.ArcFrames>0;
#endif
	bSideShiftTurnSteering=SideShiftTurn.Update(Shift,CurrentMovementServerFrame,GroundProbeRotation.Rotator().Yaw,FVector::DotProduct(PhysicsHandle->W(),GroundNormal),
		InputCommand.Steering,InputCommand.bHandbrake,DeltaTime,SideShiftTurnAcceleration,BodyTransform.GetLocation(),RecoilDeltaVelocity);
	const bool bArc=SideShiftTurn.IsArcDriving(Shift,CurrentMovementServerFrame);
#if !UE_BUILD_SHIPPING
	if(!HadArc&&bArc&&CVarHALSideShiftArcLog.GetValueOnAnyThread())
	{ UE_LOG(LogTemp,Log,TEXT("Arc capture Player=%d Frame=%d Start=%d Steps=%d Span=%.3f Base=%s Initial=%s Pending=%s"),
		MovementPlayerId.Load(),CurrentMovementServerFrame,Shift.StartFrame,SideShiftTurn.ArcFrames,SideShiftTurn.ArcSpan,*SideShiftTurn.ArcBaseVelocity.ToString(),*Shift.InitialVelocity.ToString(),*PhysicsHandle->LinearImpulseVelocity().ToString()); }
#endif
	const bool bShifting=Shift.IsSliding(CurrentMovementServerFrame)&&!SideShiftTurn.bConsumed;
	FVector ScriptVelocity=FVector::ZeroVector;
	if(bArc)
	{
		ScriptVelocity=SideShiftTurn.ArcStepVelocity(Shift,CurrentMovementServerFrame,DeltaTime)*CurrentMovementScales.Y;
		// Chaos V is a COM velocity, but the authored path describes the vehicle's
		// root position. Compensate the rotating COM offset using this step's yaw
		// torque; asymmetric collision hulls must not shrink one side of the dodge.
		const FVector NextW=EffectiveW+GroundNormal*SideShiftTurnAcceleration*DeltaTime;
		const auto NextRotation=Chaos::FRotation3::IntegrateRotationWithAngularVelocity(PhysicsHandle->R(),NextW,DeltaTime);
		const FVector COMStepVelocity=(NextRotation.RotateVector(PhysicsHandle->CenterOfMass())-COMOffset)/DeltaTime;
		const FVector Target=SideShiftTurn.ArcBackgroundVelocity(Shift,CurrentMovementServerFrame)+ScriptVelocity+COMStepVelocity;
		const FVector Delta=FVector(Target.X-EffectiveV.X,Target.Y-EffectiveV.Y,0).GetClampedToMaxSize(Shift.MaximumAcceleration*DeltaTime*CurrentMovementScales.X);
		FPhysicsInterface::AddVelocity_AssumesLocked(Body->GetPhysicsActor(),Delta,true);
	}
	else if(bShifting)
	{
		const float ShiftSpeed=Shift.StepSpeed(CurrentMovementServerFrame,DeltaTime)*CurrentMovementScales.Y;
		ScriptVelocity=Shift.Direction*ShiftSpeed;
		const float Target=Shift.InitialLateralSpeed+ShiftSpeed;
		const float Delta=FMath::Clamp(Target-float(FVector::DotProduct(EffectiveV,Shift.Direction)),
			-Shift.MaximumAcceleration*DeltaTime*CurrentMovementScales.X,Shift.MaximumAcceleration*DeltaTime*CurrentMovementScales.X);
		// A bounded motor, not a pose/velocity teleport. Other velocity axes, gravity,
		// external impacts and Chaos contacts remain physical; walls can shorten travel.
		FPhysicsInterface::AddVelocity_AssumesLocked(Body->GetPhysicsActor(),Shift.Direction*Delta,true);
	}
	const float DriveSpeed=bArc?float(FVector::DotProduct(SideShiftTurn.ArcBackgroundVelocity(Shift,CurrentMovementServerFrame),ForwardDirection))
		:ForwardSpeed-float(FVector::DotProduct(ScriptVelocity,ForwardDirection));
	ApplyLongitudinalForces(*Body, ForwardDirection, DriveSpeed, DeltaTime);
	if(!bShifting&&!bArc) { ApplyLateralGrip(*Body, RightDirection, LateralSpeed); }
	const float AreaSway = bMovementDriveAllowed && !RecoveryPhysics.bActive
		? CurrentMovement.EvaluateSway(CurrentMovementServerFrame, FMath::Sqrt(FMath::Square(ForwardSpeed) + FMath::Square(LateralSpeed))) : 0;
	ApplySteering(*Body, GroundNormal, ForwardSpeed,
		FVector::DotProduct(PhysicsHandle->W(), GroundNormal), DeltaTime, AreaSway);
}

void UArcadeVehicleMovementComponent::SetUpdatedPrimitive(UPrimitiveComponent* InPrimitive)
{
	if (UpdatedPrimitive != InPrimitive)
	{ FScopeLock Lock(&PendingPhysicsDataLock); PendingImpacts.Reset(); bHasPendingImpacts.Store(false); }
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
	{ FScopeLock Lock(&PendingPhysicsDataLock); PendingImpacts.Reset(); bHasPendingImpacts.Store(false); }
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
	OutState.SideShiftTurn = SideShiftTurn;
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
	if (InState.SideShiftTurn.IsValid()) { SideShiftTurn = InState.SideShiftTurn; }
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
		// Confirmed speculation must still replay before the later authority action frame.
		// Only rejected tokens are suppressed independently of rewound state.
		if (Bounce->PredictionToken)
		{
			FScopeLock Lock(&PendingPhysicsDataLock);
			if (!FVehicleNetInputData::IsNewerSequence(Bounce->PredictionToken,RetiredBouncePredictionToken)
				|| RejectedBouncePredictionTokens.Contains(Bounce->PredictionToken)) { return; }
		}
		if (bShouldRunPhysics.Load() && Bounce->Value.IsValid() && Bounce->Value.bActive
			&& FVehicleNetInputData::IsNewerSequence(Bounce->Value.Sequence,BounceBackPhysics.Sequence))
		{
			BounceBackPhysics = Bounce->Value;
			if (Bounce->bHasEntryOrigin && !Bounce->EntryOrigin.ContainsNaN() && UpdatedPrimitive)
			{
				const auto* Body=UpdatedPrimitive->GetBodyInstance();
				auto Handle=Body ? Body->GetBodyInstanceAsyncPhysicsTickHandle() : FBodyInstanceAsyncPhysicsTickHandle();
				if (Handle.IsValid())
				{
					BounceBackPhysics.Distance=FVehicleBounceBackState::DistanceFromEntry(
						Bounce->Value.Distance,Handle->X(),Bounce->EntryOrigin,Bounce->Value.Direction);
				}
			}
		}
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
			SideShiftTurn.TrackGameplayImpulse(CurrentMovement.SideShift,CurrentMovementServerFrame,DeltaV);
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
		SideShiftTurn.TrackGameplayImpulse(CurrentMovement.SideShift,CurrentMovementServerFrame,Recoil->DeltaVelocity);
	}
}

FVehicleInputCmd UArcadeVehicleMovementComponent::GetAcceptedDrivingInput() const
{
	FScopeLock Lock(&PendingPhysicsDataLock);
	return bShouldRunPhysics.Load() && !bHistoryInputBlocked.Load() ? PublishedAcceptedDrivingInput : FVehicleInputCmd();
}

FVehicleContactObservation UArcadeVehicleMovementComponent::GetContactObservation() const
{
	FScopeLock Lock(&PendingPhysicsDataLock);
	return PublishedContact;
}

bool UArcadeVehicleMovementComponent::IsBounceBackActive() const
{
	FScopeLock Lock(&PendingPhysicsDataLock);
	return PublishedBounceBack.bActive;
}

void UArcadeVehicleMovementComponent::ObserveBounceAuthority(int32 LocalFrame, uint32 Sequence)
{
	FScopeLock Lock(&PendingPhysicsDataLock);
	if (LocalFrame <= BounceAuthorityFrame.Load()) { return; }
	BounceAuthoritySequence.Store(Sequence);
	BounceAuthorityFrame.Store(LocalFrame);
	if (PendingBouncePredictionToken && LocalFrame >= PendingBouncePredictionFrame.Load())
	{
		const bool Confirmed = Sequence == PendingBouncePredictionSequence
			|| FVehicleNetInputData::IsNewerSequence(Sequence,PendingBouncePredictionSequence);
		// An authority checkpoint beyond the whole speculative pulse without its sequence
		// also resolves it. It must remain rejected if an older frame is replayed later.
		if (Confirmed || LocalFrame >= PendingBouncePredictionExpiry)
		{
			if (!Confirmed) { RejectedBouncePredictionTokens.Add(PendingBouncePredictionToken); }
			ResolvedBouncePredictionToken.Store(PendingBouncePredictionToken);
		}
	}
}

bool UArcadeVehicleMovementComponent::PredictBounceBack(const FVector& Direction, float Distance, float Duration, uint32 SourceId, const FVector* EntryOrigin)
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	auto* History = GetOwner()->FindComponentByClass<UNetworkPhysicsComponent>();
	if (GetNetMode()!=NM_Client || !Pawn || !Pawn->IsLocallyControlled() || !SourceId
		|| !bUseNetworkPhysicsHistory.Load() || !History || !History->IsNetworkPhysicsTickOffsetAssigned()
		|| !UpdatedPrimitive || !UpdatedPrimitive->IsSimulatingPhysics() || GetLastPhysicsFrame()<0) { return false; }
	FVehicleBounceBackAction Action;
	if (EntryOrigin && EntryOrigin->ContainsNaN()) { return false; }
	Action.bHasEntryOrigin=EntryOrigin!=nullptr;
	if (EntryOrigin) { Action.EntryOrigin=*EntryOrigin; }
	int32 Frame;
	{
		FScopeLock Lock(&PendingPhysicsDataLock);
		if (bMovementResetPending || FVehicleNetInputData::IsNewerSequence(
			PendingBouncePredictionToken,ResolvedBouncePredictionToken.Load())) { return false; }
		Action.Value.Direction=Direction; Action.Value.Distance=Distance; Action.Value.Duration=Duration;
		Action.Value.Sequence=BounceAuthoritySequence.Load()+1;
		if (!Action.Value.Sequence) { Action.Value.Sequence=1; }
		Action.Value.bActive=true;
		if (!Action.Value.IsValid()) { return false; }
		Action.PredictionToken=++NextBouncePredictionToken;
		if (!Action.PredictionToken) { Action.PredictionToken=++NextBouncePredictionToken; }
		Frame=GetLastPhysicsFrame()+1;
		PendingBouncePredictionFrame.Store(Frame);
		PendingBouncePredictionSequence=Action.Value.Sequence;
		const int32 KnownFrame=BounceAuthorityFrame.Load();
		const int32 UnknownLead=KnownFrame>=0 ? FMath::Max(0,Frame-KnownFrame) : 0;
		PendingBouncePredictionExpiry=Frame+UnknownLead
			+FMath::CeilToInt(Duration/GetDefault<UPhysicsSettings>()->AsyncFixedTimeStepSize);
		PendingBouncePredictionToken=Action.PredictionToken;
	}
	History->EnqueueScheduledActionAtFrame_External(Action,SourceId,Frame,false);
	++PredictedBounceCount;
	return true;
}

bool UArcadeVehicleMovementComponent::QueueBounceBack(const FVector& Direction, float Distance, float Duration, uint32 SourceId, const FVector* EntryOrigin)
{
	if (!GetOwner()->HasAuthority() || !UpdatedPrimitive || !UpdatedPrimitive->IsSimulatingPhysics()) { return false; }
	FVehicleBounceBackAction Action;
	if (EntryOrigin && EntryOrigin->ContainsNaN()) { return false; }
	Action.bHasEntryOrigin=EntryOrigin!=nullptr;
	if (EntryOrigin) { Action.EntryOrigin=*EntryOrigin; }
	Action.Value.Direction=Direction; Action.Value.Distance=Distance; Action.Value.Duration=Duration;
	Action.Value.Sequence=++NextBounceBackSequence;
	if (!Action.Value.Sequence) { Action.Value.Sequence=++NextBounceBackSequence; }
	Action.Value.bActive=true;
	if (!Action.Value.IsValid()) { return false; }
	if (bUseNetworkPhysicsHistory.Load())
	{
		auto* History=GetOwner()->FindComponentByClass<UNetworkPhysicsComponent>();
		if (!History) { return false; }
		History->EnqueueImmediateAction_External(Action,SourceId,true);
	}
	else
	{
		FScopeLock Lock(&PendingPhysicsDataLock);
		PendingBounceBack=Action.Value;
		bPendingBounceHasEntry=EntryOrigin!=nullptr;
		if (EntryOrigin) { PendingBounceEntry=*EntryOrigin; }
		bPendingBounceBack=true;
	}
	return true;
}

bool UArcadeVehicleMovementComponent::QueueImpact(FVehicleImpactAction& Action, const FGuid& AttackId, uint32 PulseSequence)
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
		if (!IsRegistered() || !bShouldRunPhysics.Load()) { return false; }
#if !UE_BUILD_SHIPPING
		const bool bTrace = CVarHALImpactPhysicsLog.GetValueOnGameThread() > 0;
		FVector BeforePosition = FVector::ZeroVector, BeforeV = FVector::ZeroVector, BeforeW = FVector::ZeroVector;
		int32 BeforeFrame = INDEX_NONE, AwakeBefore = -1;
		if (bTrace)
		{
			const auto* Body = UpdatedPrimitive->GetBodyInstance();
			BeforePosition = Body ? Body->GetUnrealWorldTransform().GetLocation() : UpdatedPrimitive->GetComponentLocation();
			BeforeV = UpdatedPrimitive->GetPhysicsLinearVelocity();
			BeforeW = UpdatedPrimitive->GetPhysicsAngularVelocityInRadians();
			BeforeFrame = GetLastPhysicsFrame(); AwakeBefore = Body ? int32(Body->IsInstanceAwake()) : -1;
		}
#endif
		{
			FScopeLock Lock(&PendingPhysicsDataLock);
			if (PendingImpacts.Num() >= MaximumPendingImpacts)
			{
#if !UE_BUILD_SHIPPING
				if (bTrace) { UE_LOG(LogTemp, Log, TEXT("Vehicle impact trace: Phase=RejectedGT Actor=%s Attack=%s Pulse=%u Event=%u Reason=QueueFull128"),
					*GetNameSafe(GetOwner()), *AttackId.ToString(), PulseSequence, Action.EventSequence); }
#endif
				return false;
			}
			auto& Pending = PendingImpacts.AddDefaulted_GetRef();
			Pending.Action = Action;
#if !UE_BUILD_SHIPPING
			Pending.AttackId = AttackId; Pending.PulseSequence = PulseSequence; Pending.SubmitFrame = GetLastPhysicsFrame();
#endif
			bHasPendingImpacts.Store(true);
		}
#if !UE_BUILD_SHIPPING
		if (bTrace)
		{
			// GT only queues the rule payload. The PT resolves minimum ascent and rotation
			// against the actual body, including earlier impulses waiting in that same step.
			FImpactDiagnosticTrace Trace; Trace.AttackId = AttackId; Trace.PulseSequence = PulseSequence;
			Trace.EventSequence = Action.EventSequence; Trace.OwnerName = GetNameSafe(GetOwner());
			Trace.SubmitFrame = BeforeFrame; Trace.SubmitPosition = BeforePosition;
			{
				FScopeLock Lock(&PendingPhysicsDataLock);
				if (ImpactDiagnosticTraces.Num() >= 8)
				{
					const auto& Old = ImpactDiagnosticTraces[0];
					UE_LOG(LogTemp, Log, TEXT("Vehicle impact trace: Phase=Evicted Actor=%s Attack=%s Pulse=%u Event=%u Reason=Budget8"),
						*Old.OwnerName, *Old.AttackId.ToString(), Old.PulseSequence, Old.EventSequence);
					ImpactDiagnosticTraces.RemoveAt(0);
				}
				ImpactDiagnosticTraces.Add(Trace); bHasImpactDiagnosticTraces.Store(true);
			}
			const auto* Body = UpdatedPrimitive->GetBodyInstance();
			UE_LOG(LogTemp, Log, TEXT("Vehicle impact trace: Phase=SubmitGT Actor=%s Attack=%s Pulse=%u Event=%u Mode=QueuedPT FrameBefore=%d FrameAfter=%d PTEnabled=%d Mass=%.3f AwakeBefore=%d AwakeAfter=%d PosBefore=%s BeforeV=%s BeforeW=%s PreviewDV=%s TargetUp=%.3f SetW=%d PreviewW=%s ImmediateV=%s ImmediateW=%s"),
				*Trace.OwnerName, *AttackId.ToString(), PulseSequence, Action.EventSequence, BeforeFrame, GetLastPhysicsFrame(), int32(bShouldRunPhysics.Load()), UpdatedPrimitive->GetMass(),
				AwakeBefore, Body ? int32(Body->IsInstanceAwake()) : -1, *BeforePosition.ToString(), *BeforeV.ToString(), *BeforeW.ToString(),
				*Action.ResolveLinearDelta(BeforeV).ToString(), Action.bMinimumUpwardSpeed ? Action.MinimumUpwardSpeed : 0.f, int32(Action.bSetPlanarAngularVelocity), *Action.ResolveAngularVelocity(BeforeW).ToString(),
				*UpdatedPrimitive->GetPhysicsLinearVelocity().ToString(), *UpdatedPrimitive->GetPhysicsAngularVelocityInRadians().ToString());
		}
#endif
	}
	return true;
}

void UArcadeVehicleMovementComponent::ConsumeNonHistoryImpacts_Internal(int32 PhysicsFrame)
{
	if (!bHasPendingImpacts.Load()) { return; }
	FBodyInstance* Body = UpdatedPrimitive ? UpdatedPrimitive->GetBodyInstance() : nullptr;
	auto Handle = Body ? Body->GetBodyInstanceAsyncPhysicsTickHandle() : FBodyInstanceAsyncPhysicsTickHandle();
	if (!Handle.IsValid()) { return; } // Retain until the body is ready; teardown/mode changes retire the queue.
	TArray<FQueuedVehicleImpact> Impacts;
	{
		FScopeLock Lock(&PendingPhysicsDataLock);
		Swap(Impacts, PendingImpacts);
		bHasPendingImpacts.Store(false);
	}
	for (const auto& Pending : Impacts)
	{
		if (Handle->ObjectState() != Chaos::EObjectStateType::Sleeping
			&& Handle->ObjectState() != Chaos::EObjectStateType::Dynamic) { continue; }
		// Multiple contacts may share this step. Pending impulse velocity is part of
		// the effective state: minimum ascent is a floor, and the last planar W wins.
		const FVector BeforeV = Handle->V() + Handle->LinearImpulseVelocity();
		const FVector BeforeW = Handle->W() + Handle->AngularImpulseVelocity();
		const auto& Action = Pending.Action;
		const FVector DeltaV = Action.ResolveLinearDelta(BeforeV);
		const FVector DeltaW = Action.bSetPlanarAngularVelocity
			? Action.ResolveAngularVelocity(BeforeW) - BeforeW : FVector::ZeroVector;
		const bool bSleeping = Handle->ObjectState() == Chaos::EObjectStateType::Sleeping;
		const bool bNoOp = DeltaV.IsNearlyZero() && DeltaW.IsNearlyZero();
		if (!bNoOp)
		{
			// Wake and both impulses must address the same Internal body in this Solver
			// step. A successful GT wake/readback does not prove PT consumed the hit.
			if (bSleeping) { Handle->SetObjectState(Chaos::EObjectStateType::Dynamic, true); }
			if (!DeltaV.IsNearlyZero()) { FPhysicsInterface::AddVelocity_AssumesLocked(Body->GetPhysicsActor(), DeltaV, true); }
			SideShiftTurn.TrackGameplayImpulse(CurrentMovement.SideShift,CurrentMovementServerFrame,DeltaV);
			if (!DeltaW.IsNearlyZero()) { FPhysicsInterface::AddAngularVelocityInRadians_AssumesLocked(Body->GetPhysicsActor(), DeltaW, true); }
		}
#if !UE_BUILD_SHIPPING
		if (CVarHALImpactPhysicsLog.GetValueOnAnyThread() > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("Vehicle impact trace: Phase=ConsumePT Actor=%s Attack=%s Pulse=%u Event=%u Frame=%d SubmitFrame=%d PTContext=%d SleepingBefore=%d SleepingAfter=%d NoOp=%d EffectiveBeforeV=%s EffectiveBeforeW=%s AppliedDV=%s AppliedDW=%s PendingDV=%s PendingDW=%s"),
				*GetNameSafe(GetOwner()), *Pending.AttackId.ToString(), Pending.PulseSequence, Action.EventSequence, PhysicsFrame, Pending.SubmitFrame,
				int32(Chaos::IsInPhysicsThreadContext()), int32(bSleeping), int32(Handle->ObjectState() == Chaos::EObjectStateType::Sleeping), int32(bNoOp),
				*BeforeV.ToString(), *BeforeW.ToString(), *DeltaV.ToString(), *DeltaW.ToString(),
				*FVector(Handle->LinearImpulseVelocity()).ToString(), *FVector(Handle->AngularImpulseVelocity()).ToString());
		}
#endif
	}
}

#if !UE_BUILD_SHIPPING
void UArcadeVehicleMovementComponent::SampleImpactDiagnostics_Internal(int32 PhysicsFrame)
{
	if (!bHasImpactDiagnosticTraces.Load()) { return; }
	FScopeLock Lock(&PendingPhysicsDataLock);
	if (CVarHALImpactPhysicsLog.GetValueOnAnyThread() <= 0 || bUseNetworkPhysicsHistory.Load())
	{ ImpactDiagnosticTraces.Reset(); bHasImpactDiagnosticTraces.Store(false); return; }
	const auto* Body = UpdatedPrimitive ? UpdatedPrimitive->GetBodyInstance() : nullptr;
	auto Handle = Body ? Body->GetBodyInstanceAsyncPhysicsTickHandle() : FBodyInstanceAsyncPhysicsTickHandle();
	if (PhysicsFrame < 0 || !Handle.IsValid()) { return; }
	constexpr int32 SampleAges[] = {0, 1, 2, 3, 7, 15, 31, 63, 95};
	for (int32 I = ImpactDiagnosticTraces.Num()-1; I >= 0; --I)
	{
		auto& T = ImpactDiagnosticTraces[I];
		if (T.FirstFrame == INDEX_NONE) { T.FirstFrame = PhysicsFrame; T.FirstPosition = Handle->X(); }
		if (PhysicsFrame <= T.LastFrame || PhysicsFrame < T.FirstFrame) { continue; }
		T.LastFrame = PhysicsFrame;
		const int32 Age = PhysicsFrame-T.FirstFrame;
		if (T.NextSample < UE_ARRAY_COUNT(SampleAges) && Age >= SampleAges[T.NextSample])
		{
			while (T.NextSample < UE_ARRAY_COUNT(SampleAges) && Age >= SampleAges[T.NextSample]) { ++T.NextSample; }
			// This is the current pre-simulation Internal state: later frames contain previous
			// integrated motion, never a second GT read mislabeled as PostSolve/consumption.
			UE_LOG(LogTemp, Log, TEXT("Vehicle impact trace: Phase=SamplePT Actor=%s Attack=%s Pulse=%u Event=%u Frame=%d SubmitFrame=%d AgeFrames=%d PTContext=%d Sleeping=%d GroundedPrev=%d Pos=%s V=%s W=%s PendingImpulse=%s TravelFromGT=%s TravelFromFirstPT=%s"),
				*T.OwnerName, *T.AttackId.ToString(), T.PulseSequence, T.EventSequence, PhysicsFrame, T.SubmitFrame, Age, int32(Chaos::IsInPhysicsThreadContext()),
				int32(Handle->ObjectState() == Chaos::EObjectStateType::Sleeping), int32(bGrounded), *FVector(Handle->X()).ToString(), *FVector(Handle->V()).ToString(), *FVector(Handle->W()).ToString(),
				*FVector(Handle->LinearImpulse()).ToString(), *FVector(Handle->X()-T.SubmitPosition).ToString(), *FVector(Handle->X()-T.FirstPosition).ToString());
		}
		if (Age >= SampleAges[UE_ARRAY_COUNT(SampleAges)-1]) { ImpactDiagnosticTraces.RemoveAtSwap(I); }
	}
	bHasImpactDiagnosticTraces.Store(!ImpactDiagnosticTraces.IsEmpty());
}
#endif

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
	const FVector& VehicleForwardDirection,
	float VehicleForwardSpeed,
	float DeltaTime)
{
	FVector ForwardDirection = VehicleForwardDirection;
	float CurrentForwardSpeed = VehicleForwardSpeed;
	ResolveDragDriveFrame(Body, ForwardDirection, CurrentForwardSpeed);

	// Brake wins when both face buttons are held, producing deterministic input.
	const bool bBrakeHeld = InputCommand.Brake > KINDA_SMALL_NUMBER;
	const bool bThrottleHeld = InputCommand.Throttle > KINDA_SMALL_NUMBER && !bBrakeHeld;
	const float EffectiveForwardAcceleration = ForwardAcceleration * CurrentMovementScales.X;
	const float EffectiveReverseAcceleration = ReverseAcceleration * CurrentMovementScales.X;
	const bool bSpraying = CurrentMovement.WaterJet.IsActive(CurrentMovementServerFrame) && GroundProbeRotation.GetUpVector().Z > .35f && !RecoveryPhysics.bActive;
	const float EffectiveMaxForwardSpeed = MaxForwardSpeed * CurrentMovementScales.Y * (bSpraying ? CurrentMovement.WaterJet.ForwardLimitScale : 1.f);
	const float EffectiveMaxReverseSpeed = MaxReverseSpeed * CurrentMovementScales.Y;

	float RequestedAcceleration = 0.0f;
	if (bBrakeHeld)
	{
		if (CurrentForwardSpeed > ReverseEngageSpeed)
		{
			RequestedAcceleration = -BrakeDeceleration * InputCommand.Brake;
		}
		else if (bMovementDriveAllowed && !CurrentMovement.Overdrive.IsDriving(CurrentMovementServerFrame) && CurrentForwardSpeed > -EffectiveMaxReverseSpeed)
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
	const float CorrectionRate = CurrentMovement.Overdrive.IsRecovering(CurrentMovementServerFrame)
		? FMath::Min(FMath::Max(OverspeedCorrectionRate, 5.f / CurrentMovement.Overdrive.RecoverySeconds), 1.f / FMath::Max(DeltaTime, SMALL_NUMBER))
		: OverspeedCorrectionRate;

	if (bMovementDriveAllowed && CurrentForwardSpeed > EffectiveMaxForwardSpeed)
	{
		ResistanceAcceleration -= (CurrentForwardSpeed - EffectiveMaxForwardSpeed) * CorrectionRate;
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
	float DeltaTime,
	float AreaSwayAcceleration)
{
	if(bSideShiftTurnSteering)
	{
		const float Acceleration=SideShiftTurnAcceleration+AreaSwayAcceleration;
		FPhysicsInterface::AddTorque_AssumesLocked(Body.GetPhysicsActor(),GroundNormal*Acceleration,false,true,true);
		return;
	}
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
	const float OverdriveSteering = CurrentMovement.Overdrive.SteeringAt(CurrentMovementServerFrame);
	const float DampingRate = InputCommand.bHandbrake ? HandbrakeYawDampingRate : YawDampingRate;

	const float SteeringAcceleration =
		InputCommand.Steering
		* TravelDirection
		* SteeringAngularAcceleration
		* SteeringAuthority
		* SteeringMultiplier * OverdriveSteering;
	const float DampingAcceleration = -CurrentYawRate * DampingRate;
	const float EscapeAcceleration = VehicleWallEscape::YawAcceleration(
		InputCommand.Steering * FMath::Max(0.0f, WallEscapeTargetYawRate) * OverdriveSteering, CurrentYawRate,
		WallEscapeYawResponseRate, WallEscapeMaxYawAcceleration, DeltaTime);
	const float YawAcceleration = FMath::Lerp(
		SteeringAcceleration + DampingAcceleration + AreaSwayAcceleration, EscapeAcceleration, WallEscapeBlend);

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
