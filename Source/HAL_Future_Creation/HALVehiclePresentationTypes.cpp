#include "HALVehiclePresentationTypes.h"

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
