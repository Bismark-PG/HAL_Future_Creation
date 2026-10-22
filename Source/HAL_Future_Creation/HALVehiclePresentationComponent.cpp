#include "HALVehiclePresentationComponent.h"
#include "HALVehiclePresentationConfig.h"
#include "HALVehicleAnimInstance.h"
#include "ArcadeVehicleMovementComponent.h"
#include "VehicleRecoveryComponent.h"
#include "TestVehiclePawn.h"
#include "AnimationRuntime.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Net/UnrealNetwork.h"

UHALVehiclePresentationComponent::UHALVehiclePresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(true);
}

void UHALVehiclePresentationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UHALVehiclePresentationComponent, ObserverSteering, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UHALVehiclePresentationComponent, bObserverDrivingLeanAllowed, COND_SkipOwner);
}

void UHALVehiclePresentationComponent::Configure(UHALVehiclePresentationConfig* InConfig, UPrimitiveComponent* InBody,
	UStaticMeshComponent* InFallback, USkeletalMeshComponent* InMesh, bool bPreview)
{
	check(IsInGameThread());
	StopPresentation();
	Body = InBody; Fallback = InFallback; Mesh = InMesh;
	if (!InConfig || !Body || !Fallback || !Mesh) { return; }
	FString Error;
	if (!InConfig->Validate(Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: skeletal presentation fallback (%s): %s"),
			*GetNameSafe(GetOwner()), *GetNameSafe(InConfig), *Error);
		return;
	}
	Config = InConfig;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetRelativeTransform(Config->RelativeTransform);
	Mesh->SetSkeletalMesh(Config->Mesh);
	Mesh->SetAnimInstanceClass(Config->AnimClass);
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh->AddTickPrerequisiteComponent(this);
	for (const auto& Wheel : Config->Wheels)
	{
		ReferenceWheelCenters[uint8(Wheel.Wheel)] = FAnimationRuntime::GetComponentSpaceTransformRefPose(
			Config->Mesh->GetRefSkeleton(), Config->Mesh->GetRefSkeleton().FindBoneIndex(Wheel.SpinBone)).GetLocation();
		ReferenceSuspensions[uint8(Wheel.Wheel)] = FAnimationRuntime::GetComponentSpaceTransformRefPose(
			Config->Mesh->GetRefSkeleton(), Config->Mesh->GetRefSkeleton().FindBoneIndex(Wheel.SuspensionBone));
	}
	bFallbackWasVisible = Fallback->IsVisible();
	bFallbackWasHiddenInGame = Fallback->bHiddenInGame;
	Fallback->SetVisibility(false);
	Fallback->SetHiddenInGame(true);
	Mesh->SetVisibility(true);
	Mesh->SetHiddenInGame(false);
	bConfigured = true;
	ResetPose();
	SetComponentTickEnabled(!bPreview);
}

void UHALVehiclePresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(bConfigured);
}

void UHALVehiclePresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	StopPresentation();
	Super::EndPlay(Reason);
}

void UHALVehiclePresentationComponent::StopPresentation()
{
	SetComponentTickEnabled(false);
	if (bConfigured && Fallback)
	{
		Fallback->SetVisibility(bFallbackWasVisible);
		Fallback->SetHiddenInGame(bFallbackWasHiddenInGame);
	}
	if (Mesh)
	{
		Mesh->RemoveTickPrerequisiteComponent(this);
		Mesh->SetHiddenInGame(true);
		Mesh->SetVisibility(false);
		Mesh->SetAnimInstanceClass(nullptr);
		Mesh->SetSkeletalMesh(nullptr);
	}
	bConfigured = false;
	Config = nullptr;
	ResetPose();
}

void UHALVehiclePresentationComponent::ResetPose()
{
	Frame = FHALVehiclePresentationFrame();
	ChassisState = FHALChassisPresentationState();
	for (float& Angle : SpinDegrees) { Angle = 0; }
	for (float& Offset : SuspensionOffsets) { Offset = 0; }
	SuspensionDebugElapsed = 0;
	bHasSample = false;
	PreviousController.Reset();
	PublishElapsed = 0;
}

bool UHALVehiclePresentationComponent::ReadDrivingLeanAllowed() const
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Body || Body->GetUpVector().Z <= 0) { return false; }
	if (const auto* Recovery = Pawn->FindComponentByClass<UVehicleRecoveryComponent>(); Recovery && Recovery->IsExecuting()) { return false; }
	if (Pawn->HasAuthority() || Pawn->IsLocallyControlled())
	{
		const auto* Movement = Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>();
		const auto Sample = Movement ? Movement->GetStabilityObservation() : FVehicleStabilityObservation();
		return Sample.bValid && Sample.bNearGround && Sample.bDrivingAllowed && !Sample.Recovery.bActive;
	}
	return bObserverDrivingLeanAllowed;
}

float UHALVehiclePresentationComponent::ReadSteering(float DeltaTime, bool bDrivingLeanAllowed)
{
	auto* Pawn = Cast<ATestVehiclePawn>(GetOwner());
	if (!Pawn) { return 0; }
	float Accepted = 0;
	if (Pawn->HasAuthority())
	{
		if (Pawn->GetController())
		{
			if (const auto* Movement = Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>())
			{
				const auto Input = Movement->GetPresentationInput();
				Accepted = Input.bValid ? Input.Steering : 0;
			}
		}
		// Cosmetic property updates are bounded to 15 Hz, independent of render rate.
		PublishElapsed += DeltaTime;
		if (PublishElapsed >= 1.f / 15.f)
		{
			PublishElapsed = FMath::Fmod(PublishElapsed, 1.f / 15.f);
			const int8 Quantized = int8(FMath::RoundToInt(FMath::Clamp(Accepted, -1.f, 1.f) * 127.f));
			if (ObserverSteering != Quantized || bObserverDrivingLeanAllowed != bDrivingLeanAllowed)
			{
				ObserverSteering = Quantized;
				bObserverDrivingLeanAllowed = bDrivingLeanAllowed;
				Pawn->ForceNetUpdate();
			}
		}
	}
	if (Pawn->IsLocallyControlled())
	{
		const float Local = Pawn->GetCurrentInputCommand().Steering;
		return FMath::IsFinite(Local) ? FMath::Clamp(Local, -1.f, 1.f) : 0;
	}
	return Pawn->HasAuthority() ? Accepted : GetObserverSteering();
}

void UHALVehiclePresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	check(IsInGameThread());
	if (!bConfigured || !Config || !Body) { return; }
	const FVector Position = Body->GetComponentLocation();
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const bool bControllerChanged = PreviousController.Get() != (Pawn ? Pawn->GetController() : nullptr);
	if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0 || DeltaTime > Config->MaxSampleDeltaSeconds
		|| Position.ContainsNaN() || (bHasSample && (bControllerChanged
			|| FVector::DistSquared(Position, PreviousPosition) > FMath::Square(Config->TeleportResetDistanceCm))))
	{
		ResetPose();
		return;
	}
	PreviousController = Pawn ? Pawn->GetController() : nullptr;
	PreviousPosition = Position;
	const bool bDrivingLeanAllowed = ReadDrivingLeanAllowed();
	const float Steering = ReadSteering(DeltaTime, bDrivingLeanAllowed);
	// These external/game-thread physics APIs synchronize access to the rigid body.
	const FVector Velocity = Body->GetPhysicsLinearVelocity();
	const FVector Angular = Body->GetPhysicsAngularVelocityInRadians();
	const FVector CenterOfMass = Body->GetCenterOfMass();
	if (Velocity.ContainsNaN() || Angular.ContainsNaN() || CenterOfMass.ContainsNaN()) { ResetPose(); return; }
	Frame.SteeringDegrees = HALVehiclePresentation::SmoothSteering(Frame.SteeringDegrees,
		Steering * Config->MaxSteeringDegrees, Config->SteeringResponseSeconds, DeltaTime);
	Frame.ForwardSpeedCmPerSecond = FVector::DotProduct(Velocity, Body->GetForwardVector());
	FHALChassisMotionSample ChassisSample;
	ChassisSample.Position = Position;
	ChassisSample.Velocity = Velocity;
	ChassisSample.Rotation = Body->GetComponentQuat();
	ChassisSample.bDrivingAllowed = bDrivingLeanAllowed;
	HALVehiclePresentation::UpdateChassis(Config->Chassis, ChassisSample, DeltaTime, ChassisState);
	Frame.ChassisRotation = HALVehiclePresentation::ChassisRotation(Config->Chassis, ChassisState);
	Frame.ChassisPitchDegrees = ChassisState.PitchDegrees;
	Frame.ChassisRollDegrees = ChassisState.RollDegrees;
	Frame.bDrivingLeanActive = Config->Chassis.bEnabled && bDrivingLeanAllowed;
	for (const auto& Wheel : Config->Wheels)
	{
		const int32 Slot = uint8(Wheel.Wheel);
		auto& Pose = Frame.Wheel(Wheel.Wheel);
		const float Steer = Wheel.bSteers ? Frame.SteeringDegrees : 0;
		Pose.SteeringRotation = HALVehiclePresentation::AxisRotation(Wheel.SteeringAxis, Steer * Wheel.SteeringSign);
		const FVector Point = Mesh ? Mesh->GetComponentTransform().TransformPosition(ReferenceWheelCenters[Slot]) : Position;
		const FVector PointVelocity = Velocity + FVector::CrossProduct(Angular, Point - CenterOfMass);
		const FVector RollingDirection = FQuat(Body->GetUpVector(), FMath::DegreesToRadians(Steer)).RotateVector(Body->GetForwardVector());
		float Speed = FVector::DotProduct(PointVelocity, RollingDirection);
		if (FMath::Abs(Speed) < Config->RollingDeadZoneCmPerSecond) { Speed = 0; }
		if (bHasSample) { SpinDegrees[Slot] = HALVehiclePresentation::AdvanceWheel(SpinDegrees[Slot], Speed, Wheel.RadiusCm, Wheel.SpinSign, DeltaTime); }
		Pose.SpinRotation = HALVehiclePresentation::AxisRotation(Wheel.SpinAxis, SpinDegrees[Slot]);
	}
	UpdateSuspension(DeltaTime);
	bHasSample = true;
	Frame.bValid = true;
}
