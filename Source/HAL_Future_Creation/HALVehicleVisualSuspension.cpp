#include "HALVehiclePresentationComponent.h"
#include "HALVehiclePresentationConfig.h"
#include "VehicleRecoveryComponent.h"
#include "BallControlComponent.h"
#include "BasicBallActor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "DrawDebugHelpers.h"
#include "Stats/Stats.h"

namespace
{
	TAutoConsoleVariable<int32> CVarSuspensionDebug(TEXT("hal.VehicleSuspensionDebug"), 0,
		TEXT("Visual suspension probes/targets and per-vehicle once-per-second query diagnostics. 0=off, 1=on."));
	DECLARE_CYCLE_STAT(TEXT("HAL Visual Suspension"), STAT_HALVisualSuspension, STATGROUP_Game);

	bool ValidProbeGeometry(const FHALVisualSuspensionConfig& Config, const FHALWheelSuspensionTravel& Travel,
		const FVector& Center, const FVector& Up, float Radius)
	{
		FString Error;
		return Config.Validate(Error) && Travel.Validate(Error) && !Center.ContainsNaN() && !Up.ContainsNaN()
			&& Up.IsNormalized() && Up.Z >= Config.MinimumVehicleUpZ && FMath::IsFinite(Radius) && Radius >= .1f;
	}
}

bool HALVehiclePresentation::BuildSuspensionProbe(const FHALVisualSuspensionConfig& Config, const FHALWheelSuspensionTravel& Travel,
	const FVector& Center, const FVector& Up, float Radius, FVector& Start, FVector& End)
{
	Start = End = Center;
	if (!ValidProbeGeometry(Config, Travel, Center, Up, Radius)) { return false; }
	Start = Center + Up * (Travel.MaxCompressionCm + Config.ProbeMarginCm);
	// A tilted support plane needs radius / dot(normal, up), not simply radius, along the probe.
	End = Center - Up * (Radius / Config.MinimumSupportNormalDot + Travel.MaxExtensionCm + Config.ProbeMarginCm);
	return true;
}

FHALWheelSuspensionTarget HALVehiclePresentation::SuspensionTarget(const FHALVisualSuspensionConfig& Config,
	const FHALWheelSuspensionTravel& Travel, const FVector& Center, const FVector& Up, float Radius, const FHALWheelSupportSample& Support)
{
	FHALWheelSuspensionTarget Target;
	if (!ValidProbeGeometry(Config, Travel, Center, Up, Radius)) { return Target; }
	Target.OffsetCm = -Travel.MaxExtensionCm;
	if (!Support.bHit || Support.bStartPenetrating || Support.Point.ContainsNaN() || Support.Normal.ContainsNaN()
		|| !Support.Normal.IsNormalized() || Support.Normal.Z < Config.MinimumSupportNormalDot) { return Target; }
	const double Alignment = FVector::DotProduct(Support.Normal, Up);
	if (Alignment < Config.MinimumSupportNormalDot) { return Target; }
	// Place a radius-sized decorative wheel tangent to the measured plane along the allowed travel axis.
	const double Offset = (Radius - FVector::DotProduct(Center - Support.Point, Support.Normal)) / Alignment;
	if (!FMath::IsFinite(Offset) || Offset < -Travel.MaxExtensionCm - UE_KINDA_SMALL_NUMBER) { return Target; }
	Target.OffsetCm = float(FMath::Clamp(Offset, -double(Travel.MaxExtensionCm), double(Travel.MaxCompressionCm)));
	Target.bSupported = true;
	return Target;
}

FVector HALVehiclePresentation::SuspensionBoneTranslation(const FTransform& MeshWorld, const FTransform& ReferenceSuspension,
	const FVector& Up, float Offset)
{
	if (MeshWorld.ContainsNaN() || ReferenceSuspension.ContainsNaN() || Up.ContainsNaN() || !FMath::IsFinite(Offset)
		|| MeshWorld.GetScale3D().GetMin() <= UE_SMALL_NUMBER || ReferenceSuspension.GetScale3D().GetMin() <= UE_SMALL_NUMBER)
	{ return FVector::ZeroVector; }
	return ReferenceSuspension.InverseTransformVector(MeshWorld.InverseTransformVector(Up * Offset));
}

void UHALVehiclePresentationComponent::UpdateSuspension(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_HALVisualSuspension);
	Frame.bSuspensionActive = false;
	Frame.SuspensionTraceCount = 0;
	Frame.SuspensionQueryMicroseconds = 0;
	for (auto& Support : WheelSupports) { Support = {}; }
	if ((!Config->Suspension.bEnabled && !bTireSkidReady) || !Mesh || !GetWorld())
	{
		for (auto& Wheel : Config->Wheels)
		{
			SuspensionOffsets[uint8(Wheel.Wheel)] = 0;
			auto& Pose = Frame.Wheel(Wheel.Wheel);
			Pose.SuspensionTranslation = FVector::ZeroVector;
			Pose.SuspensionOffsetCm = 0;
			Pose.bSuspensionSupported = false;
		}
		return;
	}
	const FVector Up = Body->GetUpVector();
	const auto* Recovery = GetOwner()->FindComponentByClass<UVehicleRecoveryComponent>();
	const FTransform MeshWorld = Mesh->GetComponentTransform();
	const FVector Scale = MeshWorld.GetScale3D();
	const bool bCanProbe = Up.Z >= Config->Suspension.MinimumVehicleUpZ && !(Recovery && Recovery->IsExecuting())
		&& !MeshWorld.ContainsNaN() && Scale.GetMin() > 0 && FMath::IsNearlyEqual(Scale.X, Scale.Y) && FMath::IsNearlyEqual(Scale.X, Scale.Z);
	Frame.bSuspensionActive = bCanProbe && Config->Suspension.bEnabled;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HALVisualSuspension), false, GetOwner());
	if (const auto* BallControl = GetOwner()->FindComponentByClass<UBallControlComponent>())
	{ if (const auto* Ball = BallControl->GetHeldBall()) { Params.AddIgnoredActor(Ball); } }
	// WorldStatic is an object type, not a Mobility test: current Movable test floors still qualify.
	// PhysicsBody balls/cars and WorldDynamic gameplay actors never become a decorative support.
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	for (const auto& Wheel : Config->Wheels)
	{
		const int32 Slot = uint8(Wheel.Wheel);
		auto& Pose = Frame.Wheel(Wheel.Wheel);
		const FVector Center = MeshWorld.TransformPosition(ReferenceWheelCenters[Slot]);
		FVector Start, End;
		FHALWheelSupportSample Support;
		FHALWheelSuspensionTarget Target;
		const bool bProbeBuilt = bCanProbe && HALVehiclePresentation::BuildSuspensionProbe(Config->Suspension, Wheel.Suspension, Center, Up, Wheel.RadiusCm, Start, End);
		if (bProbeBuilt)
		{
			FHitResult Hit;
			++Frame.SuspensionTraceCount;
			const double QueryStart = FPlatformTime::Seconds();
			const bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, Objects, Params);
			Frame.SuspensionQueryMicroseconds += float((FPlatformTime::Seconds() - QueryStart) * 1000000);
			if (bHit)
			{
				const auto* HitComponent = Hit.GetComponent();
				Support.bHit = HitComponent && !HitComponent->IsSimulatingPhysics() && !Cast<APawn>(Hit.GetActor()) && !Cast<ABasicBallActor>(Hit.GetActor());
				Support.bStartPenetrating = Hit.bStartPenetrating;
				Support.Point = Hit.ImpactPoint;
				Support.Normal = Hit.ImpactNormal;
			}
			Target = HALVehiclePresentation::SuspensionTarget(Config->Suspension, Wheel.Suspension, Center, Up, Wheel.RadiusCm, Support);
			if (Target.bSupported) { WheelSupports[Slot] = Support; }
		}
		if (!Config->Suspension.bEnabled) { Target = {}; }
		SuspensionOffsets[Slot] = bHasSample ? HALVehiclePresentation::SmoothSteering(SuspensionOffsets[Slot], Target.OffsetCm,
			Config->Suspension.ResponseSeconds, DeltaTime) : Target.OffsetCm;
		Pose.SuspensionOffsetCm = SuspensionOffsets[Slot];
		Pose.SuspensionTranslation = HALVehiclePresentation::SuspensionBoneTranslation(MeshWorld, ReferenceSuspensions[Slot], Up, SuspensionOffsets[Slot]);
		Pose.bSuspensionSupported = Target.bSupported;
#if ENABLE_DRAW_DEBUG
		if (bProbeBuilt && CVarSuspensionDebug.GetValueOnGameThread() != 0)
		{
			DrawDebugLine(GetWorld(), Start, End, Target.bSupported ? FColor::Green : FColor::Red, false, -1, 0, 1);
			DrawDebugPoint(GetWorld(), Center + Up * Target.OffsetCm, 8, FColor::Cyan, false, -1);
		}
#endif
	}
	if (CVarSuspensionDebug.GetValueOnGameThread() != 0)
	{
		SuspensionDebugElapsed += DeltaTime;
		if (SuspensionDebugElapsed >= 1)
		{
			SuspensionDebugElapsed = 0;
			UE_LOG(LogTemp, Display, TEXT("HAL VisualSuspension: %s Role=%d Active=%d Traces=%d QueryUs=%.2f OffsetsCm=%.2f/%.2f/%.2f/%.2f Support=%d/%d/%d/%d"),
				*GetNameSafe(GetOwner()), int32(GetOwner()->GetLocalRole()), Frame.bSuspensionActive ? 1 : 0, Frame.SuspensionTraceCount,
				Frame.SuspensionQueryMicroseconds, Frame.FL.SuspensionOffsetCm, Frame.FR.SuspensionOffsetCm, Frame.BL.SuspensionOffsetCm, Frame.BR.SuspensionOffsetCm,
				Frame.FL.bSuspensionSupported ? 1 : 0, Frame.FR.bSuspensionSupported ? 1 : 0, Frame.BL.bSuspensionSupported ? 1 : 0, Frame.BR.bSuspensionSupported ? 1 : 0);
		}
	}
}
