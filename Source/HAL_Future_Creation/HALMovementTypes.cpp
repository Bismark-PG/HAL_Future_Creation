#include "HALMovementTypes.h"
#include "Serialization/Archive.h"

bool FHALMovementSnapshot::IsValidValues() const
{
	return Epoch.IsValid() && Revision != 0 && FMath::IsFinite(AccelerationScale) && FMath::IsFinite(SpeedScale)
		&& AccelerationScale >= 0 && AccelerationScale <= 3 && SpeedScale >= 0 && SpeedScale <= 3;
}
bool FHALMovementSnapshot::IsUnset() const
{
	return !Epoch.IsValid() && Revision == 0 && EffectiveServerFrame == INDEX_NONE && AccelerationScale == 1 && SpeedScale == 1;
}
bool FHALMovementSnapshot::Equals(const FHALMovementSnapshot& Other) const
{
	return Epoch == Other.Epoch && Revision == Other.Revision && EffectiveServerFrame == Other.EffectiveServerFrame
		&& FMath::IsNearlyEqual(AccelerationScale, Other.AccelerationScale, 1.e-4f)
		&& FMath::IsNearlyEqual(SpeedScale, Other.SpeedScale, 1.e-4f);
}
void FHALMovementSnapshot::Serialize(FArchive& Ar)
{
	Ar << Epoch << Revision << EffectiveServerFrame << AccelerationScale << SpeedScale;
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
	if (FormatVersion != 1 || Changes.Num() >= MaximumEntries) { return false; }
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
	if (FormatVersion != 1) { Ar.SetError(); return false; }
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
