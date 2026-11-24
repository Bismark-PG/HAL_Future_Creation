#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../HALBallComicFXComponent.h"
#include "../HALBallComicFXSettings.h"
#include "../HALIncomingBallWidget.h"
#include "../HALVehicleDoodleStatusComponent.h"
#include "../HALImpactBurstSettings.h"
#include "../HALVehiclePresentationTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include <limits>

namespace
{
	FBallRepState ComicState(EBasicBallState State, uint32 Sequence, AActor* Holder = nullptr, int32 LauncherId = INDEX_NONE)
	{
		FBallRepState Out;
		Out.State = State; Out.StateSequence = Sequence; Out.Holder = Holder; Out.LastLauncherPlayerId = LauncherId;
		return Out;
	}
	FHALBallComicFXSeen ComicSeen(const FBallRepState& State)
	{
		FHALBallComicFXSeen Out;
		Out.bValid = true; Out.State = State.State; Out.Sequence = State.StateSequence;
		Out.Holder = State.Holder.Get(); Out.LauncherPlayerId = State.LastLauncherPlayerId;
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBallComicFXTransitionTest, "HAL.FutureCreation.Presentation.ComicDoodle.BallTrailAndAimTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBallComicFXTransitionTest::RunTest(const FString&)
{
	using namespace HALBallComicFX;
	FHALBallComicFXConfig Config;
	FString Error;
	TestTrue(TEXT("Shipped ball comic defaults are valid"), Config.Validate(Error));
	Config.IncomingMaximumArrows = 0; TestFalse(TEXT("Incoming warning needs at least one arrow"), Config.Validate(Error));
	Config = {}; Config.bEnabled = false; Config.IncomingMaximumArrows = 0;
	TestTrue(TEXT("Disabled configuration is never rejected"), Config.Validate(Error));
	Config = {};

	AActor* HolderA = NewObject<AActor>(GetTransientPackage(), AActor::StaticClass(), NAME_None, RF_Transient);
	AActor* HolderB = NewObject<AActor>(GetTransientPackage(), AActor::StaticClass(), NAME_None, RF_Transient);

	// First snapshot ever: a free ball draws nothing.
	FHALBallComicFXTransition T = ResolveTransition({}, ComicState(EBasicBallState::Free, 1));
	TestFalse(TEXT("A free ball starts no layer"), T.bStartTrail || T.bStopTrail || T.bStartAim || T.bStopAim);

	// A late joiner whose first snapshot is already Launched still gets the trail.
	T = ResolveTransition({}, ComicState(EBasicBallState::Launched, 5, nullptr, 2));
	TestTrue(TEXT("First snapshot Launched starts the trail"), T.bStartTrail && !T.bStopTrail);

	const FBallRepState Held = ComicState(EBasicBallState::Controlled, 2, HolderA);
	T = ResolveTransition(ComicSeen(ComicState(EBasicBallState::Free, 1)), Held);
	TestTrue(TEXT("Capture starts the aim arrow only"), T.bStartAim && !T.bStopAim && !T.bStartTrail);
	T = ResolveTransition(ComicSeen(Held), Held);
	TestFalse(TEXT("A repeated snapshot restarts nothing"), T.bStartAim || T.bStopAim || T.bStartTrail || T.bStopTrail);

	const FBallRepState Shot = ComicState(EBasicBallState::Launched, 3, nullptr, 0);
	T = ResolveTransition(ComicSeen(Held), Shot);
	TestTrue(TEXT("Launch hands the arrow over to the trail"), T.bStopAim && !T.bStartAim && T.bStartTrail && !T.bStopTrail);
	T = ResolveTransition(ComicSeen(Shot), ComicState(EBasicBallState::Free, 4));
	TestTrue(TEXT("The trail stops the moment the ball is harmless"), T.bStopTrail && !T.bStartTrail);

	{
		// Triangle trail: level quad behind the ball, tip (+Y of the quad) pointing back along the ground velocity.
		FHALBallComicFXConfig Wedge;
		FTransform Quad;
		float Aspect = 0;
		const FVector Centre(1000, 2000, 160);
		TestTrue(TEXT("A fast ball has a triangle"), TrailWedgeQuad(Wedge, Centre, 160.f, FVector(1500, 0, 0), Quad, Aspect));
		const double Width = 320 * Wedge.TrailWidthDiameterFraction, Length = 1500 * Wedge.TrailWedgeLengthSeconds;
		TestTrue(TEXT("Triangle lies level"), Quad.GetUnitAxis(EAxis::Z).Equals(FVector::UpVector, .001));
		TestTrue(TEXT("Tip points back the way the ball came"), Quad.GetUnitAxis(EAxis::Y).Equals(-FVector::ForwardVector, .001));
		TestTrue(TEXT("Triangle is as wide as configured and as long as the speed says"), Quad.GetScale3D().Equals(FVector(Width * .01, Length * .01, 1), .001));
		TestTrue(TEXT("Base starts behind the ball's centre"), Quad.GetLocation().Equals(Centre - FVector(160 * Wedge.TrailWedgeStartRadiusFraction + .5 * Length, 0, 0), .01));
		TestEqual(TEXT("Aspect is length over width"), Aspect, float(Length / Width), .001f);
		TestTrue(TEXT("A very fast ball stops at the longest triangle"), TrailWedgeQuad(Wedge, Centre, 160.f, FVector(0, 90000, 0), Quad, Aspect)
			&& FMath::IsNearlyEqual(Quad.GetScale3D().Y, 320 * Wedge.TrailWedgeMaxLengthDiameters * .01, .001) && Quad.GetUnitAxis(EAxis::Y).Equals(-FVector::RightVector, .001));
		TestTrue(TEXT("A falling ball is measured along the ground only"), TrailWedgeQuad(Wedge, Centre, 160.f, FVector(1500, 0, -5000), Quad, Aspect)
			&& FMath::IsNearlyEqual(Quad.GetScale3D().Y, Length * .01, .001) && Quad.GetUnitAxis(EAxis::Z).Equals(FVector::UpVector, .001));
		TestTrue(TEXT("A slow shot keeps the shortest triangle"), TrailWedgeQuad(Wedge, Centre, 160.f, FVector(Wedge.TrailWedgeMinSpeed + 10, 0, 0), Quad, Aspect)
			&& FMath::IsNearlyEqual(Quad.GetScale3D().Y, 320 * Wedge.TrailWedgeMinLengthDiameters * .01, .001));
		TestFalse(TEXT("A crawling ball has no triangle"), TrailWedgeQuad(Wedge, Centre, 160.f, FVector(Wedge.TrailWedgeMinSpeed * .5, 0, 0), Quad, Aspect));
		TestFalse(TEXT("A ball dropping straight down has no triangle"), TrailWedgeQuad(Wedge, Centre, 160.f, FVector(0, 0, -3000), Quad, Aspect));
		TestFalse(TEXT("Non finite velocity is rejected"), TrailWedgeQuad(Wedge, Centre, 160.f, FVector(std::numeric_limits<double>::quiet_NaN(), 0, 0), Quad, Aspect));
		FString WedgeError;
		Wedge.TrailWedgeMaterial = FSoftObjectPath(TEXT("/Game/Assets/FX/Materials/MI_FX_BallTrailWedge.MI_FX_BallTrailWedge"));
		TestFalse(TEXT("A triangle trail needs its quad mesh"), Wedge.Validate(WedgeError));
		Wedge.TrailWedgeMesh = FSoftObjectPath(TEXT("/Engine/BasicShapes/Plane.Plane"));
		TestTrue(TEXT("Triangle trail alone is a valid configuration"), Wedge.Validate(WedgeError));
		Wedge.TrailWedgeLengthSeconds = 0; TestFalse(TEXT("A triangle with no length is rejected"), Wedge.Validate(WedgeError));
	}

	// One net update can merge Launched(A) -> Free -> Launched(B): the state reads Launched on both sides.
	T = ResolveTransition(ComicSeen(Shot), ComicState(EBasicBallState::Launched, 7, nullptr, 1));
	TestTrue(TEXT("A merged relaunch restarts the trail for the new launcher"), T.bStartTrail && T.bStopTrail);
	T = ResolveTransition(ComicSeen(Held), ComicState(EBasicBallState::Controlled, 9, HolderB));
	TestTrue(TEXT("A merged steal moves the arrow to the new holder"), T.bStartAim && T.bStopAim);
	T = ResolveTransition(ComicSeen(Held), ComicState(EBasicBallState::Controlled, 10, nullptr));
	TestTrue(TEXT("A holder that is not resolved yet shows no arrow"), T.bStopAim && !T.bStartAim);

	// Aim anchor: along the physics body's flattened forward, past the whole ball, on the ground.
	FTransform Quad;
	const FVector Extent(120, 60, 40);
	const FTransform Body(FRotator(0, 90, 0), FVector(1000, 2000, 50));
	// The held ball sits ahead of the nose: centre 160 cm in front of the body origin, drawn radius 30.
	const FVector HeldBall(1000, 2160, 80);
	const double Width = 2 * Extent.Y;
	const double Length = Width * Config.AimLengthWidthFraction;
	TestTrue(TEXT("An upright car has an aim anchor"), AimAnchor(Config, Body, Extent, HeldBall, 30.f, 1.f, Quad));
	double Expected = 160 + 30 + Width * Config.AimGapWidthFraction + Length * .5;
	TestTrue(TEXT("Arrow starts past the far edge of the held ball"), Quad.GetLocation().Equals(FVector(1000, 2000 + Expected, 50 - 40 + Config.AimGroundLiftCm), .01));
	TestTrue(TEXT("Arrow tail clears the ball"), Quad.GetLocation().Y - Length * .5 > HeldBall.Y + 30);
	TestTrue(TEXT("Arrow points along the car's forward"), Quad.GetUnitAxis(EAxis::X).Equals(FVector(0, 1, 0), .001));
	TestTrue(TEXT("Arrow size follows the car's width"), Quad.GetScale3D().Equals(FVector(Length * .01, Width * Config.AimWidthWidthFraction * .01, 1), .001));
	// A ball drawn bigger than its physics sphere pushes the arrow further out instead of sitting on top of it.
	TestTrue(TEXT("A big drawn ball still has an anchor"), AimAnchor(Config, Body, Extent, HeldBall, 160.f, 1.f, Quad));
	TestTrue(TEXT("Arrow tail clears a big drawn ball"), Quad.GetLocation().Y - Length * .5 > HeldBall.Y + 160);
	// A ball tucked behind the nose never pulls the arrow under the car.
	TestTrue(TEXT("A ball behind the nose still has an anchor"), AimAnchor(Config, Body, Extent, FVector(1000, 2000, 80), 30.f, 1.f, Quad));
	Expected = 120 + Width * Config.AimGapWidthFraction + Length * .5;
	TestEqual(TEXT("The nose is the nearest the arrow may start"), Quad.GetLocation().Y, 2000 + Expected, .01);
	// A leaning body must not tilt or shorten the arrow's direction: only the flattened heading counts.
	TestTrue(TEXT("A pitched car still has an anchor"), AimAnchor(Config, FTransform(FRotator(40, 90, 25), FVector(1000, 2000, 50)), Extent, HeldBall, 30.f, 1.f, Quad));
	TestTrue(TEXT("Pitch and roll never tilt the arrow"), Quad.GetUnitAxis(EAxis::X).Equals(FVector(0, 1, 0), .001) && Quad.GetUnitAxis(EAxis::Z).Equals(FVector::UpVector, .001));
	TestFalse(TEXT("A car standing on its nose has no honest direction"), AimAnchor(Config, FTransform(FRotator(90, 0, 0), FVector::ZeroVector), Extent, HeldBall, 30.f, 1.f, Quad));
	TestFalse(TEXT("Non finite input is rejected"), AimAnchor(Config, Body, Extent, HeldBall, std::numeric_limits<float>::quiet_NaN(), 1.f, Quad));
	TestEqual(TEXT("A drawn ball is never smaller than its physics sphere"), DrawnBallRadius(50.f, FVector(20, 20, 20)), 50.f);
	TestEqual(TEXT("A bigger visual mesh sets the drawn radius"), DrawnBallRadius(50.f, FVector(160, 150, 160)), 160.f);
	{
		// A ball that has rolled: its world bounds grow with the rotation, the measured ball must not.
		UStaticMeshComponent* Rolled = NewObject<UStaticMeshComponent>();
		Rolled->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		Rolled->SetRelativeScale3D(FVector(3.2));
		const FVector Rest = UnrotatedExtent(*Rolled);
		TestTrue(TEXT("Engine sphere at scale 3.2 measures a 160 cm radius"), Rest.Equals(FVector(160), 1.));
		Rolled->SetRelativeRotation(FRotator(45, 45, 30));
		TestTrue(TEXT("World bounds of the rolled ball are wider than the ball"),
			Rolled->CalcBounds(FTransform(Rolled->GetRelativeRotation(), FVector::ZeroVector, Rolled->GetRelativeScale3D())).BoxExtent.X > Rest.X * 1.2);
		TestTrue(TEXT("A rolled ball measures the same as one at rest"), UnrotatedExtent(*Rolled).Equals(Rest, .01));
	}

	TestEqual(TEXT("Pop starts from nothing"), AimPopScale(Config, 0.f), 0.f);
	TestEqual(TEXT("Pop overshoots"), AimPopScale(Config, Config.AimPopSeconds * .6f), Config.AimPopOvershoot, .001f);
	TestEqual(TEXT("Pop settles at full size"), AimPopScale(Config, Config.AimPopSeconds), 1.f);
	TestEqual(TEXT("Pop stays settled"), AimPopScale(Config, 10.f), 1.f);

	// Hold rings: pop in while held, shrink (never fade) to nothing once the ball has left.
	TestEqual(TEXT("Rings pop from nothing"), HoldRingScale(Config, EHALBallHoldRingPhase::Shown, 0.f), 0.f);
	TestEqual(TEXT("Rings overshoot"), HoldRingScale(Config, EHALBallHoldRingPhase::Shown, Config.HoldPopSeconds * .6f), Config.HoldPopOvershoot, .001f);
	TestEqual(TEXT("Rings settle at full size"), HoldRingScale(Config, EHALBallHoldRingPhase::Shown, 10.f), 1.f);
	TestEqual(TEXT("Leaving starts at full size"), HoldRingScale(Config, EHALBallHoldRingPhase::Leaving, 0.f), 1.f);
	TestEqual(TEXT("Leaving shrinks linearly"), HoldRingScale(Config, EHALBallHoldRingPhase::Leaving, Config.HoldShrinkSeconds * .5f), .5f, .001f);
	TestEqual(TEXT("Leaving ends at nothing"), HoldRingScale(Config, EHALBallHoldRingPhase::Leaving, Config.HoldShrinkSeconds), 0.f);
	TestEqual(TEXT("Off draws nothing"), HoldRingScale(Config, EHALBallHoldRingPhase::Off, 0.f), 0.f);
	FHALBallComicFXConfig Cut; Cut.HoldShrinkSeconds = 0;
	TestEqual(TEXT("A zero shrink time cuts at once"), HoldRingScale(Cut, EHALBallHoldRingPhase::Leaving, 0.f), 0.f);

	// Holder marker: over the rear half of the car, facing the camera, its arrow (+Y of the quad) pointing at the roof centre.
	{
		const FVector Down = -FVector::UpVector, North = FVector::ForwardVector, Roof(1000, 2000, 95);
		FTransform Marker;
		TestTrue(TEXT("A car has a marker"), HoldMarkerQuad(Config, Roof, North, 400.f, 120.f, Down, North, 1.f, Marker));
		TestTrue(TEXT("Marker floats behind the middle of the car and above the roof"), Marker.GetLocation().Equals(
			Roof + FVector(-400 * Config.HoldMarkerBackLengthFraction, 0, 120 * Config.HoldMarkerLiftWidthFraction), .01));
		TestTrue(TEXT("Marker faces the camera"), Marker.GetUnitAxis(EAxis::Z).Equals(FVector::UpVector, .001));
		TestTrue(TEXT("Arrow points from the rear at the middle of the car"), Marker.GetUnitAxis(EAxis::Y).Equals(North, .001));
		TestTrue(TEXT("Marker size follows the car's width"), Marker.GetScale3D().Equals(FVector(120 * Config.HoldRoofSizeWidthFraction * .01, 120 * Config.HoldRoofSizeWidthFraction * .01, 1), .001));
		TestTrue(TEXT("Car heading east: the arrow turns with it"), HoldMarkerQuad(Config, Roof, FVector::RightVector, 400.f, 120.f, Down, North, 1.f, Marker)
			&& Marker.GetUnitAxis(EAxis::Y).Equals(FVector::RightVector, .001) && Marker.GetLocation().Y < Roof.Y);
		TestTrue(TEXT("Marker scales with the pop"), HoldMarkerQuad(Config, Roof, North, 400.f, 120.f, Down, North, .5f, Marker)
			&& FMath::IsNearlyEqual(Marker.GetScale3D().X, 120 * Config.HoldRoofSizeWidthFraction * .005, .001));
		// Tilted camera: the lift alone already puts the marker up the screen, so the arrow still has a direction.
		const FRotationMatrix Tilted(FRotator(-60, 0, 0));
		TestTrue(TEXT("Tilted camera keeps the quad facing it"), HoldMarkerQuad(Config, Roof, North, 400.f, 120.f, Tilted.GetUnitAxis(EAxis::X), Tilted.GetUnitAxis(EAxis::Z), 1.f, Marker)
			&& Marker.GetUnitAxis(EAxis::Z).Equals(-Tilted.GetUnitAxis(EAxis::X), .001) && FMath::Abs(Marker.GetUnitAxis(EAxis::Y) | Tilted.GetUnitAxis(EAxis::X)) < .001);
		FHALBallComicFXConfig Overhead; Overhead.HoldMarkerBackLengthFraction = 0;
		TestTrue(TEXT("Straight overhead and seen from above, the arrow points down the screen"), HoldMarkerQuad(Overhead, Roof, North, 400.f, 120.f, Down, North, 1.f, Marker)
			&& Marker.GetUnitAxis(EAxis::Y).Equals(-North, .001));
		TestFalse(TEXT("A car without width has no marker"), HoldMarkerQuad(Config, Roof, North, 400.f, 0.f, Down, North, 1.f, Marker));
		TestFalse(TEXT("Nothing is drawn at zero size"), HoldMarkerQuad(Config, Roof, North, 400.f, 120.f, Down, North, 0.f, Marker));
		TestFalse(TEXT("Non finite bounds are rejected"), HoldMarkerQuad(Config, FVector(std::numeric_limits<double>::quiet_NaN(), 0, 0), North, 400.f, 120.f, Down, North, 1.f, Marker));
	}

	// Ball sprite: wider than the ball as drawn, so the rings have room outside it.
	TestEqual(TEXT("Ball sprite half size follows the drawn radius"), HoldBallHalfSize(Config, 160.f, 1.f), 160.f * Config.HoldBallSizeDiameterFraction, .001f);
	TestTrue(TEXT("Ball sprite is wider than the ball"), HoldBallHalfSize(Config, 160.f, 1.f) > 160.f);
	TestEqual(TEXT("Non finite radius draws nothing"), HoldBallHalfSize(Config, std::numeric_limits<float>::quiet_NaN(), 1.f), 0.f);
	Config.HoldBallMaterial = FSoftObjectPath(TEXT("/Game/Assets/FX/Materials/MI_FX_BallHoldRing_Ball.MI_FX_BallHoldRing_Ball"));
	TestFalse(TEXT("Ball rings need the quad mesh too"), Config.Validate(Error));
	Config.HoldRoofMesh = FSoftObjectPath(TEXT("/Engine/BasicShapes/Plane.Plane"));
	TestTrue(TEXT("Ball rings alone are a valid configuration"), Config.Validate(Error));
	{
		// Camera looking straight down with north (+X) up the screen.
		const FVector Down = -FVector::UpVector, North = FVector::ForwardVector;
		FTransform Ring;
		TestTrue(TEXT("A held ball has a ring quad"), HoldBallQuad(FVector(10, 20, 30), 200.f, FVector::RightVector, Down, North, Ring));
		TestTrue(TEXT("Ring quad sits on the ball and spans twice its half size"), Ring.GetLocation().Equals(FVector(10, 20, 30)) && Ring.GetScale3D().Equals(FVector(4, 4, 1), .001));
		TestTrue(TEXT("Ring quad faces the camera"), Ring.GetUnitAxis(EAxis::Z).Equals(FVector::UpVector, .001));
		TestTrue(TEXT("Car heading up the screen: rings lie across it"), FMath::Abs(Ring.GetUnitAxis(EAxis::X) | FVector::RightVector) > .999);
		TestTrue(TEXT("Hidden half is up the screen"), (Ring.GetUnitAxis(EAxis::Y) | North) < -.999);
		// Car turned to head east: its right is south, the rings turn with it.
		TestTrue(TEXT("Turned car still has a ring quad"), HoldBallQuad(FVector::ZeroVector, 200.f, -FVector::ForwardVector, Down, North, Ring));
		TestTrue(TEXT("Rings stay square to the heading"), FMath::Abs(Ring.GetUnitAxis(EAxis::X) | FVector::RightVector) < .001
			&& FMath::Abs(Ring.GetUnitAxis(EAxis::X) | FVector::ForwardVector) > .999);
		// Car heading down the screen: same line on screen, hidden half must not flip to the near side.
		TestTrue(TEXT("Reversed car has a ring quad"), HoldBallQuad(FVector::ZeroVector, 200.f, -FVector::RightVector, Down, North, Ring));
		TestTrue(TEXT("Hidden half stays up the screen when the car reverses"), (Ring.GetUnitAxis(EAxis::Y) | North) < -.999);
		TestTrue(TEXT("Car side-on to the view falls back to screen right"), HoldBallQuad(FVector::ZeroVector, 200.f, FVector::UpVector, Down, North, Ring)
			&& FMath::Abs(Ring.GetUnitAxis(EAxis::X) | FVector::RightVector) > .999);
		TestFalse(TEXT("No size draws no rings"), HoldBallQuad(FVector::ZeroVector, 0.f, FVector::RightVector, Down, North, Ring));
	}
	Config.HoldBallSizeDiameterFraction = 1.f; TestFalse(TEXT("A sprite no wider than the ball leaves no room for rings"), Config.Validate(Error));
	Config = {}; Config.HoldRoofMaterial = FSoftObjectPath(TEXT("/Game/Assets/FX/Materials/MI_FX_BallHoldRing_Roof.MI_FX_BallHoldRing_Roof"));
	TestFalse(TEXT("A holder marker needs its quad mesh"), Config.Validate(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALIncomingBallTest, "HAL.FutureCreation.Presentation.ComicDoodle.IncomingBallWarning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALIncomingBallTest::RunTest(const FString&)
{
	using namespace HALIncomingBall;
	const FHALBallComicFXConfig Config;
	float Seconds = 0;
	TestTrue(TEXT("A ball flying straight at the car is incoming"), IsIncoming(Config, FVector(0, 0, 50), FVector(1000, 0, 0), FVector(1000, 0, 0), Seconds));
	TestEqual(TEXT("Arrival time is distance over speed"), Seconds, 1.f, .001f);
	TestFalse(TEXT("A ball flying away is not"), IsIncoming(Config, FVector(0, 0, 50), FVector(-1000, 0, 0), FVector(1000, 0, 0), Seconds));
	TestFalse(TEXT("A ball that will miss wide is not"), IsIncoming(Config, FVector(0, 0, 50), FVector(1000, 0, 0), FVector(1000, Config.IncomingMissDistanceCm + 10, 0), Seconds));
	TestTrue(TEXT("A near miss still warns"), IsIncoming(Config, FVector(0, 0, 50), FVector(1000, 0, 0), FVector(1000, Config.IncomingMissDistanceCm - 10, 0), Seconds));
	TestFalse(TEXT("A ball too far in time is not"), IsIncoming(Config, FVector(0, 0, 50), FVector(1000, 0, 0), FVector(1000 * (Config.IncomingMaximumSeconds + .1f), 0, 0), Seconds));
	TestFalse(TEXT("A slow roll is never announced"), IsIncoming(Config, FVector(0, 0, 50), FVector(Config.IncomingMinimumSpeed - 1, 0, 0), FVector(200, 0, 0), Seconds));
	TestTrue(TEXT("Height difference is ignored"), IsIncoming(Config, FVector(0, 0, 400), FVector(1000, 0, -200), FVector(1000, 0, 0), Seconds));
	TestFalse(TEXT("Non finite input is rejected"), IsIncoming(Config, FVector(0, 0, 0), FVector(std::numeric_limits<double>::quiet_NaN(), 0, 0), FVector(1000, 0, 0), Seconds));

	const float Inset = .06f;
	TestTrue(TEXT("Right of screen lands on the right inset edge"), EdgePoint(FVector2D(3, 0), Inset).Equals(FVector2D(1 - Inset, .5), .001));
	TestTrue(TEXT("Above the screen lands on the top inset edge"), EdgePoint(FVector2D(0, -.7), Inset).Equals(FVector2D(.5, Inset), .001));
	const FVector2D Corner = EdgePoint(FVector2D(1, 1), Inset);
	TestTrue(TEXT("A diagonal lands in the inset corner"), Corner.Equals(FVector2D(1 - Inset, 1 - Inset), .001));
	TestTrue(TEXT("No direction stays at the centre"), EdgePoint(FVector2D::ZeroVector, Inset).Equals(FVector2D(.5, .5), .001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALDoodleStatusTest, "HAL.FutureCreation.Presentation.ComicDoodle.StunStarsAndLowHealth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALDoodleStatusTest::RunTest(const FString&)
{
	using namespace HALVehicleDoodleStatus;
	FHALVehicleDoodleStatusConfig Config;
	FString Error;
	TestTrue(TEXT("Shipped doodle status defaults are valid"), Config.Validate(Error));
	Config.LowHealthFraction = 1.f; TestFalse(TEXT("A threshold of one would mark every car"), Config.Validate(Error));
	Config = {}; Config.StunSizeWidthFraction = 0; TestFalse(TEXT("Sprite size must be positive"), Config.Validate(Error));
	Config = {};

	TestTrue(TEXT("A stunned live car shows stars"), ShowStunStars(true, 40.f));
	TestFalse(TEXT("An eliminated car shows no stars"), ShowStunStars(true, 0.f));
	TestFalse(TEXT("A car that is not stunned shows no stars"), ShowStunStars(false, 40.f));

	TestFalse(TEXT("Sweat is off by default"), ShowLowHealth(Config, 100.f * Config.LowHealthFraction, 100.f));
	Config.bLowHealth = true;
	TestFalse(TEXT("Full health shows no sweat"), ShowLowHealth(Config, 100.f, 100.f));
	TestTrue(TEXT("At the threshold the sweat appears"), ShowLowHealth(Config, 100.f * Config.LowHealthFraction, 100.f));
	TestFalse(TEXT("Just above the threshold it does not"), ShowLowHealth(Config, 100.f * Config.LowHealthFraction + 1.f, 100.f));
	TestFalse(TEXT("An eliminated car shows no sweat"), ShowLowHealth(Config, 0.f, 100.f));
	TestFalse(TEXT("Unknown maximum HP shows nothing"), ShowLowHealth(Config, 10.f, 0.f));
	TestFalse(TEXT("Non finite HP shows nothing"), ShowLowHealth(Config, std::numeric_limits<float>::quiet_NaN(), 100.f));

	// Two cars each publish one spark event for the same contact, so the per event range stays small.
	const FHALImpactBurstConfig Burst;
	TestTrue(TEXT("Shipped spark range stays readable when doubled"), Burst.MinimumSparks >= 1 && Burst.MaximumSparks * 2 <= 12);
	TestEqual(TEXT("Only direct car contact sparks"), int32(HALVehiclePresentation::BurstSparkCount(Burst, 1.f, false)), 0);
	return true;
}
#endif
