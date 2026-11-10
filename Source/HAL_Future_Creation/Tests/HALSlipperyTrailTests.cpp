#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../HALSlipperyTrailAbility.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSlipperyTrailGroundMeshTest, "HAL.FutureCreation.SlipperyTrail.GroundMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALSlipperyTrailGroundMeshTest::RunTest(const FString&)
{
	FHALSlipperyTrailShape Shape;
	Shape.InitialHalfWidth = 30;
	Shape.MaximumHalfWidth = 90;
	Shape.ExpansionSeconds = 2;
	FHALSlipperyTrailPresentation Look;
	TArray<FHALSlipperyTrailPoint> Path;
	Path.SetNum(3);
	Path[0].Ground = FVector(1000, 2000, 100);
	Path[0].GroundNormal = FVector(0, -.25, 1).GetSafeNormal();
	Path[1].Ground = Path[0].Ground + FVector(300, 0, 0);
	Path[1].GroundNormal = Path[0].GroundNormal;
	Path[1].EmissionAge = 1;
	Path[1].bLinked = true;
	Path[2].Ground = Path[0].Ground + FVector(650, 0, 0);
	Path[2].EmissionAge = 1.5f;

	const auto First = AHALSlipperyTrail::BuildGroundMesh(Path, Shape, Look, 0);
	TestEqual(TEXT("Stationary cast makes a single rounded patch"), First.TriangleCount(), 24);
	TestTrue(TEXT("Future samples do not draw ahead of the synchronized clock"),
		FMath::IsNearlyEqual(First.GetBounds().Max.X, 30., .001));

	const auto Mesh = AHALSlipperyTrail::BuildGroundMesh(Path, Shape, Look, 1.5f);
	TestEqual(TEXT("A linked strip and a disconnected patch form two regions"), Mesh.TriangleCount(), 50);
	const auto* UV = Mesh.Attributes()->PrimaryUV();
	for (const int32 Triangle : Mesh.TriangleIndicesItr())
	{
		const auto Vertices = Mesh.GetTriangle(Triangle);
		const auto Elements = UV->GetTriangle(Triangle);
		TestTrue(TEXT("Face points up for the top-down view"), Mesh.GetTriNormal(Triangle).Z > 0);
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			const FVector P = Mesh.GetVertex(Vertices[Corner]);
			TestTrue(TEXT("UV0 is fixed in ground units as the shape expands"),
				UV->GetElement(Elements[Corner]).Equals(FVector2f(P.X / Look.TextureTileSize, P.Y / Look.TextureTileSize), .0001f));
			TestTrue(TEXT("No triangles bridge the airborne gap"), P.X <= 345.001 || P.X >= 619.999);
			if (P.X <= 345.001)
			{
				TestTrue(TEXT("Mesh follows the sampled cross-slope"), FMath::IsNearlyEqual(P.Z, P.Y * .25 + Look.GroundLift, .001));
			}
		}
	}
	TestTrue(TEXT("Older point reaches a larger width than newer point"),
		FMath::IsNearlyEqual(Mesh.GetVertex(1).Y, 75., .001));
	const auto Full = AHALSlipperyTrail::BuildGroundMesh(Path, Shape, Look, 4);
	TestTrue(TEXT("Width clamps at the authored maximum"), FMath::IsNearlyEqual(Full.GetBounds().Max.Y, 90., .001));
	TestEqual(TEXT("Whole mesh expires on the area's clock"),
		AHALSlipperyTrail::BuildGroundMesh(Path, Shape, Look, Shape.LifetimeSeconds).TriangleCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSlipperyTrailFootprintTest, "HAL.FutureCreation.SlipperyTrail.FootprintAndSweep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALSlipperyTrailFootprintTest::RunTest(const FString&)
{
	auto* Config = NewObject<UHALSlipperyTrailConfig>();
	FString Error;
	TestTrue(TEXT("Authored defaults are valid"), Config->Validate(Error));
	Config->Shape.LifetimeSeconds = Config->Emission.EmissionSeconds;
	TestFalse(TEXT("A residual lifetime is required"), Config->Validate(Error));

	FHALSlipperyTrailShape Shape;
	Shape.InitialHalfWidth = 30;
	Shape.MaximumHalfWidth = 100;
	Shape.ExpansionSeconds = 2;
	Shape.GroundHeightTolerance = 20;
	TArray<FHALSlipperyTrailPoint> Path;
	auto& Start = Path.AddDefaulted_GetRef();
	Start.Ground = FVector::ZeroVector;
	const auto Hit = [&](float Age, const FVector& Point) { return AHALSlipperyTrail::Intersects(Path, Shape, Age, Point, Point); };
	TestTrue(TEXT("Stationary cast starts with a round patch"), Hit(0, FVector(0, 25, 0)));
	TestFalse(TEXT("New patch does not start at max width"), Hit(0, FVector(0, 80, 0)));
	TestTrue(TEXT("Previously laid ground expands in place"), Hit(2, FVector(0, 80, 0)));
	TestFalse(TEXT("Expansion is capped"), Hit(5, FVector(0, 101, 0)));
	auto& End = Path.AddDefaulted_GetRef();
	End.Ground = FVector(300, 0, 15);
	End.EmissionAge = 2;
	End.bLinked = true;
	TestTrue(TEXT("A linked segment includes the tapered middle"), Hit(2, FVector(150, 61, 7)));
	TestFalse(TEXT("The wider end does not determine all widths"), Hit(2, FVector(230, 60, 10)));
	TestFalse(TEXT("Crossing on another floor does not apply"), Hit(2, FVector(150, 0, 35)));
	TestTrue(TEXT("A fast crossing hits even when both endpoints are outside"), AHALSlipperyTrail::Intersects(
		Path, Shape, 2, FVector(150, -135, 10), FVector(150, 135, 10)));
	TestFalse(TEXT("The entire path expires together"), Hit(Shape.LifetimeSeconds, FVector::ZeroVector));
	Path.Last().bLinked = false;
	TestFalse(TEXT("Unlinked landing leaves the intervening gap clear"), Hit(2, FVector(150, 0, 7)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSlipperyTrailSwayTest, "HAL.FutureCreation.SlipperyTrail.SingleSwayAndReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALSlipperyTrailSwayTest::RunTest(const FString&)
{
	FHALMovementAreaConfig TrailA, TrailB, Ice;
	TrailA.Kind = TrailB.Kind = Ice.Kind = EHALMovementAreaKind::Slippery;
	TrailA.LateralGripMultiplier = .3f;
	TrailB.LateralGripMultiplier = .4f;
	TrailA.Sway.Amplitude = 3.f;
	TrailB.Sway.Amplitude = 5.f;
	TrailB.Sway.PeriodSeconds = .8f;
	TrailB.Sway.RampSeconds = .1f;
	FHALMovementAreaSelection Selection;
	Selection.Include(TrailB); Selection.Include(Ice); Selection.Include(TrailA);
	TestEqual(TEXT("Conventional ice can provide the lowest grip"), Selection.Scales.Z, double(Ice.LateralGripMultiplier));
	TestEqual(TEXT("At most one yaw term wins"), Selection.Sway.Amplitude, 5.f);
	FHALMovementAreaSelection Reverse;
	Reverse.Include(TrailA); Reverse.Include(Ice); Reverse.Include(TrailB);
	TestTrue(TEXT("Sway winner is independent of registration order"), Selection.Sway == Reverse.Sway);

	FHALMovementSnapshot Enter;
	Enter.Epoch = FGuid::NewGuid();
	Enter.Revision = 1;
	Enter.bHasAreaState = true;
	Enter.AreaGrip.Target = float(Selection.Scales.Z);
	Enter.AreaSway = Selection.Sway;
	Enter.Stamp(nullptr, 100, .02f);
	const float FullSpeed = Enter.AreaSway.FullSpeed;
	TestTrue(TEXT("The torque alternates directions at fixed physics frames"),
		Enter.EvaluateSway(110, FullSpeed) > 4.99f && Enter.EvaluateSway(130, FullSpeed) < -4.99f);
	TestEqual(TEXT("No push at rest"), Enter.EvaluateSway(110, 0), 0.f);
	TestTrue(TEXT("A crawling car gets proportionally smaller sway"),
		FMath::IsNearlyEqual(Enter.EvaluateSway(110, .1f * FullSpeed), .5f));
	auto Change = Enter;
	Change.Revision = 2;
	Change.AreaSpeed.OrdinaryDelta = .25f;
	Change.Stamp(&Enter, 115, .02f);
	TestEqual(TEXT("Unrelated revisions keep the original phase"), Change.SwayStartFrame, 100);
	auto Leave = Change;
	Leave.Revision = 3;
	Leave.AreaSway = {};
	Leave.AreaGrip.Target = 1;
	Leave.Stamp(&Change, 132, .02f);
	TestEqual(TEXT("Sway stops at exit"), Leave.EvaluateSway(135, FullSpeed), 0.f);
	TestTrue(TEXT("Grip still recovers after sway stops"), Leave.EvaluateScales(135).Z < 1);

	FHALMovementTimeline Timeline;
	Timeline.Append(Enter, 120);
	Timeline.Append(Change, 120);
	Timeline.Append(Leave, 120);
	Timeline.ExtendTo(150, 120);
	TArray<uint8> Bytes;
	{ FMemoryWriter Writer(Bytes); TestTrue(TEXT("Sway timeline writes"), Timeline.Serialize(Writer)); }
	FHALMovementTimeline Received;
	{ FMemoryReader Reader(Bytes); TestTrue(TEXT("Sway timeline reads"), Received.Serialize(Reader)); }
	FHALMovementSnapshot Replay;
	TestTrue(TEXT("Replay after expiry retains earlier yaw and grip"),
		Received.Query(130, Replay) && Replay.Equals(Change) && Replay.EvaluateSway(130, FullSpeed) < -4.99f);
	TestTrue(TEXT("Wire exit clears sway without clearing the grip curve"),
		Received.Query(135, Replay) && Replay.Equals(Leave));
	return true;
}
#endif
