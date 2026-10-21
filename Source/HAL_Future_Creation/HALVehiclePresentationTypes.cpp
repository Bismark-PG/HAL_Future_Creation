#include "HALVehiclePresentationTypes.h"
#include "HALVehiclePresentationConfig.h"

FHALWheelPresentationPose& FHALVehiclePresentationFrame::Wheel(EHALVehicleWheel Id)
{
	switch (Id)
	{
	case EHALVehicleWheel::FR: return FR;
	case EHALVehicleWheel::BL: return BL;
	case EHALVehicleWheel::BR: return BR;
	default: return FL;
	}
}

FRotator HALVehiclePresentation::AxisRotation(EHALBoneAxis Axis, float Degrees)
{
	if (!FMath::IsFinite(Degrees)) { return FRotator::ZeroRotator; }
	const FVector Vector = Axis == EHALBoneAxis::X ? FVector::ForwardVector
		: (Axis == EHALBoneAxis::Y ? FVector::RightVector : FVector::UpVector);
	return FQuat(Vector, FMath::DegreesToRadians(Degrees)).Rotator();
}

float HALVehiclePresentation::AdvanceWheel(float Angle, float Speed, float Radius, float Sign, float Delta)
{
	if (!FMath::IsFinite(Angle) || !FMath::IsFinite(Speed) || !FMath::IsFinite(Radius)
		|| !FMath::IsFinite(Sign) || !FMath::IsFinite(Delta) || Radius < 0.1f || Delta < 0)
	{
		return 0;
	}
	const double Result = double(Angle) + FMath::RadiansToDegrees(double(Speed) / Radius * Sign * Delta);
	return FMath::IsFinite(Result) ? float(FMath::UnwindDegrees(Result)) : 0;
}

float HALVehiclePresentation::SmoothSteering(float Current, float Target, float Response, float Delta)
{
	if (!FMath::IsFinite(Current) || !FMath::IsFinite(Target) || !FMath::IsFinite(Response)
		|| !FMath::IsFinite(Delta) || Delta < 0) { return 0; }
	return Response <= 0 ? Target : FMath::Lerp(Current, Target, 1.f - FMath::Exp(-Delta / Response));
}

void HALVehiclePresentation::UpdateChassis(const FHALChassisPresentationConfig& Config,
	const FHALChassisMotionSample& Sample, float Delta, FHALChassisPresentationState& State)
{
	if (!Config.bEnabled || !FMath::IsFinite(Delta) || Delta <= 0 || Sample.Position.ContainsNaN()
		|| Sample.Velocity.ContainsNaN() || Sample.Rotation.ContainsNaN() || !Sample.Rotation.IsNormalized())
	{ State = {}; return; }
	if (!State.bHasSample)
	{ State.Previous = Sample; State.bHasSample = true; return; }
	// Differentiate world velocity before projecting into today's vehicle axes.
	const FVector Acceleration = (Sample.Velocity - State.Previous.Velocity) / Delta;
	const FVector ExpectedPosition = State.Previous.Position + (Sample.Velocity + State.Previous.Velocity) * (.5f * Delta);
	const bool bDiscontinuity = Acceleration.ContainsNaN()
		|| Acceleration.SizeSquared() > FMath::Square(Config.MaximumSampleAcceleration)
		|| FVector::DistSquared(Sample.Position, ExpectedPosition) > FMath::Square(Config.CorrectionResetDistanceCm);
	float ForwardTarget = 0, RightTarget = 0;
	if (bDiscontinuity)
	{
		State.ForwardAcceleration = State.RightAcceleration = 0;
	}
	else if (Sample.bDrivingAllowed && State.Previous.bDrivingAllowed)
	{
		ForwardTarget = float(FVector::DotProduct(Acceleration, Sample.Rotation.GetForwardVector()));
		RightTarget = float(FVector::DotProduct(Acceleration, Sample.Rotation.GetRightVector()));
	}
	// Filter BEFORE saturation: at 120 render Hz a 60 Hz body publishes alternating zero/double
	// velocity differences. Clamping those impulses first would incorrectly halve steady lean.
	State.ForwardAcceleration = SmoothSteering(State.ForwardAcceleration, ForwardTarget, Config.ResponseSeconds, Delta);
	State.RightAcceleration = SmoothSteering(State.RightAcceleration, RightTarget, Config.ResponseSeconds, Delta);
	auto Response = [&Config](float Value, float FullAcceleration, float Limit)
	{
		const float Magnitude = FMath::Max(0.f, FMath::Abs(Value) - Config.AccelerationDeadZone);
		return FMath::Sign(Value) * FMath::Clamp(Magnitude / (FullAcceleration - Config.AccelerationDeadZone), 0.f, 1.f) * Limit;
	};
	State.PitchDegrees = Response(State.ForwardAcceleration, Config.FullPitchAcceleration, Config.MaxPitchDegrees);
	State.RollDegrees = Response(State.RightAcceleration, Config.FullRollAcceleration, Config.MaxRollDegrees);
	State.Previous = Sample;
}

FRotator HALVehiclePresentation::ChassisRotation(const FHALChassisPresentationConfig& Config, const FHALChassisPresentationState& State)
{
	return (AxisRotation(Config.RollAxis, State.RollDegrees * Config.RollSign).Quaternion()
		* AxisRotation(Config.PitchAxis, State.PitchDegrees * Config.PitchSign).Quaternion()).Rotator();
}
