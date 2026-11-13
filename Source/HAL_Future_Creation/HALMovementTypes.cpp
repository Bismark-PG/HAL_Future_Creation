#include "HALMovementTypes.h"
#include "Serialization/Archive.h"

bool FHALGravityDrive::IsValid(bool bStamped) const
{
	if (!Source.IsValid()) { return *this == FHALGravityDrive{}; }
	return !Center.ContainsNaN() && Radius >= 1 && Radius <= 10000 && HalfHeight >= 1 && HalfHeight <= 1000
		&& SofteningRadius >= 1 && SofteningRadius <= 1000 && Inward >= 0 && Inward <= 1
		&& Tangential >= -1 && Tangential <= 1 && Inward * Inward + Tangential * Tangential > 0
		&& Strength >= 0 && Strength <= 1 && SteeringResistance >= 0 && SteeringResistance <= 1
		&& RampSeconds >= 0 && RampSeconds <= 5 && RampFrames >= 0 && RampFrames <= 6000
		&& (bStamped ? StartFrame >= 0 : StartFrame >= INDEX_NONE);
}
void FHALGravityDrive::Stamp(const FHALGravityDrive* Previous, int32 Frame, float FixedDeltaSeconds)
{
	if (!Source.IsValid()) { *this = {}; return; }
	StartFrame = Previous && Previous->Source == Source ? Previous->StartFrame : Frame;
	RampFrames = FixedDeltaSeconds > 0 ? FMath::Clamp(FMath::CeilToInt(RampSeconds / FixedDeltaSeconds), 0, 6000) : 0;
}
void FHALGravityDrive::Serialize(FArchive& Ar)
{
	bool bActive = Source.IsValid(); Ar.SerializeBits(&bActive, 1);
	if (bActive)
	{
		Ar << Source << Center << Radius << HalfHeight << SofteningRadius << Inward << Tangential;
		Ar << Strength << SteeringResistance << RampSeconds << StartFrame << RampFrames;
		if (!Source.IsValid()) { Ar.SetError(); }
	}
	else if (Ar.IsLoading()) { *this = {}; }
}
FVector FHALGravityDrive::Bend(const FVector& Position, const FVector& Forward, float Steering, int32 Frame) const
{
	if (!Source.IsValid() || Frame < StartFrame) { return Forward; }
	const FVector Offset = Center - Position;
	if (FMath::Abs(Offset.Z) > HalfHeight || Offset.SizeSquared2D() > FMath::Square(double(Radius))) { return Forward; }
	const FVector ToCenter = Offset.GetSafeNormal2D(), Heading = Forward.GetSafeNormal2D();
	const FVector Flow = (ToCenter * Inward + FVector::CrossProduct(FVector::UpVector, -ToCenter) * Tangential).GetSafeNormal();
	if (Flow.IsNearlyZero() || Heading.IsNearlyZero()) { return Forward; }
	const float T = RampFrames > 0 ? FMath::Clamp(float(int64(Frame) - StartFrame) / RampFrames, 0.f, 1.f) : 1.f;
	const double Alpha = Strength * (T*T*(3.f-2.f*T)) * (1.f-SteeringResistance*FMath::Clamp(FMath::Abs(Steering),0.f,1.f))
		* FMath::Min(Offset.Size2D()/SofteningRadius,1.);
	// Interpolate angles, not vectors: opposing headings must not cancel propulsion magnitude.
	double Angle = FMath::Atan2(FVector::CrossProduct(Heading,Flow).Z,FVector::DotProduct(Heading,Flow));
	if (FMath::Abs(FMath::Abs(Angle)-UE_PI) < 1.e-6) { Angle = Tangential < 0 ? -UE_PI : UE_PI; }
	const double Turn = Angle * Alpha, Cos = FMath::Cos(Turn), Sin = FMath::Sin(Turn);
	return FVector(Forward.X*Cos-Forward.Y*Sin,Forward.X*Sin+Forward.Y*Cos,Forward.Z).GetSafeNormal();
}

bool FHALMovementAreaSway::IsValid() const
{
	return Amplitude >= 0 && Amplitude <= 12 && PeriodSeconds >= .2f && PeriodSeconds <= 3
		&& FullSpeed >= 1 && FullSpeed <= 6000 && RampSeconds >= 0 && RampSeconds <= 2;
}

bool FHALMovementAreaSway::IsStrongerThan(const FHALMovementAreaSway& Other) const
{
	if (Amplitude != Other.Amplitude) { return Amplitude > Other.Amplitude; }
	if (Amplitude == 0) { return false; }
	if (PeriodSeconds != Other.PeriodSeconds) { return PeriodSeconds > Other.PeriodSeconds; }
	if (FullSpeed != Other.FullSpeed) { return FullSpeed < Other.FullSpeed; }
	return RampSeconds < Other.RampSeconds;
}

bool FHALMovementAreaChannel::IsValid(bool bStamped) const
{
	return FMath::IsFinite(OrdinaryDelta) && FMath::IsFinite(Target) && Target >= 0 && Target <= 1
		&& FMath::IsFinite(RecoverySeconds) && RecoverySeconds >= 0 && RecoverySeconds <= 10
		&& FMath::IsFinite(From) && From >= 0 && From <= 1 && DurationFrames >= 0 && DurationFrames <= 6000
		&& (bStamped ? StartFrame >= 0 : StartFrame >= INDEX_NONE);
}
bool FHALMovementAreaChannel::Equals(const FHALMovementAreaChannel& Other) const
{
	return OrdinaryDelta == Other.OrdinaryDelta && Target == Other.Target && RecoverySeconds == Other.RecoverySeconds
		&& From == Other.From && StartFrame == Other.StartFrame && DurationFrames == Other.DurationFrames;
}
float FHALMovementAreaChannel::RegionalValue(int32 Frame) const
{
	const double Alpha = DurationFrames > 0 ? FMath::Clamp(double(int64(Frame) - StartFrame) / DurationFrames, 0.0, 1.0) : 1.0;
	return FMath::Lerp(From, Target, float(Alpha));
}
float FHALMovementAreaChannel::EffectiveValue(int32 Frame) const
{
	// Sum before clamping: even a saturated ordinary buff must retain its contribution.
	return float(FMath::Clamp(double(OrdinaryDelta) + RegionalValue(Frame), 0.0, 3.0));
}
void FHALMovementAreaChannel::Stamp(const FHALMovementAreaChannel* Previous, int32 Frame, float FixedDeltaSeconds)
{
	if (Previous && Target == Previous->Target)
	{
		From = Previous->From; StartFrame = Previous->StartFrame; DurationFrames = Previous->DurationFrames;
		return; // A boost starting/expiring must not restart the area's recovery.
	}
	From = Previous ? Previous->RegionalValue(Frame) : 1;
	StartFrame = Frame;
	DurationFrames = Target > From && RecoverySeconds > 0 && FixedDeltaSeconds > 0
		? FMath::Clamp(FMath::CeilToInt(RecoverySeconds / FixedDeltaSeconds), 1, 6000) : 0;
	if (DurationFrames == 0) { From = Target; }
}
void FHALMovementAreaChannel::Serialize(FArchive& Ar)
{
	Ar << OrdinaryDelta << Target << RecoverySeconds << From << StartFrame << DurationFrames;
}
bool FHALMovementSnapshot::IsValidValues() const
{
	return Epoch.IsValid() && Revision != 0 && FMath::IsFinite(AccelerationScale) && FMath::IsFinite(SpeedScale)
		&& FMath::IsFinite(GripScale) && AccelerationScale >= 0 && AccelerationScale <= 3
		&& SpeedScale >= 0 && SpeedScale <= 3 && GripScale >= 0 && GripScale <= 3
		&& Overdrive.IsValidState(false) && GravityDrive.IsValid(false) && AreaSway.IsValid() && (bHasAreaState || AreaSway.Amplitude == 0)
		&& (!bHasAreaState || (AreaAcceleration.IsValid(false) && AreaSpeed.IsValid(false) && AreaGrip.IsValid(false)));
}
bool FHALMovementSnapshot::IsValid() const
{
	const bool bSwayValid = AreaSway.Amplitude > 0
		? SwayStartFrame >= 0 && SwayStartFrame <= EffectiveServerFrame && SwayPeriodFrames > 0
			&& SwayPeriodFrames <= 6000 && SwayRampFrames >= 0 && SwayRampFrames <= 6000
		: SwayStartFrame == INDEX_NONE && SwayPeriodFrames == 0 && SwayRampFrames == 0;
	const bool bOverdriveValid = Overdrive.IsValidState(true) && (!Overdrive.Activation.IsValid()
		|| (Overdrive.StartFrame <= EffectiveServerFrame && (!Overdrive.bStunned
			|| (Overdrive.StunFrame >= Overdrive.StartFrame && Overdrive.StunFrame <= EffectiveServerFrame))));
	return IsValidValues() && GravityDrive.IsValid(true) && (!GravityDrive.Source.IsValid() || GravityDrive.StartFrame <= EffectiveServerFrame)
		&& bOverdriveValid && bSwayValid && EffectiveServerFrame >= 0 && (!bHasAreaState
		|| (AreaAcceleration.IsValid(true) && AreaSpeed.IsValid(true) && AreaGrip.IsValid(true)
			&& AreaAcceleration.StartFrame <= EffectiveServerFrame && AreaSpeed.StartFrame <= EffectiveServerFrame
			&& AreaGrip.StartFrame <= EffectiveServerFrame));
}
bool FHALMovementSnapshot::IsUnset() const
{
	return !Epoch.IsValid() && Revision == 0 && EffectiveServerFrame == INDEX_NONE
		&& AccelerationScale == 1 && SpeedScale == 1 && GripScale == 1 && !bHasAreaState && !bControlsBlocked && !bHandbrakeBlocked
		&& Overdrive == FHALOverdriveMotion{} && GravityDrive == FHALGravityDrive{} && AreaSway == FHALMovementAreaSway{} && SwayStartFrame == INDEX_NONE && SwayPeriodFrames == 0 && SwayRampFrames == 0;
}
bool FHALMovementSnapshot::Equals(const FHALMovementSnapshot& Other) const
{
	return Epoch == Other.Epoch && Revision == Other.Revision && EffectiveServerFrame == Other.EffectiveServerFrame
		&& FMath::IsNearlyEqual(AccelerationScale, Other.AccelerationScale, 1.e-4f)
		&& FMath::IsNearlyEqual(SpeedScale, Other.SpeedScale, 1.e-4f)
		&& FMath::IsNearlyEqual(GripScale, Other.GripScale, 1.e-4f) && bHasAreaState == Other.bHasAreaState
		&& bControlsBlocked == Other.bControlsBlocked && bHandbrakeBlocked == Other.bHandbrakeBlocked
		&& (!bHasAreaState || (AreaAcceleration.Equals(Other.AreaAcceleration)
			&& AreaSpeed.Equals(Other.AreaSpeed) && AreaGrip.Equals(Other.AreaGrip)))
		&& Overdrive == Other.Overdrive && GravityDrive == Other.GravityDrive && AreaSway == Other.AreaSway && SwayStartFrame == Other.SwayStartFrame
		&& SwayPeriodFrames == Other.SwayPeriodFrames && SwayRampFrames == Other.SwayRampFrames;
}
void FHALMovementSnapshot::Serialize(FArchive& Ar)
{
	Ar << Epoch << Revision << EffectiveServerFrame << AccelerationScale << SpeedScale << bControlsBlocked << GripScale << bHasAreaState;
	Ar << bHandbrakeBlocked;
	Overdrive.Serialize(Ar);
	GravityDrive.Serialize(Ar);
	if (bHasAreaState) { AreaAcceleration.Serialize(Ar); AreaSpeed.Serialize(Ar); AreaGrip.Serialize(Ar); }
	else if (Ar.IsLoading()) { AreaAcceleration = {}; AreaSpeed = {}; AreaGrip = {}; }
	bool bHasSway = !Ar.IsLoading() && AreaSway.Amplitude > 0;
	Ar.SerializeBits(&bHasSway, 1);
	if (bHasSway)
	{
		Ar << AreaSway.Amplitude << AreaSway.PeriodSeconds << AreaSway.FullSpeed << AreaSway.RampSeconds;
		Ar << SwayStartFrame << SwayPeriodFrames << SwayRampFrames;
		if (AreaSway.Amplitude <= 0) { Ar.SetError(); }
	}
	else if (Ar.IsLoading()) { AreaSway = {}; SwayStartFrame = INDEX_NONE; SwayPeriodFrames = SwayRampFrames = 0; }
}
void FHALMovementSnapshot::Stamp(const FHALMovementSnapshot* Previous, int32 Frame, float FixedDeltaSeconds)
{
	EffectiveServerFrame = Frame;
	Overdrive.Stamp(Previous ? &Previous->Overdrive : nullptr, Frame, FixedDeltaSeconds);
	GravityDrive.Stamp(Previous ? &Previous->GravityDrive : nullptr, Frame, FixedDeltaSeconds);
	SwayStartFrame = INDEX_NONE;
	SwayPeriodFrames = SwayRampFrames = 0;
	if (!bHasAreaState) { return; }
	const bool bPrevious = Previous && Previous->IsValid() && Previous->Epoch == Epoch && Previous->bHasAreaState;
	AreaAcceleration.Stamp(bPrevious ? &Previous->AreaAcceleration : nullptr, Frame, FixedDeltaSeconds);
	AreaSpeed.Stamp(bPrevious ? &Previous->AreaSpeed : nullptr, Frame, FixedDeltaSeconds);
	AreaGrip.Stamp(bPrevious ? &Previous->AreaGrip : nullptr, Frame, FixedDeltaSeconds);
	if (AreaSway.Amplitude <= 0 || FixedDeltaSeconds <= 0) { AreaSway = {}; return; }
	SwayStartFrame = bPrevious && Previous->AreaSway.Amplitude > 0 ? Previous->SwayStartFrame : Frame;
	SwayPeriodFrames = FMath::Clamp(FMath::RoundToInt(AreaSway.PeriodSeconds / FixedDeltaSeconds), 1, 6000);
	SwayRampFrames = FMath::Clamp(FMath::CeilToInt(AreaSway.RampSeconds / FixedDeltaSeconds), 0, 6000);
}
FVector FHALMovementSnapshot::EvaluateScales(int32 Frame) const
{
	FVector Scales = bHasAreaState ? FVector(AreaAcceleration.EffectiveValue(Frame), AreaSpeed.EffectiveValue(Frame), AreaGrip.EffectiveValue(Frame))
		: FVector(AccelerationScale, SpeedScale, GripScale);
	if (Overdrive.IsDriving(Frame))
	{
		const double AccelerationBase = bHasAreaState ? double(AreaAcceleration.OrdinaryDelta) + AreaAcceleration.RegionalValue(Frame) : Scales.X;
		const double SpeedBase = bHasAreaState ? double(AreaSpeed.OrdinaryDelta) + AreaSpeed.RegionalValue(Frame) : Scales.Y;
		Scales.X = FMath::Clamp(AccelerationBase + FMath::Lerp(Overdrive.InitialAccelerationScale, Overdrive.FinalAccelerationScale, Overdrive.RampAlpha(Frame)) - 1, 0., 3.);
		Scales.Y = FMath::Clamp(SpeedBase + (Overdrive.FinalSpeedScale - 1) * Overdrive.RampAlpha(Frame), 0., 3.);
	}
	return Scales;
}
float FHALMovementSnapshot::EvaluateSway(int32 Frame, float PlanarSpeed) const
{
	if (AreaSway.Amplitude <= 0 || SwayPeriodFrames <= 0 || Frame < SwayStartFrame) { return 0; }
	const int64 Age = int64(Frame) - SwayStartFrame;
	const float SpeedAlpha = FMath::Clamp(PlanarSpeed / AreaSway.FullSpeed, 0.f, 1.f);
	const float RampAlpha = SwayRampFrames > 0 ? FMath::Min(float(Age) / SwayRampFrames, 1.f) : 1.f;
	const float Phase = 2.f * UE_PI * float(Age % SwayPeriodFrames) / SwayPeriodFrames;
	return AreaSway.Amplitude * SpeedAlpha * RampAlpha * FMath::Sin(Phase);
}
bool FHALMovementSnapshot::CalculateScales(float AccelerationDelta, float SpeedDelta, float& Acceleration, float& Speed)
{
	if (!FMath::IsFinite(AccelerationDelta) || !FMath::IsFinite(SpeedDelta)) { return false; }
	Acceleration = float(FMath::Clamp(1.0 + double(AccelerationDelta), 0.0, 3.0));
	Speed = float(FMath::Clamp(1.0 + double(SpeedDelta), 0.0, 3.0));
	return true;
}
bool FHALMovementTimeline::Validate() const
{
	if (FormatVersion != CurrentFormat || Changes.Num() >= MaximumEntries) { return false; }
	if (IsEmpty()) { return EndFrame == INDEX_NONE && !Epoch.IsValid() && Anchor.IsUnset() && Changes.IsEmpty(); }
	if (StartFrame < 0 || EndFrame < StartFrame || EndFrame - StartFrame >= MaximumEntries
		|| !Anchor.IsValid() || Epoch != Anchor.Epoch || Anchor.EffectiveServerFrame > StartFrame) { return false; }
	FHALMovementSnapshot Previous = Anchor;
	for (const auto& Value : Changes)
	{
		if (!Value.IsValid() || Value.Epoch != Epoch || Value.EffectiveServerFrame <= StartFrame
			|| Value.EffectiveServerFrame <= Previous.EffectiveServerFrame || Value.EffectiveServerFrame > EndFrame
			|| Value.Revision <= Previous.Revision) { return false; }
		Previous = Value;
	}
	return true;
}
bool FHALMovementTimeline::Query(int32 Frame, FHALMovementSnapshot& Out) const
{
	if (IsEmpty() || Frame < StartFrame || Frame > EndFrame) { return false; }
	Out = Anchor;
	for (const auto& Change : Changes)
	{
		if (Change.EffectiveServerFrame > Frame) { break; }
		Out = Change;
	}
	return true;
}
void FHALMovementTimeline::Trim(int32 WindowFrames)
{
	if (IsEmpty()) { return; }
	WindowFrames = FMath::Clamp(WindowFrames, 1, MaximumEntries - 2);
	const int32 NewStart = FMath::Max(StartFrame, EndFrame - WindowFrames + 1);
	FHALMovementSnapshot NewAnchor;
	if (Query(NewStart, NewAnchor))
	{
		Anchor = NewAnchor;
		StartFrame = NewStart;
		Changes.RemoveAll([NewStart](const auto& Value) { return Value.EffectiveServerFrame <= NewStart; });
	}
}
bool FHALMovementTimeline::ExtendTo(int32 Frame, int32 WindowFrames)
{
	if (IsEmpty() || Frame < 0) { return false; }
	EndFrame = FMath::Max(EndFrame, Frame);
	Trim(WindowFrames);
	return true;
}
bool FHALMovementTimeline::Append(const FHALMovementSnapshot& Value, int32 WindowFrames)
{
	if (!Value.IsValid() || (!IsEmpty() && Epoch != Value.Epoch)) { return false; }
	if (IsEmpty())
	{
		Epoch = Value.Epoch;
		Anchor = Value;
		StartFrame = EndFrame = Value.EffectiveServerFrame;
		return true;
	}
	FHALMovementSnapshot Last;
	Query(EndFrame, Last);
	if (Value.EffectiveServerFrame < EndFrame)
	{
		FHALMovementSnapshot Existing;
		return Query(Value.EffectiveServerFrame, Existing) && Existing.Equals(Value);
	}
	if (Last.Revision == Value.Revision) { return Last.Equals(Value); }
	if (Value.Revision < Last.Revision) { return false; }
	if (Value.EffectiveServerFrame == Last.EffectiveServerFrame)
	{
		if (Changes.IsEmpty()) { Anchor = Value; } else { Changes.Last() = Value; }
	}
	else { Changes.Add(Value); }
	EndFrame = Value.EffectiveServerFrame;
	Trim(WindowFrames);
	return Validate();
}
bool FHALMovementTimeline::Merge(const FHALMovementTimeline& Incoming, int32 WindowFrames)
{
	if (!Incoming.Validate() || Incoming.IsEmpty() || (!IsEmpty() && Incoming.Epoch != Epoch)) { return false; }
	if (IsEmpty()) { *this = Incoming; Trim(WindowFrames); return true; }
	// Never claim knowledge of a gap. Retain the newer complete checkpoint.
	if (int64(Incoming.StartFrame) > int64(EndFrame) + 1 || int64(StartFrame) > int64(Incoming.EndFrame) + 1)
	{
		if (Incoming.EndFrame > EndFrame) { *this = Incoming; Trim(WindowFrames); }
		return true;
	}
	const int32 End = FMath::Max(EndFrame, Incoming.EndFrame);
	const int32 Start = FMath::Max(FMath::Min(StartFrame, Incoming.StartFrame), End - FMath::Clamp(WindowFrames, 1, MaximumEntries - 2) + 1);
	FHALMovementTimeline Result;
	for (int64 Cursor = Start; Cursor <= End; ++Cursor)
	{
		const int32 Frame = int32(Cursor);
		FHALMovementSnapshot A, B;
		const bool bA = Query(Frame, A), bB = Incoming.Query(Frame, B);
		if (!bA && !bB) { return false; }
		if (bA && bB && A.Revision == B.Revision && !A.Equals(B)) { return false; }
		const auto& Value = !bB || (bA && A.Revision >= B.Revision) ? A : B;
		if (Result.IsEmpty()) { Result.Epoch = Value.Epoch; Result.StartFrame = Frame; Result.Anchor = Value; }
		else
		{
			FHALMovementSnapshot Previous;
			Result.Query(Result.EndFrame, Previous);
			if (!Previous.Equals(Value)) { Result.Changes.Add(Value); }
		}
		Result.EndFrame = Frame;
	}
	if (!Result.Validate()) { return false; }
	*this = MoveTemp(Result);
	return true;
}
bool FHALMovementTimeline::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving() && !Validate()) { Ar.SetError(); return false; }
	Ar << FormatVersion;
	if (FormatVersion != CurrentFormat) { Ar.SetError(); return false; }
	uint8 Present = IsEmpty() ? 0 : 1;
	Ar << Present;
	if (Ar.IsLoading() && Present == 0) { *this = {}; return !Ar.IsError(); }
	if (Present == 0) { return Validate() && !Ar.IsError(); }
	if (Present != 1) { Ar.SetError(); return false; }
	Ar << Epoch << StartFrame << EndFrame;
	Anchor.Serialize(Ar);
	uint16 Count = uint16(Changes.Num());
	Ar << Count;
	if (Count >= MaximumEntries) { Ar.SetError(); return false; }
	if (Ar.IsLoading()) { Changes.SetNum(Count); }
	for (auto& Value : Changes) { Value.Serialize(Ar); }
	if (!Validate()) { Ar.SetError(); }
	return !Ar.IsError();
}
