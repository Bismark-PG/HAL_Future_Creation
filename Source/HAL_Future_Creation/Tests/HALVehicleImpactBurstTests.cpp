#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../HALVehiclePresentationTypes.h"
#include "../HALImpactBurstSettings.h"
#include <limits>

namespace
{
	FHALImpactBurstEvent BurstEvent(uint32 Sequence, uint8 Strength = 0,
		EHALImpactTier Tier = EHALImpactTier::Heavy, FVector Point = FVector(100, 0, 0))
	{
		FHALImpactBurstEvent Out;
		Out.Sequence = Sequence; Out.Strength = Strength; Out.Tier = Tier; Out.WorldPoint = Point;
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALImpactBurstShapeTest, "HAL.FutureCreation.Presentation.ImpactBurst.StrengthSeedAndPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALImpactBurstShapeTest::RunTest(const FString&)
{
	using namespace HALVehiclePresentation;
	FHALImpactBurstConfig Config;
	FString Error;
	TestTrue(TEXT("Shipped burst defaults are valid"), Config.Validate(Error));
	Config.MaximumPushSpeed = Config.MinimumPushSpeed; TestFalse(TEXT("Push speed range must ascend"), Config.Validate(Error));
	Config = {}; Config.MinimumSizeCm = 300; TestFalse(TEXT("Size range must ascend"), Config.Validate(Error));
	Config = {}; Config.LifetimeSeconds = 0; TestFalse(TEXT("Lifetime must be positive"), Config.Validate(Error));
	Config = {}; Config.MaximumConcurrent = 0; TestFalse(TEXT("Budget must allow one burst"), Config.Validate(Error));
	Config = {}; Config.MaximumEventAgeSeconds = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("Finite event age required"), Config.Validate(Error));
	Config = {}; Config.SeedParameter = NAME_None; TestFalse(TEXT("Material parameter names are required"), Config.Validate(Error));
	Config = {}; Config.bEnabled = false; Config.LifetimeSeconds = -1;
	TestTrue(TEXT("Disabled configuration is never rejected"), Config.Validate(Error));

	Config = {};
	TestEqual(TEXT("Weakest accepted push still shows the smallest burst"), BurstStrength(Config, 0.f), 0.f);
	TestEqual(TEXT("Push at the floor reads as zero strength"), BurstStrength(Config, Config.MinimumPushSpeed), 0.f);
	TestEqual(TEXT("Push midpoint reads as half strength"), BurstStrength(Config, 525.f), .5f, .001f);
	TestEqual(TEXT("Push above the ceiling clamps"), BurstStrength(Config, 5000.f), 1.f);
	TestEqual(TEXT("Non finite push is not a strong hit"), BurstStrength(Config, std::numeric_limits<float>::quiet_NaN()), 0.f);
	TestEqual(TEXT("Full strength quantizes to the top step"), int32(QuantizeBurstStrength(1.f)), 255);
	TestEqual(TEXT("Zero strength quantizes to the bottom step"), int32(QuantizeBurstStrength(-2.f)), 0);
	TestEqual(TEXT("Quantization survives the round trip"), float(QuantizeBurstStrength(.5f)) / 255.f, .5f, .01f);

	TestEqual(TEXT("Seeds repeat for the same authority identity"), BurstSeed(7), BurstSeed(7));
	TestFalse(TEXT("Seeds differ between impacts"), FMath::IsNearlyEqual(BurstSeed(7), BurstSeed(8)));
	TestTrue(TEXT("Seeds stay inside the material range"), BurstSeed(123456) >= 0.f && BurstSeed(123456) <= 1.f);
	TestEqual(TEXT("Heavy hits take the heavy colour"), BurstEdgeColor(Config, EHALImpactTier::Heavy), Config.HeavyColor);
	TestEqual(TEXT("Medium hits take the medium colour"), BurstEdgeColor(Config, EHALImpactTier::Medium), Config.MediumColor);
	TestEqual(TEXT("Light hits take the light colour"), BurstEdgeColor(Config, EHALImpactTier::Light), Config.LightColor);

	const FVector Target(0, 0, 0), Push(1, 0, 0);
	TestEqual(TEXT("Reported contact is lifted off the surface toward the source"),
		ResolveBurstPoint(Config, FVector(50, 0, 0), Target, Push), FVector(40, 0, 20));
	TestEqual(TEXT("Attacks without a contact point fall back to the target edge"),
		ResolveBurstPoint(Config, FVector::ZeroVector, Target, Push), FVector(-100, 0, 20));
	TestEqual(TEXT("A contact point far from the target is not trusted"),
		ResolveBurstPoint(Config, FVector(5000, 0, 0), Target, Push), FVector(-100, 0, 20));
	TestEqual(TEXT("A vertical push still places a burst"),
		ResolveBurstPoint(Config, FVector::ZeroVector, Target, FVector::ZeroVector), FVector(0, 0, 20));
	TestTrue(TEXT("A non finite target never produces a non finite sprite position"),
		ResolveBurstPoint(Config, FVector(50, 0, 0), FVector(std::numeric_limits<double>::quiet_NaN(), 0, 0), Push).IsZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALImpactBurstIdentityTest, "HAL.FutureCreation.Presentation.ImpactBurst.IdentityBudgetAndLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALImpactBurstIdentityTest::RunTest(const FString&)
{
	using namespace HALVehiclePresentation;
	FHALImpactBurstConfig Config;
	FHALImpactBurstState State;
	TArray<int32> Released;

	TestFalse(TEXT("Disabled configuration never starts a burst"),
		[&] { FHALImpactBurstConfig Off; Off.bEnabled = false; return AcceptBurst(Off, BurstEvent(1), 0, State, Released); }());
	TestFalse(TEXT("Immune or reduced impacts do not burst"), AcceptBurst(Config, BurstEvent(1, 0, EHALImpactTier::None), 0, State, Released));
	TestFalse(TEXT("Zero sequence rejected"), AcceptBurst(Config, BurstEvent(0), 0, State, Released));
	TestTrue(TEXT("First received impact starts once"), AcceptBurst(Config, BurstEvent(1), 0, State, Released));
	TestFalse(TEXT("Repeated identity does not restart"), AcceptBurst(Config, BurstEvent(1), 0, State, Released));
	TestFalse(TEXT("Late packet is discarded"), AcceptBurst(Config, BurstEvent(2), .6f, State, Released));
	TestFalse(TEXT("Discarded identity cannot replay with a fresh age"), AcceptBurst(Config, BurstEvent(2), 0, State, Released));
	TestFalse(TEXT("A packet older than the burst itself never draws"), AcceptBurst(Config, BurstEvent(3), .25f, State, Released));
	TestEqual(TEXT("Only the drawable burst is live"), State.Pulses.Num(), 1);
	TestEqual(TEXT("Started count follows drawn bursts"), int32(State.StartedCount), 1);

	const FHALImpactBurstPulse& First = State.Pulses[0];
	TestEqual(TEXT("The weakest hit uses the smallest sprite"), First.SizeCm, Config.MinimumSizeCm);
	TestEqual(TEXT("The weakest hit uses the fewest spikes"), First.Spikes, Config.MinimumSpikes);
	TestEqual(TEXT("Lifetime starts at the configured base"), First.Lifetime, Config.LifetimeSeconds);

	State = {}; Released.Reset();
	TestTrue(TEXT("Strongest hit accepted"), AcceptBurst(Config, BurstEvent(1, 255), 0, State, Released));
	TestEqual(TEXT("The strongest hit uses the largest sprite"), State.Pulses[0].SizeCm, Config.MaximumSizeCm);
	TestEqual(TEXT("The strongest hit uses the most spikes"), State.Pulses[0].Spikes, Config.MaximumSpikes);
	TestEqual(TEXT("The strongest hit lingers longest"), State.Pulses[0].Lifetime,
		Config.LifetimeSeconds + Config.StrengthLifetimeSeconds, .0001f);

	State = {};
	for (uint32 Sequence = 1; Sequence <= uint32(Config.MaximumConcurrent); ++Sequence)
	{
		AcceptBurst(Config, BurstEvent(Sequence), 0, State, Released);
		State.Pulses.Last().Slot = int32(Sequence) - 1;
	}
	TestEqual(TEXT("Budget filled"), State.Pulses.Num(), Config.MaximumConcurrent);
	TestTrue(TEXT("Overflow accepted"), AcceptBurst(Config, BurstEvent(99), 0, State, Released));
	TestEqual(TEXT("Overflow stays inside the budget"), State.Pulses.Num(), Config.MaximumConcurrent);
	TestTrue(TEXT("The evicted sprite is handed back exactly once"), Released.Num() == 1 && Released[0] == 0);
	TestEqual(TEXT("The surviving oldest burst kept its sprite"), State.Pulses[0].Slot, 1);

	State = {}; AcceptBurst(Config, BurstEvent(1, 0), 0, State, Released);
	State.Pulses.Last().Slot = 3;
	AdvanceBursts(Config, Config.LifetimeSeconds * .5f, State, Released);
	TestTrue(TEXT("A live burst keeps its sprite"), State.Pulses.Num() == 1 && Released.IsEmpty());
	TestEqual(TEXT("Age advances with the frame"), State.Pulses[0].Age, Config.LifetimeSeconds * .5f, .0001f);
	AdvanceBursts(Config, Config.LifetimeSeconds, State, Released);
	TestTrue(TEXT("An expired burst releases its sprite"), State.Pulses.IsEmpty() && Released.Num() == 1 && Released[0] == 3);
	AdvanceBursts(Config, std::numeric_limits<float>::quiet_NaN(), State, Released);
	TestTrue(TEXT("A broken frame delta clears instead of corrupting ages"), State.Pulses.IsEmpty());

	State = {}; State.LastSeenSequence = MAX_uint32 - 1;
	TestTrue(TEXT("Prewrap sequence accepted"), AcceptBurst(Config, BurstEvent(MAX_uint32), 0, State, Released));
	TestTrue(TEXT("Postwrap sequence accepted"), AcceptBurst(Config, BurstEvent(1), 0, State, Released));
	State.Pulses.Last().Slot = 2;
	Config.bEnabled = false;
	AdvanceBursts(Config, .02f, State, Released);
	TestTrue(TEXT("Disable clears bursts, releases sprites and keeps the watermark"),
		State.Pulses.IsEmpty() && Released.Contains(2) && State.LastSeenSequence == 1);
	return true;
}
#endif
