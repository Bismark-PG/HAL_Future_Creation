#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../HALMovementTypes.h"
#include "../HALMovementAttributeSet.h"
#include "../HALMovementEffectBridge.h"
#include "../HALPlayerState.h"
#include "../HALAbilitySystemComponent.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../VehicleNetPhysicsData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include <limits>

namespace
{
	FHALMovementSnapshot MakeMovement(FGuid Epoch, uint32 Revision, int32 Frame, float Acceleration, float Speed = 1)
	{
		FHALMovementSnapshot Value; Value.Epoch = Epoch; Value.Revision = Revision; Value.EffectiveServerFrame = Frame;
		Value.AccelerationScale = Acceleration; Value.SpeedScale = Speed; return Value;
	}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMovementTimelineTest, "HAL.FutureCreation.Movement.TimelineBoundariesAndPackets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALMovementTimelineTest::RunTest(const FString& Parameters)
{
	const auto Epoch = FGuid::NewGuid();
	FHALMovementTimeline Timeline;
	TestTrue(TEXT("Neutral initial anchor"), Timeline.Append(MakeMovement(Epoch, 1, 10, 1), 64));
	TestTrue(TEXT("Start is discrete"), Timeline.Append(MakeMovement(Epoch, 2, 15, 1.5f, 1.25f), 64));
	TestTrue(TEXT("Expiry is part of the same history"), Timeline.Append(MakeMovement(Epoch, 3, 25, 1), 64));
	Timeline.ExtendTo(30, 64);
	FHALMovementSnapshot Value;
	Timeline.Query(14, Value); TestEqual(TEXT("Before start"), Value.AccelerationScale, 1.f);
	Timeline.Query(15, Value); TestEqual(TEXT("Exact start"), Value.AccelerationScale, 1.5f);
	Timeline.Query(24, Value); TestEqual(TEXT("Old frame remains boosted after current expiry"), Value.AccelerationScale, 1.5f);
	Timeline.Query(25, Value); TestEqual(TEXT("Exact expiry"), Value.AccelerationScale, 1.f);
	TestFalse(TEXT("Unknown frames are not backfilled"), Timeline.Query(9, Value));
	TestFalse(TEXT("Future coverage is not invented"), Timeline.Query(31, Value));
	FHALMovementTimeline Newer = Timeline; Newer.Trim(8);
	TestEqual(TEXT("Trim keeps an exact boundary"), Newer.StartFrame, 23);
	Newer.Query(23, Value); TestEqual(TEXT("Pruned start retains its effective anchor"), Value.AccelerationScale, 1.5f);
	FHALMovementTimeline Reordered = Newer;
	TestTrue(TEXT("Late packet can fill an earlier known interval"), Reordered.Merge(Timeline, 64));
	Reordered.Query(15, Value); TestEqual(TEXT("Late facts do not overwrite the later expiry"), Value.Revision, uint32(2));
	Reordered.Query(29, Value); TestEqual(TEXT("Later expiry retained"), Value.Revision, uint32(3));
	TestTrue(TEXT("Duplicate packet is idempotent"), Reordered.Merge(Timeline, 64));
	auto Conflict = Timeline; Conflict.Anchor.AccelerationScale = 2;
	TestFalse(TEXT("Conflicting immutable revision rejected"), Reordered.Merge(Conflict, 64));
	auto Other = Timeline; Other.Epoch = FGuid::NewGuid();
	TestFalse(TEXT("Old/new Avatar cannot merge"), Reordered.Merge(Other, 64));
	TestTrue(TEXT("Same frame higher revision wins"), Timeline.Append(MakeMovement(Epoch, 4, 30, 2), 64));
	Timeline.Query(30, Value); TestEqual(TEXT("Highest same-frame value"), Value.Revision, uint32(4));
	TestFalse(TEXT("Stale revision cannot overwrite"), Timeline.Append(MakeMovement(Epoch, 2, 30, 1.5f), 64));
	FHALMovementTimeline Gap;
	Gap.Append(MakeMovement(Epoch, 7, 100, 1), 8); Gap.ExtendTo(105, 8);
	TestTrue(TEXT("New full checkpoint can recover after an evicted event"), Reordered.Merge(Gap, 8));
	TestFalse(TEXT("Recovery never invents the gap"), Reordered.Query(70, Value));
	TestTrue(TEXT("Recovered current interval"), Reordered.Query(104, Value));
	FHALMovementTimeline EndOfFrameRange;
	EndOfFrameRange.Append(MakeMovement(Epoch, 10, MAX_int32 - 2, 1), 8);
	EndOfFrameRange.ExtendTo(MAX_int32, 8);
	TestTrue(TEXT("Merging the last representable frame terminates"), EndOfFrameRange.Merge(EndOfFrameRange, 8));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMovementSerializationTest, "HAL.FutureCreation.Movement.SerializationAndDiscreteInterpolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALMovementSerializationTest::RunTest(const FString& Parameters)
{
	const auto Epoch = FGuid::NewGuid();
	FVehiclePhysicsHistoryState Low, High, Mid;
	Low.LocalFrame = Low.ServerFrame = 11;
	Low.State.MovementTimeline.Append(MakeMovement(Epoch, 1, 10, 1), 64);
	Low.State.MovementSnapshot = Low.State.MovementTimeline.Anchor;
	High = Low; High.LocalFrame = High.ServerFrame = 31;
	High.State.MovementTimeline.Append(MakeMovement(Epoch, 2, 15, 1.5f), 64);
	High.State.MovementTimeline.Append(MakeMovement(Epoch, 3, 25, 1), 64);
	High.State.MovementTimeline.ExtendTo(30, 64);
	High.State.MovementTimeline.Query(30, High.State.MovementSnapshot);
	Mid.LocalFrame = Mid.ServerFrame = 20;
	Mid.InterpolateData(Low, High);
	TestEqual(TEXT("S(20) uses value after19; never an averaged or nearest version"), Mid.State.MovementSnapshot.AccelerationScale, 1.5f);
	TArray<uint8> Bytes; bool bSuccess;
	{ FMemoryWriter Writer(Bytes); High.NetSerialize(Writer, nullptr, bSuccess); }
	TestTrue(TEXT("State with timeline serializes"), bSuccess);
	FVehiclePhysicsHistoryState Read;
	{ FMemoryReader Reader(Bytes); Read.NetSerialize(Reader, nullptr, bSuccess); }
	TestTrue(TEXT("State with timeline deserializes"), bSuccess);
	FHALMovementSnapshot Value; Read.State.MovementTimeline.Query(16, Value);
	TestEqual(TEXT("Old boosted frame survives wire round trip"), Value.AccelerationScale, 1.5f);
	Read.State.MovementTimeline.Changes[0].AccelerationScale = 1.25f;
	TestFalse(TEXT("Missed old boost differs even after both current values expired"), High.CompareData(Read));
	Read.State.MovementTimeline = High.State.MovementTimeline;
	Read.State.MovementSnapshot.Revision++;
	TestFalse(TEXT("Identity changes trigger correction even when values have returned to1"), High.CompareData(Read));
	FHALMovementTimeline Invalid = High.State.MovementTimeline; Invalid.FormatVersion = 99;
	Bytes.Reset(); { FMemoryWriter Writer(Bytes); TestFalse(TEXT("Unknown wire version rejected"), Invalid.Serialize(Writer)); }
	Invalid = High.State.MovementTimeline; Invalid.Changes[0].SpeedScale = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN payload rejected"), Invalid.Validate());
	Invalid = High.State.MovementTimeline; Invalid.Changes[0].EffectiveServerFrame = 10;
	TestFalse(TEXT("Out of order boundary rejected"), Invalid.Validate());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMovementGASTest, "HAL.FutureCreation.Movement.GASContributionsAndBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALMovementGASTest::RunTest(const FString& Parameters)
{
	const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* State = World->SpawnActor<AHALPlayerState>();
	auto* ASC = State->GetHALAbilitySystemComponent();
	AActor* Avatar = World->SpawnActor<AActor>();
	auto* Movement = NewObject<UArcadeVehicleMovementComponent>(Avatar); Avatar->AddInstanceComponent(Movement); Movement->RegisterComponent();
	auto* Bridge = NewObject<UHALMovementEffectBridge>(Avatar); Avatar->AddInstanceComponent(Bridge); Bridge->RegisterComponent();
	ASC->InitAbilityActorInfo(State, Avatar); Bridge->Bind(ASC, 10);
	const auto Add = [ASC](float Delta)
	{
		auto* GE = NewObject<UGameplayEffect>(); GE->DurationPolicy = EGameplayEffectDurationType::Infinite;
		FGameplayModifierInfo Modifier; Modifier.Attribute = UHALMovementAttributeSet::GetAccelerationMultiplierDeltaAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive; Modifier.ModifierMagnitude = FScalableFloat(Delta); GE->Modifiers.Add(Modifier);
		return ASC->ApplyGameplayEffectToSelf(GE, 1, ASC->MakeEffectContext());
	};
	const auto Positive = Add(0.5f), Negative = Add(-0.3f);
	const auto* Values = ASC->GetSet<UHALMovementAttributeSet>();
	float Accel = 0, Speed = 0;
	FHALMovementSnapshot::CalculateScales(Values->GetAccelerationMultiplierDelta(), Values->GetSpeedLimitMultiplierDelta(), Accel, Speed);
	TestTrue(TEXT("Independent GE deltas add to1.2"), FMath::IsNearlyEqual(Accel, 1.2f));
	const auto StrongNegative = Add(-2);
	FHALMovementSnapshot::CalculateScales(Values->GetAccelerationMultiplierDelta(), 0, Accel, Speed);
	TestEqual(TEXT("Final scale clamps at0"), Accel, 0.f);
	ASC->RemoveActiveGameplayEffect(StrongNegative); ASC->RemoveActiveGameplayEffect(Negative);
	FHALMovementSnapshot::CalculateScales(Values->GetAccelerationMultiplierDelta(), 0, Accel, Speed);
	TestEqual(TEXT("Clamping did not discard contributions needed on removal"), Accel, 1.5f);
	ASC->InitAbilityActorInfo(State, Avatar); Bridge->Bind(ASC, 10);
	TestEqual(TEXT("Repeated binding does not reset active GE"), Values->GetAccelerationMultiplierDelta(), 0.5f);
	ASC->RemoveActiveGameplayEffect(Positive);
	TestFalse(TEXT("Nonfinite deltas cannot publish"), FHALMovementSnapshot::CalculateScales(std::numeric_limits<float>::infinity(), 0, Accel, Speed));
	FHALMovementSnapshot::CalculateScales(MAX_flt, -MAX_flt, Accel, Speed);
	TestEqual(TEXT("Upper test guard"), Accel, 3.f); TestEqual(TEXT("Zero allowed"), Speed, 0.f);
	Bridge->Unbind();
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}
#endif
