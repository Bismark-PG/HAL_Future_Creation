#include "HALVehicleStatusFXRuntime.h"

bool FHALStatusFXTiming::IsValid() const
{
	return FMath::IsFinite(FadeInSeconds) && FadeInSeconds>=0 && FMath::IsFinite(FadeOutSeconds)
		&& FadeOutSeconds>=0 && FMath::IsFinite(ResidueSeconds) && ResidueSeconds>=0;
}
bool FHALStatusFXTiming::operator==(const FHALStatusFXTiming& Other) const
{ return FadeInSeconds==Other.FadeInSeconds && FadeOutSeconds==Other.FadeOutSeconds && ResidueSeconds==Other.ResidueSeconds; }

FHALStatusFXRuntime::FHALStatusFXRuntime() { ResetBinding(); }
void FHALStatusFXRuntime::ResetBinding()
{
	Binding=FGuid::NewGuid(); NextSourceId=NextInstanceId=1; Time=0; Sources.Reset(); Instances.Reset();
}
FHALStatusFXSourceToken FHALStatusFXRuntime::AcquireSource()
{
	FHALStatusFXSourceToken Result;
	if (NextSourceId==MAX_uint64) { return Result; }
	Result.Binding=Binding; Result.Id=NextSourceId++; Sources.Add(Result.Id,FGameplayTag{}); return Result;
}
bool FHALStatusFXRuntime::HasSource(const FHALStatusFXSourceToken& Source) const
{ return Source.IsValid() && Source.Binding==Binding && Sources.Contains(Source.Id); }
bool FHALStatusFXRuntime::AcceptsTime(double Now) const
{ return FMath::IsFinite(Now) && Now>=Time; }
bool FHALStatusFXRuntime::IsAliveAt(const FInstance& I,double Now)
{ return I.Sources>0 || Now-I.ReleasedAt<double(I.Timing.FadeOutSeconds)+double(I.Timing.ResidueSeconds); }
float FHALStatusFXRuntime::EnvelopeAt(const FInstance& I,double Now)
{
	if (I.Phase==EHALStatusFXPhase::Residue) { return 0; }
	if (I.Phase==EHALStatusFXPhase::Fading)
	{
		const double Alpha=I.Timing.FadeOutSeconds>0?FMath::Clamp((Now-I.ReleasedAt)/I.Timing.FadeOutSeconds,0.0,1.0):1;
		return I.ReleaseIntensity*float(1-Alpha);
	}
	const double Alpha=I.Timing.FadeInSeconds>0?FMath::Clamp((Now-I.EnvelopeAt)/I.Timing.FadeInSeconds,0.0,1.0):1;
	return FMath::Lerp(I.EnvelopeFrom,1.f,float(Alpha));
}
bool FHALStatusFXRuntime::Advance(double Now)
{
	if (!AcceptsTime(Now)) { return false; }
	Time=Now;
	for (auto It=Instances.CreateIterator();It;++It)
	{
		auto& I=It.Value();
		if (!IsAliveAt(I,Now)) { It.RemoveCurrent(); }
		else if (I.Sources==0 && Now-I.ReleasedAt>=I.Timing.FadeOutSeconds) { I.Phase=EHALStatusFXPhase::Residue; }
	}
	return true;
}
bool FHALStatusFXRuntime::Submit(const FHALStatusFXSourceToken& Source,FGameplayTag Tag,const FHALStatusFXTiming& Timing,double Now,double InitialClockSeconds)
{
	const auto Root=FGameplayTag::RequestGameplayTag(TEXT("State.Visual"),false);
	if (!HasSource(Source) || !AcceptsTime(Now) || !Timing.IsValid() || !Tag.IsValid()
		|| !Root.IsValid() || Tag==Root || !Tag.MatchesTag(Root) || !FMath::IsFinite(InitialClockSeconds)
		|| InitialClockSeconds<0 || InitialClockSeconds>Now) { return false; }
	auto* SourceTag=Sources.Find(Source.Id);
	if (SourceTag->IsValid() && *SourceTag!=Tag) { return false; }
	const auto* Previous=Instances.Find(Tag);
	const bool Exists=Previous && IsAliveAt(*Previous,Now);
	if ((Exists && !(Previous->Timing==Timing)) || (!Exists && NextInstanceId==MAX_uint64)
		|| (Exists && !SourceTag->IsValid() && Previous->Sources==MAX_int32)) { return false; }
	// Validate before advancing: rejected input cannot expire tails or change another instance's clock.
	Advance(Now);
	auto* I=Instances.Find(Tag);
	if (!I)
	{
		FInstance New; New.Id=NextInstanceId++; New.Timing=Timing;
		New.StartedAt=New.PhaseStartedAt=New.EnvelopeAt=Now-InitialClockSeconds;
		I=&Instances.Add(Tag,New);
	}
	if (!SourceTag->IsValid())
	{
		if (I->Sources==0 && I->Phase!=EHALStatusFXPhase::Waiting)
		{
			I->EnvelopeFrom=I->Available?EnvelopeAt(*I,Now):0; I->EnvelopeAt=I->PhaseStartedAt=Now;
			I->Phase=I->Available?EHALStatusFXPhase::Active:EHALStatusFXPhase::Waiting;
		}
		*SourceTag=Tag; ++I->Sources;
	}
	return true;
}
bool FHALStatusFXRuntime::RemoveSource(const FHALStatusFXSourceToken& Source,double Now)
{
	if (!HasSource(Source) || !AcceptsTime(Now)) { return false; }
	const auto Tag=Sources.FindChecked(Source.Id);
	Advance(Now); Sources.Remove(Source.Id);
	if (!Tag.IsValid()) { return true; } // Unsubmitted leases can be cancelled too.
	auto& I=Instances.FindChecked(Tag);
	if (--I.Sources>0) { return true; }
	if (I.Phase==EHALStatusFXPhase::Waiting) { Instances.Remove(Tag); return true; }
	I.ReleaseIntensity=EnvelopeAt(I,Now); I.ReleasedAt=I.PhaseStartedAt=Now; I.Phase=EHALStatusFXPhase::Fading;
	Advance(Now); return true; // Handles zero-length fade/residue without a phantom frame.
}
bool FHALStatusFXRuntime::SetAvailable(const FHALStatusFXInstanceHandle& Expected,bool Available,double Now)
{
	const auto* Previous=Instances.Find(Expected.Tag);
	if (!AcceptsTime(Now) || Expected.Binding!=Binding || !Previous || Previous->Id!=Expected.Id || !IsAliveAt(*Previous,Now)) { return false; }
	Advance(Now); auto& I=Instances.FindChecked(Expected.Tag);
	if (I.Available!=Available && I.Sources>0)
	{ I.Phase=Available?EHALStatusFXPhase::Active:EHALStatusFXPhase::Waiting; I.PhaseStartedAt=Now; }
	I.Available=Available;
	return true;
}
int32 FHALStatusFXRuntime::GetActiveSourceCount() const
{
	int32 Count=0; for (const auto& Entry:Sources) { Count+=Entry.Value.IsValid()?1:0; } return Count;
}
bool FHALStatusFXRuntime::GetSnapshot(FGameplayTag Tag,FHALStatusFXInstanceSnapshot& Out) const
{
	const auto* I=Instances.Find(Tag); Out={}; if (!I) { return false; }
	Out.Tag=Tag; Out.Binding=Binding; Out.InstanceId=I->Id; Out.Phase=I->Phase; Out.SourceCount=I->Sources; Out.bAvailable=I->Available;
	Out.ClockSeconds=Time-I->StartedAt;
	Out.PhaseSeconds=I->Phase==EHALStatusFXPhase::Residue?(Time-I->ReleasedAt)-I->Timing.FadeOutSeconds:Time-I->PhaseStartedAt;
	Out.Intensity=I->Available?EnvelopeAt(*I,Time):0;
	return true;
}
TArray<FHALStatusFXInstanceSnapshot> FHALStatusFXRuntime::GetSnapshots() const
{
	TArray<FHALStatusFXInstanceSnapshot> Result;
	for (const auto& Entry:Instances) { FHALStatusFXInstanceSnapshot S; GetSnapshot(Entry.Key,S); Result.Add(S); }
	Result.Sort([](const auto& A,const auto& B){ return A.Tag.ToString()<B.Tag.ToString(); }); return Result;
}
bool FHALStatusFXRuntime::NeedsPhaseUpdate() const
{
	for (const auto& Entry:Instances)
	{
		const auto& I=Entry.Value;
		if (I.Phase==EHALStatusFXPhase::Fading || I.Phase==EHALStatusFXPhase::Residue
			|| (I.Phase==EHALStatusFXPhase::Active && Time-I.EnvelopeAt<I.Timing.FadeInSeconds)) { return true; }
	}
	return false;
}
