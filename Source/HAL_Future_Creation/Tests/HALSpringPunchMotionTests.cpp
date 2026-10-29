#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../SpringPunchGimmick.h"
#include "Curves/CurveFloat.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSpringPunchMotionTest, "HAL.FutureCreation.SpringPunch.AnnouncedMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALSpringPunchMotionTest::RunTest(const FString& Parameters)
{
	auto* Extension = NewObject<UCurveFloat>();
	Extension->FloatCurve.AddKey(0, 0);
	Extension->FloatCurve.AddKey(.5f, .9f);
	Extension->FloatCurve.AddKey(1, 1);
	auto* Retraction = NewObject<UCurveFloat>();
	Retraction->FloatCurve.AddKey(0, 0);
	Retraction->FloatCurve.AddKey(.5f, .25f);
	Retraction->FloatCurve.AddKey(1, 1);
	FSpringPunchMotion Motion;
	Motion.bValid = true;
	Motion.StartsAt = 10;
	Motion.ExtendSeconds = .25f;
	Motion.RetractSeconds = .7f;
	Motion.ExtensionCurve = Extension;
	Motion.RetractionCurve = Retraction;
	TestEqual(TEXT("Windup stays at origin"), Motion.EvaluateOffset(9.9, 400), 0.f);
	TestEqual(TEXT("Extension starts exactly at origin"), Motion.EvaluateOffset(10, 400), 0.f);
	TestTrue(TEXT("Announced nonlinear extension midpoint"), FMath::IsNearlyEqual(Motion.EvaluateOffset(10.125, 400), 360.f, .01f));
	TestEqual(TEXT("Boundary reaches full extension without a new packet"), Motion.EvaluateOffset(10.25, 400), 400.f);
	TestTrue(TEXT("Retraction uses its own curve without a new packet"), FMath::IsNearlyEqual(Motion.EvaluateOffset(10.60, 400), 300.f, .01f));
	TestEqual(TEXT("Dropped phase packets still reach rest"), Motion.EvaluateOffset(12, 400), 0.f);
	// A late observer samples the same absolute schedule; arrival time never restarts motion.
	const FSpringPunchMotion LateSnapshot = Motion;
	TestEqual(TEXT("Late snapshot preserves progress"), LateSnapshot.EvaluateOffset(10.60, 400), Motion.EvaluateOffset(10.60, 400));
	Motion.ExtensionCurve = nullptr;
	Motion.RetractionCurve = nullptr;
	TestTrue(TEXT("Unassigned extension stays linear"), FMath::IsNearlyEqual(Motion.EvaluateOffset(10.125, 400), 200.f, .01f));
	TestTrue(TEXT("Unassigned return stays linear"), FMath::IsNearlyEqual(Motion.EvaluateOffset(10.60, 400), 200.f, .01f));
	FSpringPunchCycle Cycle;
	Cycle.bValid = true;
	Cycle.Motion = Motion;
	Cycle.Motion.ExtensionCurve = Extension;
	Cycle.Motion.RetractionCurve = Retraction;
	Cycle.WindupSeconds = .4f;
	Cycle.CooldownSeconds = .35f;
	Cycle.Distance = 400;
	for (int32 Index : {0, 1, 5, 1000})
	{
		const double Start = 10 + Index * Cycle.Period();
		TestTrue(TEXT("Whole missing cycles preserve nonlinear extension"), FMath::IsNearlyEqual(Cycle.EvaluateOffset(Start+.125),360.f,.01f));
		TestTrue(TEXT("Whole missing cycles preserve independent return"), FMath::IsNearlyEqual(Cycle.EvaluateOffset(Start+.60),300.f,.01f));
		TestEqual(TEXT("Cooldown stays at rest"), Cycle.EvaluateOffset(Start+1.1),0.f);
		TestEqual(TEXT("Derived extension phase"), Cycle.EvaluateState(Start+.125),ESpringPunchState::Extending);
		TestEqual(TEXT("Derived return phase"), Cycle.EvaluateState(Start+.60),ESpringPunchState::Retracting);
		TestEqual(TEXT("Derived cooldown phase"), Cycle.EvaluateState(Start+1.1),ESpringPunchState::Cooldown);
		TestEqual(TEXT("Derived next windup"), Cycle.EvaluateState(Start+1.4),ESpringPunchState::Windup);
	}
	TestEqual(TEXT("Initial delay stays idle"), Cycle.EvaluateState(9),ESpringPunchState::Idle);
	TestEqual(TEXT("First windup reconstructed"), Cycle.EvaluateState(9.8),ESpringPunchState::Windup);
	Cycle.bValid = false;
	TestEqual(TEXT("Inactive cycle stays still"), Cycle.EvaluateOffset(10.125),0.f);
	Motion.bValid = false;
	TestEqual(TEXT("Inactive schedule cannot move"), Motion.EvaluateOffset(10.125, 400), 0.f);
	return true;
}
#endif
