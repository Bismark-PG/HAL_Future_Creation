#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../HALVehicleStatusFXRuntime.h"
#include "../HALGameplayTags.h"
#include "NativeGameplayTags.h"
#include <limits>
#include <type_traits>

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_FX3A_First,"State.Visual.FX3AFixture.First");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_FX3A_Second,"State.Visual.FX3AFixture.Second");
static_assert(!std::is_copy_constructible_v<FHALStatusFXRuntime> && !std::is_move_constructible_v<FHALStatusFXRuntime>);

namespace
{
	FHALStatusFXInstanceSnapshot Snapshot(FAutomationTestBase& T,const FHALStatusFXRuntime& R,FGameplayTag Tag=TAG_FX3A_First)
	{
		FHALStatusFXInstanceSnapshot S; T.TestTrue(TEXT("Expected live instance"),R.GetSnapshot(Tag,S)); return S;
	}
	void Near(FAutomationTestBase& T,const TCHAR* Label,double Actual,double Expected)
	{ T.TestTrue(FString::Printf(TEXT("%s: %.9f == %.9f"),Label,Actual,Expected),FMath::IsNearlyEqual(Actual,Expected,1.e-6)); }
	bool Available(FAutomationTestBase& T,FHALStatusFXRuntime& R,bool Ready,double Now,FGameplayTag Tag=TAG_FX3A_First)
	{ return R.SetAvailable(Snapshot(T,R,Tag).GetHandle(),Ready,Now); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXAggregateTest,"HAL.FutureCreation.StatusFX.Runtime.AggregationAndLeaseLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXAggregateTest::RunTest(const FString&)
{
	for (bool Reverse:{false,true})
	{
		FHALStatusFXRuntime R; const FHALStatusFXTiming Timing{2,4,3}; const auto A=R.AcquireSource(),B=R.AcquireSource();
		TestTrue(TEXT("New sources are valid and distinct"),A.IsValid() && B.IsValid() && !(A==B));
		TestTrue(TEXT("First source submitted"),R.Submit(A,TAG_FX3A_First,Timing,10)); Available(*this,R,true,10);
		R.Advance(11); const auto One=Snapshot(*this,R);
		TestTrue(TEXT("Second source submitted"),R.Submit(B,TAG_FX3A_First,Timing,11));
		R.Submit(A,TAG_FX3A_First,Timing,11); const auto Two=Snapshot(*this,R);
		TestEqual(TEXT("Same tag has one instance"),R.GetInstanceCount(),1);
		TestEqual(TEXT("Duplicate input does not add a source"),Two.SourceCount,2);
		Near(*this,TEXT("Two sources do not double brightness"),Two.Intensity,One.Intensity);
		TestEqual(TEXT("Aggregation preserves instance identity"),Two.InstanceId,One.InstanceId);
		const auto First=Reverse?B:A,Last=Reverse?A:B;
		TestTrue(TEXT("Either first removal is accepted"),R.RemoveSource(First,12));
		const auto Remaining=Snapshot(*this,R); TestEqual(TEXT("Remaining source keeps effect active"),Remaining.Phase,EHALStatusFXPhase::Active);
		Near(*this,TEXT("Refreshing does not restart curve/fade-in"),Remaining.ClockSeconds,2); Near(*this,TEXT("Original fade-in completes"),Remaining.Intensity,1);
		TestFalse(TEXT("Removed lease is not reusable"),R.HasSource(First));
		TestFalse(TEXT("Late duplicate exit cannot affect another source"),R.RemoveSource(First,99));
		TestFalse(TEXT("Late duplicate update cannot revive removed source"),R.Submit(First,TAG_FX3A_First,Timing,99));
		Near(*this,TEXT("Rejected old notifications do not advance time"),Snapshot(*this,R).ClockSeconds,2);
		R.RemoveSource(Last,14); R.Advance(16); Near(*this,TEXT("Last removal fades from one"),Snapshot(*this,R).Intensity,.5);
		R.Advance(21); TestEqual(TEXT("Configured fade plus residue eventually releases"),R.GetInstanceCount(),0);
		TestEqual(TEXT("All leases removed"),R.GetSourceLeaseCount(),0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXReentryTest,"HAL.FutureCreation.StatusFX.Runtime.RefreshFadeAndResidueReentry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXReentryTest::RunTest(const FString&)
{
	FHALStatusFXRuntime R; const FHALStatusFXTiming T{2,4,3}; const auto A=R.AcquireSource();
	R.Submit(A,TAG_FX3A_First,T,0); Available(*this,R,true,0); R.Advance(2); const auto Original=Snapshot(*this,R);
	R.RemoveSource(A,2); R.Advance(3); const auto Fade=Snapshot(*this,R); Near(*this,TEXT("Fade sample"),Fade.Intensity,.75);
	const auto B=R.AcquireSource(); R.Submit(B,TAG_FX3A_First,T,3); const auto Reentered=Snapshot(*this,R);
	TestEqual(TEXT("Fading instance is reused"),Reentered.InstanceId,Original.InstanceId);
	Near(*this,TEXT("Re-entry starts at current visible intensity"),Reentered.Intensity,.75);
	R.Advance(4); R.Submit(B,TAG_FX3A_First,T,4); Near(*this,TEXT("Refresh keeps the re-entry envelope"),Snapshot(*this,R).Intensity,.875);
	R.Advance(5); R.RemoveSource(B,5); R.Advance(9);
	TestEqual(TEXT("Fade ends in residue"),Snapshot(*this,R).Phase,EHALStatusFXPhase::Residue);
	Near(*this,TEXT("Residue does not restore the active brightness"),Snapshot(*this,R).Intensity,0);
	const auto C=R.AcquireSource(); R.Submit(C,TAG_FX3A_First,T,10); const auto ResidueReentry=Snapshot(*this,R);
	TestEqual(TEXT("Residual instance is reused"),ResidueReentry.InstanceId,Original.InstanceId);
	Near(*this,TEXT("Curve clock survives fade and residue re-entry"),ResidueReentry.ClockSeconds,10);
	R.Advance(11); Near(*this,TEXT("Residue re-entry fades in from zero"),Snapshot(*this,R).Intensity,.5);
	R.RemoveSource(C,11); R.Advance(18); TestEqual(TEXT("Fully ended instance is released"),R.GetInstanceCount(),0);
	const auto D=R.AcquireSource(); R.Submit(D,TAG_FX3A_First,T,18); const auto Fresh=Snapshot(*this,R);
	TestTrue(TEXT("After full release a new instance is allocated"),Fresh.InstanceId!=Original.InstanceId);
	Near(*this,TEXT("Only the new instance resets its curve clock"),Fresh.ClockSeconds,0);
	TestFalse(TEXT("Old availability completion cannot activate a replacement"),R.SetAvailable(Original.GetHandle(),true,18));
	TestEqual(TEXT("New instance still waits for its own readiness"),Snapshot(*this,R).Phase,EHALStatusFXPhase::Waiting);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXWaitingTest,"HAL.FutureCreation.StatusFX.Runtime.WaitingAvailabilityAndCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXWaitingTest::RunTest(const FString&)
{
	FHALStatusFXRuntime R; const FHALStatusFXTiming T{2,4,3}; const auto A=R.AcquireSource(); R.Submit(A,TAG_FX3A_First,T,0);
	TestFalse(TEXT("Waiting needs no phase tick"),R.NeedsPhaseUpdate()); R.Advance(5);
	const auto Waiting=Snapshot(*this,R); Near(*this,TEXT("Waiting has no visible intensity"),Waiting.Intensity,0);
	Available(*this,R,true,5); const auto Ready=Snapshot(*this,R);
	Near(*this,TEXT("Late availability joins the continuing segment"),Ready.Intensity,1);
	Near(*this,TEXT("Waiting did not reset the clock"),Ready.ClockSeconds,5);
	Available(*this,R,false,6); R.Advance(7); Available(*this,R,true,7);
	TestEqual(TEXT("Temporary target loss does not replace instance"),Snapshot(*this,R).InstanceId,Waiting.InstanceId);
	Available(*this,R,false,8); R.RemoveSource(A,8);
	TestEqual(TEXT("Demand withdrawn while waiting has no delayed tail"),R.GetInstanceCount(),0);
	TestFalse(TEXT("Late resources cannot resurrect withdrawn demand"),R.SetAvailable(Waiting.GetHandle(),true,20));
	const auto B=R.AcquireSource(); R.Submit(B,TAG_FX3A_First,T,11); Available(*this,R,true,11);
	R.Advance(13); const auto Tail=Snapshot(*this,R); R.RemoveSource(B,13); Available(*this,R,false,14);
	TestEqual(TEXT("Existing tail keeps its phase while unavailable"),Snapshot(*this,R).Phase,EHALStatusFXPhase::Fading);
	Near(*this,TEXT("Unavailable tail is not visible"),Snapshot(*this,R).Intensity,0);
	Available(*this,R,true,15); Near(*this,TEXT("Rebound target gets current tail, not a replay"),Snapshot(*this,R).Intensity,.5);
	Available(*this,R,false,15.5); const auto C=R.AcquireSource(); R.Submit(C,TAG_FX3A_First,T,16);
	TestEqual(TEXT("Unavailable re-entry retains instance identity"),Snapshot(*this,R).InstanceId,Tail.InstanceId);
	Available(*this,R,true,17); Near(*this,TEXT("Re-entry envelope also advances while waiting"),Snapshot(*this,R).Intensity,.5);
	R.RemoveSource(C,17); Available(*this,R,false,18); R.Advance(24);
	TestEqual(TEXT("Unavailable tails expire without a target callback"),R.GetInstanceCount(),0);
	TestFalse(TEXT("Expired tail cannot be recreated"),R.SetAvailable(Tail.GetHandle(),true,24));
	const auto Pending=R.AcquireSource(),Other=R.AcquireSource();
	R.Submit(Other,TAG_FX3A_Second,T,30); Available(*this,R,true,30,TAG_FX3A_Second); R.Advance(35);
	TestTrue(TEXT("Late recipe seeds original observation age without rewinding the model"),R.Submit(Pending,TAG_FX3A_First,T,35,9));
	Available(*this,R,true,35); Near(*this,TEXT("Late recipe keeps first-observation curve age"),Snapshot(*this,R).ClockSeconds,9);
	Near(*this,TEXT("Late recipe joins its sustained segment"),Snapshot(*this,R).Intensity,1);
	Near(*this,TEXT("Other tag clock is not rewound"),Snapshot(*this,R,TAG_FX3A_Second).ClockSeconds,5);
	R.Submit(Pending,TAG_FX3A_First,T,36,20);
	Near(*this,TEXT("Age hint cannot overwrite a live clock"),Snapshot(*this,R).ClockSeconds,10);
	const auto Withdrawn=R.AcquireSource(); R.RemoveSource(Withdrawn,36);
	TestFalse(TEXT("Recipe arriving after pending demand ended is ignored"),R.Submit(Withdrawn,TAG_FX3A_First,T,40,10));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXZeroTest,"HAL.FutureCreation.StatusFX.Runtime.ZeroDurationAndSkippedPhases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXZeroTest::RunTest(const FString&)
{
	FHALStatusFXRuntime R; auto A=R.AcquireSource(); R.Submit(A,TAG_FX3A_First,{},0); Available(*this,R,true,0);
	Near(*this,TEXT("Zero fade-in is immediately active"),Snapshot(*this,R).Intensity,1);
	TestFalse(TEXT("Stable active state does not need phase updates"),R.NeedsPhaseUpdate()); R.RemoveSource(A,0);
	TestEqual(TEXT("Zero fade and residue leave no phantom instance"),R.GetInstanceCount(),0);
	A=R.AcquireSource(); R.Submit(A,TAG_FX3A_First,{0,0,2},10); Available(*this,R,true,10); R.RemoveSource(A,10);
	TestEqual(TEXT("Zero fade enters residue in the same call"),Snapshot(*this,R).Phase,EHALStatusFXPhase::Residue);
	R.Advance(12); TestEqual(TEXT("Exact residue boundary releases"),R.GetInstanceCount(),0);
	A=R.AcquireSource(); R.Submit(A,TAG_FX3A_First,{0,2,3},20); Available(*this,R,true,20); R.RemoveSource(A,20);
	R.Advance(25); TestEqual(TEXT("One large advance can cross all exit phases"),R.GetInstanceCount(),0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXBindingTest,"HAL.FutureCreation.StatusFX.Runtime.BindingAndVehicleIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXBindingTest::RunTest(const FString&)
{
	FHALStatusFXRuntime A,B; const auto SA=A.AcquireSource(),SB=B.AcquireSource(); const FHALStatusFXTiming T{0,2,0};
	A.Submit(SA,TAG_FX3A_First,T,0); B.Submit(SB,TAG_FX3A_First,T,0); Available(*this,A,true,0); Available(*this,B,true,0);
	const auto Old=Snapshot(*this,A);
	TestFalse(TEXT("A source is not transferable to another vehicle"),B.RemoveSource(SA,99));
	Near(*this,TEXT("Foreign notifications do not advance the other clock"),Snapshot(*this,B).ClockSeconds,0);
	A.ResetBinding(); const auto New=A.AcquireSource(); A.Submit(New,TAG_FX3A_First,T,0);
	TestFalse(TEXT("Old source cannot close new binding"),A.RemoveSource(SA,0));
	TestFalse(TEXT("Old readiness completion cannot enter new binding"),A.SetAvailable(Old.GetHandle(),true,0));
	TestTrue(TEXT("Binding identity changes even when local counters restart"),Snapshot(*this,A).Binding!=Old.Binding);
	Available(*this,A,true,0); A.RemoveSource(New,0); A.Advance(2);
	TestEqual(TEXT("Clearing A cannot clear B"),B.GetInstanceCount(),1);
	TestTrue(TEXT("B source remains live"),B.HasSource(SB));
	A.ResetBinding(); TestEqual(TEXT("Reset clears pending leases too"),A.GetSourceLeaseCount(),0);
	TestFalse(TEXT("Reset has no phase work"),A.NeedsPhaseUpdate());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXInvalidTest,"HAL.FutureCreation.StatusFX.Runtime.RejectedInputIsAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXInvalidTest::RunTest(const FString&)
{
	FHALStatusFXRuntime R,Other; const FHALStatusFXTiming T{2,4,3}; const auto A=R.AcquireSource(),B=R.AcquireSource();
	R.Submit(A,TAG_FX3A_First,T,5); Available(*this,R,true,5); R.Advance(6); const auto Before=Snapshot(*this,R);
	TestFalse(TEXT("Foreign source rejected"),R.Submit(Other.AcquireSource(),TAG_FX3A_First,T,100));
	TestFalse(TEXT("Nonvisual gameplay tag rejected"),R.Submit(B,HALGameplayTags::State_Movement_Boost,T,100));
	TestFalse(TEXT("Visual root is not an effect tag"),R.Submit(B,FGameplayTag::RequestGameplayTag(TEXT("State.Visual")),T,100));
	TestFalse(TEXT("Empty tag rejected"),R.Submit(B,{},T,100));
	TestFalse(TEXT("One lease cannot change its tag"),R.Submit(A,TAG_FX3A_Second,T,100));
	TestFalse(TEXT("Conflicting timing cannot overwrite a live recipe"),R.Submit(B,TAG_FX3A_First,{2,5,3},100));
	TestFalse(TEXT("Negative timing rejected"),R.Submit(B,TAG_FX3A_First,{-1,4,3},100));
	const double NaN=std::numeric_limits<double>::quiet_NaN(),Inf=std::numeric_limits<double>::infinity();
	TestFalse(TEXT("NaN clock rejected"),R.Advance(NaN)); TestFalse(TEXT("Infinite clock rejected"),R.Advance(Inf));
	TestFalse(TEXT("Negative clock rejected"),R.Advance(-1)); TestFalse(TEXT("Backward clock rejected"),R.Advance(5));
	TestFalse(TEXT("Negative age seed rejected"),R.Submit(B,TAG_FX3A_First,T,6,-1));
	TestFalse(TEXT("Age seed cannot predate the local time domain"),R.Submit(B,TAG_FX3A_First,T,6,7));
	TestFalse(TEXT("Nonfinite age seed rejected"),R.Submit(B,TAG_FX3A_First,T,6,NaN));
	TestFalse(TEXT("Nonfinite timing rejected"),R.Submit(B,TAG_FX3A_First,{0,std::numeric_limits<float>::infinity(),0},100));
	const auto After=Snapshot(*this,R); Near(*this,TEXT("Rejected actions leave clock untouched"),After.ClockSeconds,Before.ClockSeconds);
	Near(*this,TEXT("Rejected actions leave intensity untouched"),After.Intensity,Before.Intensity);
	TestEqual(TEXT("No stray second-tag instance"),R.GetInstanceCount(),1);
	TestTrue(TEXT("Unused lease can be cancelled"),R.RemoveSource(B,6));
	TestEqual(TEXT("Only submitted source counts toward existence"),R.GetActiveSourceCount(),1);
	R.RemoveSource(A,7); TestFalse(TEXT("Late old removal does not silently advance tail to expiry"),R.RemoveSource(A,100));
	TestEqual(TEXT("Tail still exists at last accepted time"),R.GetInstanceCount(),1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXIndependentTest,"HAL.FutureCreation.StatusFX.Runtime.IndependentTagsAndDeterministicSnapshots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXIndependentTest::RunTest(const FString&)
{
	FHALStatusFXRuntime R; const auto A=R.AcquireSource(),B=R.AcquireSource();
	R.Submit(A,TAG_FX3A_Second,{0,1,0},2); Available(*this,R,true,2,TAG_FX3A_Second);
	R.Submit(B,TAG_FX3A_First,{4,2,0},4); Available(*this,R,true,4); R.Advance(6);
	Near(*this,TEXT("First tag has its own later clock"),Snapshot(*this,R).ClockSeconds,2);
	Near(*this,TEXT("Second tag has original clock"),Snapshot(*this,R,TAG_FX3A_Second).ClockSeconds,4);
	Near(*this,TEXT("First fade-in progresses independently"),Snapshot(*this,R).Intensity,.5);
	Near(*this,TEXT("Second stable intensity unchanged"),Snapshot(*this,R,TAG_FX3A_Second).Intensity,1);
	const auto Snapshots=R.GetSnapshots(); TestEqual(TEXT("Two independent tags"),Snapshots.Num(),2);
	if (Snapshots.Num()!=2) { return false; }
	TestEqual(TEXT("Snapshot order does not depend on insertion order"),Snapshots[0].Tag,FGameplayTag(TAG_FX3A_First));
	R.RemoveSource(A,6); R.Advance(7);
	TestEqual(TEXT("Ending second tag does not stop first"),R.GetInstanceCount(),1);
	TestTrue(TEXT("First source still active"),R.HasSource(B));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXSchedulingTest,"HAL.FutureCreation.StatusFX.Runtime.PhaseSchedulingAndTimePartition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXSchedulingTest::RunTest(const FString&)
{
	FHALStatusFXRuntime A,B; const FHALStatusFXTiming T{2,4,3}; const double Start=1.e9;
	const auto SA=A.AcquireSource(),SB=B.AcquireSource();
	A.Submit(SA,TAG_FX3A_First,T,Start); B.Submit(SB,TAG_FX3A_First,T,Start);
	TestFalse(TEXT("Unavailable input has no periodic phase work"),A.NeedsPhaseUpdate());
	Available(*this,A,true,Start); Available(*this,B,true,Start); TestTrue(TEXT("Fade-in requests phase work"),A.NeedsPhaseUpdate());
	A.Advance(Start+2); for (int32 I=1;I<=120;++I) { B.Advance(Start+double(I)/60); }
	Near(*this,TEXT("Partition-independent envelope"),Snapshot(*this,A).Intensity,Snapshot(*this,B).Intensity);
	TestFalse(TEXT("Stable active phase can stop updating"),A.NeedsPhaseUpdate());
	A.RemoveSource(SA,Start+2); B.RemoveSource(SB,Start+2); TestTrue(TEXT("Fade-out needs updates"),A.NeedsPhaseUpdate());
	A.Advance(Start+7); for (int32 I=1;I<=300;++I) { B.Advance(Start+2+double(I)/60); }
	TestEqual(TEXT("Both skip to the same residual phase"),Snapshot(*this,A).Phase,Snapshot(*this,B).Phase);
	Near(*this,TEXT("Residual phase age is not polling-dependent"),Snapshot(*this,A).PhaseSeconds,1);
	TestTrue(TEXT("Residue needs a cleanup boundary"),A.NeedsPhaseUpdate());
	A.Advance(Start+9); B.Advance(Start+9); TestFalse(TEXT("Idle runtime stops phase work"),A.NeedsPhaseUpdate());
	TestEqual(TEXT("Both runtimes fully release"),B.GetInstanceCount(),0);
	return true;
}
#endif
