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

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarHALVehicleHistoryLog(
	TEXT("hal.VehicleHistoryLog"), 0,
	TEXT("Network Physics input source, at most once per 60 physics frames: 0=off, 1=on."));
#endif

UArcadeVehicleMovementComponent::UArcadeVehicleMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UArcadeVehicleMovementComponent::BeginPlay()
{
	Super::BeginPlay();

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
	Super::EndPlay(EndPlayReason);
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
	if (const UWorld* World = GetWorld())
	{
		if (const FPhysScene_Chaos* Scene = static_cast<const FPhysScene_Chaos*>(World->GetPhysicsScene()))
		{
			if (Chaos::FPhysicsSolver* Solver = Scene->GetSolver())
			{
				auto* RigidSolver = static_cast<Chaos::FPBDRigidsSolver*>(Solver);
				CurrentPhysicsFrame = RigidSolver->GetCurrentFrame();
				bIsResimming = static_cast<Chaos::FPhysicsSolverBase*>(Solver)->IsResimming();
			}
		}
	}
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
	SimulateVehicleStep(StepInput, DeltaTime);
}

void UArcadeVehicleMovementComponent::SimulateVehicleStep(const FVehicleNetInputData& StepInput, float DeltaTime)
{
	InputCommand = StepInput.Cmd;

	if (!UpdatedPrimitive || !UpdatedPrimitive->IsSimulatingPhysics() || DeltaTime <= 0.0f)
	{
		bGrounded = false;
		ForwardSpeed = 0.0f;
		ResetWallEscape();
		return;
	}
	FBodyInstance* Body = UpdatedPrimitive->GetBodyInstance();
	FBodyInstanceAsyncPhysicsTickHandle PhysicsHandle = Body
		? Body->GetBodyInstanceAsyncPhysicsTickHandle() : FBodyInstanceAsyncPhysicsTickHandle();
	if (!PhysicsHandle.IsValid())
	{
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

	FVector GroundNormal = FVector::UpVector;
	bGrounded = UpdateGroundContact(BodyTransform.GetLocation(), GroundNormal);

	const FVector Velocity = PhysicsHandle->V();
	const FVector ForwardDirection = FVector::VectorPlaneProject(
		BodyTransform.GetRotation().GetForwardVector(), GroundNormal).GetSafeNormal();
	const FVector RightDirection = FVector::CrossProduct(GroundNormal, ForwardDirection).GetSafeNormal();

	ForwardSpeed = FVector::DotProduct(Velocity, ForwardDirection);

	if (!bGrounded || ForwardDirection.IsNearlyZero() || RightDirection.IsNearlyZero())
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
}

void UArcadeVehicleMovementComponent::ApplyHistoryState(const FVehicleNetStateData& InState)
{
	// Chaos owns the rigid-body rewind. The custom history restores only movement memory.
	bWallEscapeActive = InState.bWallEscapeActive;
	WallEscapeBlend = FMath::Clamp(InState.WallEscapeBlend, 0.0f, 1.0f);
	WallEscapeNormals = InState.WallEscapeNormals;
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
	}
	return true;
}

bool UArcadeVehicleMovementComponent::RestoreNetState(const FVehicleNetStateData& State)
{
	if (!UpdatedPrimitive || !UpdatedPrimitive->IsSimulatingPhysics()
		|| State.Position.ContainsNaN() || State.Rotation.ContainsNaN() || !State.Rotation.IsNormalized()
		|| State.LinearVelocity.ContainsNaN() || State.AngularVelocityRadians.ContainsNaN()
		|| State.PendingRecoilDeltaVelocity.ContainsNaN()
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
		if (bDrawWallEscapeDebug)
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
	if (bDrawWallEscapeDebug && World)
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

	const FVector TraceStart = BodyLocation;
	const float TraceLength = UpdatedPrimitive->Bounds.BoxExtent.Z + GroundTraceExtraDistance;
	const FVector TraceEnd = TraceStart - (FVector::UpVector * TraceLength);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArcadeVehicleGroundTrace), false, GetOwner());
	FHitResult GroundHit;
	const bool bHit = World->LineTraceSingleByChannel(
		GroundHit,
		TraceStart,
		TraceEnd,
		GroundTraceChannel,
		QueryParams);

	if (bDrawGroundDebug)
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

void UArcadeVehicleMovementComponent::ApplyLongitudinalForces(
	FBodyInstance& Body,
	const FVector& ForwardDirection,
	float CurrentForwardSpeed)
{
	// Brake wins when both face buttons are held, producing deterministic input.
	const bool bBrakeHeld = InputCommand.Brake > KINDA_SMALL_NUMBER;
	const bool bThrottleHeld = InputCommand.Throttle > KINDA_SMALL_NUMBER && !bBrakeHeld;

	float RequestedAcceleration = 0.0f;
	if (bBrakeHeld)
	{
		if (CurrentForwardSpeed > ReverseEngageSpeed)
		{
			RequestedAcceleration = -BrakeDeceleration * InputCommand.Brake;
		}
		else if (CurrentForwardSpeed > -MaxReverseSpeed)
		{
			RequestedAcceleration = -ReverseAcceleration * InputCommand.Brake;
		}
	}
	else if (bThrottleHeld)
	{
		if (CurrentForwardSpeed < -ReverseEngageSpeed)
		{
			RequestedAcceleration = BrakeDeceleration * InputCommand.Throttle;
		}
		else if (CurrentForwardSpeed < MaxForwardSpeed)
		{
			RequestedAcceleration = ForwardAcceleration * InputCommand.Throttle;
		}
	}

	FVector DriveAcceleration = ForwardDirection * RequestedAcceleration;
	if (bThrottleHeld && CurrentForwardSpeed >= -ReverseEngageSpeed && RequestedAcceleration > 0.0f)
	{
		DriveAcceleration = VehicleWallEscape::ConstrainThrottle(DriveAcceleration,
			WallEscapeNormals, WallEscapeIntoWallThrottleScale, WallEscapeBlend);
	}
	// Do not suppress braking, reversing, coasting drag or speed-limit corrections.
	float ResistanceAcceleration = -CurrentForwardSpeed * CoastingDragRate;

	if (CurrentForwardSpeed > MaxForwardSpeed)
	{
		ResistanceAcceleration -= (CurrentForwardSpeed - MaxForwardSpeed) * OverspeedCorrectionRate;
	}
	else if (CurrentForwardSpeed < -MaxReverseSpeed)
	{
		ResistanceAcceleration -= (CurrentForwardSpeed + MaxReverseSpeed) * OverspeedCorrectionRate;
	}

	FPhysicsInterface::AddForce_AssumesLocked(Body.GetPhysicsActor(),
		DriveAcceleration + ForwardDirection * ResistanceAcceleration, false, true, true);
}

void UArcadeVehicleMovementComponent::ApplyLateralGrip(
	FBodyInstance& Body,
	const FVector& RightDirection,
	float CurrentLateralSpeed)
{
	const float GripRate = InputCommand.bHandbrake ? HandbrakeGripRate : LateralGripRate;
	const float GripAcceleration = FMath::Clamp(
		-CurrentLateralSpeed * GripRate,
		-MaxLateralGripAcceleration,
		MaxLateralGripAcceleration);

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
