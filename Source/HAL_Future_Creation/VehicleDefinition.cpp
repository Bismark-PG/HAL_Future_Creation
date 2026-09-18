#include "VehicleDefinition.h"

#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Misc/DataValidation.h"

namespace
{
	bool Fail(FString& Error, const TCHAR* Message) { Error = Message; return false; }
	bool PositiveVector(const FVector& V) { return !V.ContainsNaN() && V.X > 0 && V.Y > 0 && V.Z > 0; }
	bool ValidTransform(const FTransform& T) { return T.IsValid() && PositiveVector(T.GetScale3D()); }
	bool NonNegative(float V) { return FMath::IsFinite(V) && V >= 0; }
}

FVehicleBodyConfig::FVehicleBodyConfig()
{
	Body.SetCollisionProfileName(TEXT("PhysicsActor"));
	Body.bSimulatePhysics = true;
	Body.bEnableGravity = true;
	Body.bUseCCD = true;
	Body.bNotifyRigidBodyCollision = true;
	Body.bOverrideMass = true;
	Body.SetMassOverride(800, true);
	Body.LinearDamping = 0.15f;
	Body.AngularDamping = 0.8f;
}

bool FVehicleBodyConfig::Validate(FString& Error) const
{
	if (!PositiveVector(Scale) || !Body.bSimulatePhysics || Body.GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics
		|| Body.GetObjectType() != ECC_PhysicsBody || !Body.bOverrideMass
		|| !FMath::IsFinite(Body.GetMassOverride()) || Body.GetMassOverride() <= 0
		|| !NonNegative(Body.LinearDamping) || !NonNegative(Body.AngularDamping)
		|| !FMath::IsFinite(Body.MassScale) || Body.MassScale <= 0
		|| Body.COMNudge.ContainsNaN() || !PositiveVector(Body.InertiaTensorScale)
		|| !NonNegative(Body.MaxAngularVelocity) || !NonNegative(Body.GetMaxDepenetrationVelocity()))
	{
		return Fail(Error, TEXT("Physics requires a finite positive scale/mass, a simulating PhysicsBody with QueryAndPhysics, and valid damping/inertia."));
	}
	return true;
}

bool FVehicleVisualConfig::Validate(FString& Error) const
{
	return (IsValid(Mesh) && ValidTransform(RelativeTransform)) || Fail(Error, TEXT("Visual requires a mesh and finite positive relative scale."));
}

bool FVehicleCameraConfig::Validate(FString& Error) const
{
	if (!ValidTransform(BoomTransform) || !ValidTransform(CameraTransform) || SocketOffset.ContainsNaN() || TargetOffset.ContainsNaN()
		|| !NonNegative(TargetArmLength) || !NonNegative(ProbeSize) || ProbeChannel >= ECC_MAX
		|| !NonNegative(CameraLagSpeed) || !NonNegative(CameraRotationLagSpeed) || !NonNegative(CameraLagMaxDistance)
		|| !FMath::IsFinite(CameraLagMaxTimeStep) || CameraLagMaxTimeStep < 0.001f
		|| !FMath::IsFinite(FieldOfView) || FieldOfView < 1 || FieldOfView > 179
		|| !FMath::IsFinite(AspectRatio) || AspectRatio < 0.01f
		|| !NonNegative(PostProcessBlendWeight) || PostProcessBlendWeight > 1)
	{
		return Fail(Error, TEXT("Camera contains invalid transforms, channels or finite ranges."));
	}
	return true;
}

bool UVehicleLocalConfig::Validate(FString& Error) const
{
	if (!bReadyForUse) { return Fail(Error, TEXT("LocalConfig is not marked ReadyForUse after baseline comparison.")); }
	if (!DefaultMappingContext || !SteeringAction || !ThrottleAction || !BrakeAction || !HandbrakeAction || !LaunchAction)
	{
		return Fail(Error, TEXT("LocalConfig requires all six existing Enhanced Input references."));
	}
	return Camera.Validate(Error);
}

bool UVehicleDefinition::Validate(FString& Error) const
{
	if (!bReadyForUse) { return Fail(Error, TEXT("VehicleDefinition is not marked ReadyForUse after baseline comparison.")); }
	if (!Physics.Validate(Error) || !Visual.Validate(Error) || !Health.Validate(Error)) { return false; }
	if (CollisionShape == EVehicleCollisionShape::Box)
	{
		return PositiveVector(BoxExtent) || Fail(Error, TEXT("BoxExtent must be finite and positive."));
	}
	if (CollisionShape != EVehicleCollisionShape::ConvexMesh) { return Fail(Error, TEXT("Unknown vehicle collision shape.")); }
	if (!Physics.Body.bNotifyRigidBodyCollision) { return Fail(Error, TEXT("Driving vehicle requires hit notifications for forced ball release.")); }
	const UBodySetup* Setup = CollisionMesh ? CollisionMesh->GetBodySetup() : nullptr;
	if (!Setup || Setup->AggGeom.ConvexElems.IsEmpty() || Setup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple)
	{
		return Fail(Error, TEXT("Driving vehicle requires a simple convex collision mesh; ComplexAsSimple is invalid."));
	}
	if (!ValidTransform(BallControlPoint) || !ValidTransform(ForwardArrowTransform)
		|| !NonNegative(ForwardArrowSize) || !NonNegative(ForwardArrowLength))
	{
		return Fail(Error, TEXT("Invalid control point or marker configuration."));
	}
	return Movement.Validate(Error) && BallControl.Validate(Error)
		&& (LocalConfig ? LocalConfig->Validate(Error) : Fail(Error, TEXT("Driving vehicle requires LocalConfig.")));
}

UBallDefinition::UBallDefinition()
{
	Physics.Body.SetMassOverride(35, true);
	Physics.Body.LinearDamping = 0.25f;
	Physics.Body.AngularDamping = 0.1f;
	Physics.bHiddenInGame = false;
}

bool UBallDefinition::Validate(FString& Error) const
{
	if (!bReadyForUse) { return Fail(Error, TEXT("BallDefinition is not marked ReadyForUse after baseline comparison.")); }
	if (!FMath::IsFinite(Radius) || Radius < 0.01f) { return Fail(Error, TEXT("Ball radius must be finite and positive.")); }
	if (!Physics.Body.bNotifyRigidBodyCollision) { return Fail(Error, TEXT("Ball requires rigid-body hit notifications.")); }
	return Physics.Validate(Error) && Visual.Validate(Error) && Gameplay.Validate(Error);
}

FVehicleKnockbackConfig::FVehicleKnockbackConfig()
{
	LightTier.HorizontalDeltaSpeed = 250;
	MediumTier.HorizontalDeltaSpeed = 600;
	MediumTier.TargetVerticalSpeed = 500;
	MediumTier.AirborneFlipTurns = 1;
	HeavyTier.HorizontalDeltaSpeed = 900;
	HeavyTier.TargetVerticalSpeed = 750;
	HeavyTier.AirborneFlipTurns = 1.25f;
}

bool FVehicleKnockbackConfig::Validate(FString& Error) const
{
	if (!NonNegative(MediumImpactSpeedThreshold) || !NonNegative(HeavyImpactSpeedThreshold)
		|| HeavyImpactSpeedThreshold < MediumImpactSpeedThreshold || !NonNegative(MaxAirborneAngularSpeed)
		|| !NonNegative(GroundProbeExtraDistance)) { return Fail(Error, TEXT("Invalid knockback thresholds/stability values.")); }
	for (const FVehicleKnockbackTierDefinition* Tier : { &LightTier, &MediumTier, &HeavyTier })
	{
		if (!NonNegative(Tier->HorizontalDeltaSpeed) || !NonNegative(Tier->TargetVerticalSpeed)
			|| !NonNegative(Tier->AirborneFlipTurns) || Tier->AirborneFlipTurns > 3)
		{
			return Fail(Error, TEXT("Invalid finite knockback tier outcome."));
		}
	}
	return true;
}

EVehicleKnockbackTier FVehicleKnockbackConfig::SelectTier(float ImpactSpeed) const
{
	const float MediumThreshold = FMath::Max(0.0f, MediumImpactSpeedThreshold);
	if (ImpactSpeed >= FMath::Max(MediumThreshold, HeavyImpactSpeedThreshold)) { return EVehicleKnockbackTier::Heavy; }
	return ImpactSpeed >= MediumThreshold ? EVehicleKnockbackTier::Medium : EVehicleKnockbackTier::Light;
}

const FVehicleKnockbackTierDefinition& FVehicleKnockbackConfig::GetTierDefinition(EVehicleKnockbackTier Tier) const
{
	return Tier == EVehicleKnockbackTier::Heavy ? HeavyTier : (Tier == EVehicleKnockbackTier::Medium ? MediumTier : LightTier);
}

bool UCombatRulesDefinition::Validate(FString& Error) const
{
	return bReadyForUse ? BallImpacts.Validate(Error) : Fail(Error, TEXT("CombatRules is not marked ReadyForUse after baseline comparison."));
}

#if WITH_EDITOR
#define IMPLEMENT_CONFIG_VALIDATION(Type) \
EDataValidationResult Type::IsDataValid(FDataValidationContext& Context) const \
{ \
	Super::IsDataValid(Context); \
	FString Error; \
	if (!Validate(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; } \
	return EDataValidationResult::Valid; \
}
IMPLEMENT_CONFIG_VALIDATION(UVehicleDefinition)
IMPLEMENT_CONFIG_VALIDATION(UBallDefinition)
IMPLEMENT_CONFIG_VALIDATION(UVehicleLocalConfig)
IMPLEMENT_CONFIG_VALIDATION(UCombatRulesDefinition)
#undef IMPLEMENT_CONFIG_VALIDATION
#endif
