#include "HALOverdriveMotion.h"
#include "Serialization/Archive.h"

bool FHALOverdriveMotion::IsValidTuning() const
{
	return DurationSeconds >= .1f && DurationSeconds <= 30 && RampSeconds >= 0 && RampSeconds <= DurationSeconds
		&& RecoverySeconds >= .05f && RecoverySeconds <= 5 && InitialAccelerationScale >= 0 && InitialAccelerationScale <= 3
		&& FinalAccelerationScale >= InitialAccelerationScale && FinalAccelerationScale <= 3
		&& FinalSpeedScale >= 1 && FinalSpeedScale <= 3 && SteeringScale >= 0 && SteeringScale <= 1;
}
bool FHALOverdriveMotion::IsValidState(bool bStamped) const
{
	if (!Activation.IsValid()) { return *this == FHALOverdriveMotion{}; }
	return IsValidTuning() && (!bStamped || (StartFrame >= 0 && EndFrame >= StartFrame
		&& int64(EndFrame) - StartFrame <= 6000 && RampFrames >= 0 && RampFrames <= 6000
		&& RecoveryFrames > 0 && RecoveryFrames <= 6000 && StunFrame >= INDEX_NONE));
}
bool FHALOverdriveMotion::IsDriving(int32 Frame) const
{
	return Activation.IsValid() && Frame >= StartFrame && Frame < EndFrame;
}
bool FHALOverdriveMotion::IsRecovering(int32 Frame) const
{
	return Activation.IsValid() && EndFrame > StartFrame && Frame >= EndFrame && int64(Frame) < int64(EndFrame) + RecoveryFrames;
}
float FHALOverdriveMotion::RampAlpha(int32 Frame) const
{
	return RampFrames > 0 ? float(FMath::Clamp(double(int64(Frame) - StartFrame) / RampFrames, 0.0, 1.0)) : 1.f;
}
float FHALOverdriveMotion::SteeringAt(int32 Frame) const
{
	if (IsDriving(Frame)) { return SteeringScale; }
	return IsRecovering(Frame) ? FMath::Lerp(SteeringScale, 1.f, float(int64(Frame) - EndFrame) / RecoveryFrames) : 1.f;
}
void FHALOverdriveMotion::Stamp(const FHALOverdriveMotion* Previous, int32 Frame, float FixedDelta)
{
	if (!Activation.IsValid()) { return; }
	const bool bSame = Previous && Previous->Activation == Activation;
	if (bSame)
	{
		StartFrame = Previous->StartFrame;
		EndFrame = bActive ? Previous->EndFrame : FMath::Min(Previous->EndFrame, Frame);
		RampFrames = Previous->RampFrames;
		RecoveryFrames = Previous->RecoveryFrames;
	}
	else
	{
		StartFrame = Frame;
		EndFrame = bActive ? int32(FMath::Min(int64(MAX_int32), int64(Frame) + FMath::CeilToInt(DurationSeconds / FixedDelta))) : Frame;
		RampFrames = FMath::Clamp(FMath::CeilToInt(RampSeconds / FixedDelta), 0, 6000);
		RecoveryFrames = FMath::Clamp(FMath::CeilToInt(RecoverySeconds / FixedDelta), 1, 6000);
	}
	StunFrame = bStunned ? (bSame && Previous->bStunned ? Previous->StunFrame : Frame) : INDEX_NONE;
}
void FHALOverdriveMotion::Serialize(FArchive& Ar)
{
	Ar << Activation;
	if (!Activation.IsValid()) { if (Ar.IsLoading()) { *this = {}; } return; }
	Ar << DurationSeconds << RampSeconds << RecoverySeconds << InitialAccelerationScale << FinalAccelerationScale
		<< FinalSpeedScale << SteeringScale << bActive << bStunned << StartFrame << EndFrame << RampFrames << RecoveryFrames << StunFrame;
}
