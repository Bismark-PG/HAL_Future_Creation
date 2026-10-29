#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../HALMovementArea.h"
#include "../HALMovementAreaReceiver.h"
#include "../HALMovementTypes.h"
#include "../VehicleNetPhysicsData.h"
#include "../HALMovementAttributeSet.h"
#include "../HALMovementBoostEffect.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALPlayerState.h"
#include "../HALGameplayTags.h"
#include "../TestVehiclePawn.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include <limits>

namespace
{
	struct FAreaTestWorld
	{
		UWorld* World;
		FAreaTestWorld()
		{
			const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		}
		~FAreaTestWorld()
		{
			if (World->GetBegunPlay()) { World->EndPlay(EEndPlayReason::Quit); }
			World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
		}
		AActor* Source()
		{
			auto* Actor = World->SpawnActor<AActor>();
			Actor->PreInitializeComponents(); Actor->InitializeComponents(); Actor->PostInitializeComponents();
			Actor->DispatchBeginPlay(); return Actor;
		}
		UHALMovementAreaReceiver* Receiver(AActor* Avatar)
		{
			auto* Component = NewObject<UHALMovementAreaReceiver>(Avatar); Avatar->AddInstanceComponent(Component); Component->RegisterComponent(); return Component;
		}
	};
	FHALMovementSnapshot AreaSnapshot(FGuid Epoch, uint32 Revision, float Acceleration, float Speed, float Grip)
	{
		FHALMovementSnapshot Value; Value.Epoch = Epoch; Value.Revision = Revision; Value.bHasAreaState = true;
		Value.AreaAcceleration.Target = Acceleration; Value.AreaSpeed.Target = Speed; Value.AreaGrip.Target = Grip;
		Value.AccelerationScale = Acceleration; Value.SpeedScale = Speed; Value.GripScale = Grip;
		return Value;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALAreaSelectionTest, "HAL.FutureCreation.MovementAreas.PerParameterStrongestAndOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALAreaSelectionTest::RunTest(const FString& Parameters)
{
	FHALMovementAreaConfig A, B, Ice;
	A.AccelerationMultiplier = .7f; A.SpeedLimitMultiplier = .3f; A.RecoverySeconds = .2f;
	B.AccelerationMultiplier = .4f; B.SpeedLimitMultiplier = .8f; B.RecoverySeconds = .6f;
	Ice.Kind = EHALMovementAreaKind::Slippery;
	FHALMovementAreaSelection Forward, Reverse;
	Forward.Include(A); Forward.Include(B); Forward.Include(Ice);
	Reverse.Include(Ice); Reverse.Include(B); Reverse.Include(A);
	TestTrue(TEXT("Per-channel winners may come from different slow sources"), Forward.Scales.Equals(FVector(.4f,.3f,.2f),1.e-6));
	TestTrue(TEXT("Registration order never decides winners"), Forward.Scales == Reverse.Scales && Forward.RecoverySeconds == Reverse.RecoverySeconds);
	TestTrue(TEXT("Recovery follows each winning channel"), Forward.RecoverySeconds.Equals(FVector(.6f,.2f,.4f),1.e-6));
	A.RecoverySeconds = .9f; Forward.Include(A); TestTrue(TEXT("Equal-strength recovery tie uses max"), FMath::IsNearlyEqual(Forward.RecoverySeconds.Y,.9,1.e-6));
	const auto Before = Forward.Scales;
	A.AccelerationMultiplier = std::numeric_limits<float>::quiet_NaN(); Forward.Include(A);
	TestFalse(TEXT("Nonfinite authoring data rejected"), A.IsValid()); TestEqual(TEXT("Invalid source does not contaminate selection"), Forward.Scales, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALAreaCurveTest, "HAL.FutureCreation.MovementAreas.RecoveryReentryAndWireHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALAreaCurveTest::RunTest(const FString& Parameters)
{
	const auto Epoch = FGuid::NewGuid(); constexpr float Step = .02f;
	auto Enter = AreaSnapshot(Epoch,1,.3f,.4f,.2f); Enter.Stamp(nullptr,100,Step);
	TestTrue(TEXT("Strong entry applies at its actual authority frame"), Enter.EvaluateScales(100).Equals(FVector(.3f,.4f,.2f),1.e-6));
	auto Leave = AreaSnapshot(Epoch,2,.7f,.8f,1); Leave.Stamp(&Enter,110,Step);
	TestTrue(TEXT("Strong to weak is continuous at start"), Leave.EvaluateScales(110).Equals(Enter.EvaluateScales(110),1.e-6));
	const int32 Mid = 110 + Leave.AreaGrip.DurationFrames / 2;
	TestTrue(TEXT("Midpoint evaluated analytically, without per-frame GE updates"), Leave.EvaluateScales(Mid).Equals(FVector(.5f,.6f,.6f),.021));
	auto Boost = AreaSnapshot(Epoch,3,.7f,.8f,1); Boost.AreaAcceleration.OrdinaryDelta = .5f;
	Boost.AreaSpeed.OrdinaryDelta = .25f; Boost.Stamp(&Leave,Mid,Step);
	TestEqual(TEXT("Other buff changes do not restart area recovery"), Boost.AreaGrip.StartFrame,110);
	TestTrue(TEXT("Boost adds to the current recovering area, not to its clamped endpoint"), FMath::IsNearlyEqual(Boost.EvaluateScales(Mid).X, Leave.EvaluateScales(Mid).X+.5,.0001));
	auto Reenter = AreaSnapshot(Epoch,4,.2f,.2f,.1f); Reenter.Stamp(&Boost,Mid+1,Step);
	TestEqual(TEXT("More severe reentry does not accumulate old fades"), Reenter.AreaGrip.DurationFrames,0);
	TestTrue(TEXT("Reentry immediately replaces stronger per-channel targets"), Reenter.EvaluateScales(Mid+1).Equals(FVector(.2f,.2f,.1f),1.e-6));
	auto Exit = AreaSnapshot(Epoch,5,1,1,1); Exit.Stamp(&Reenter,150,Step);
	TestEqual(TEXT("Exit starts from current regional grip"), Exit.EvaluateScales(150).Z,Reenter.EvaluateScales(150).Z);
	TestEqual(TEXT("Configured game-time recovery reaches exactly neutral"), Exit.EvaluateScales(150+Exit.AreaGrip.DurationFrames),FVector::OneVector);
	Exit.AreaAcceleration.OrdinaryDelta = 4;
	TestEqual(TEXT("Sum raw deltas before final clamp"),Exit.EvaluateScales(150).X,3.0);
	Exit.AreaAcceleration.OrdinaryDelta = 2.2f;
	TestTrue(TEXT("An ordinary buff saturated alone still combines its raw contribution with an area"),FMath::IsNearlyEqual(Exit.EvaluateScales(150).X,2.4,1.e-6));
	Exit.AreaAcceleration.OrdinaryDelta = 0;
	FHALMovementTimeline Timeline; Timeline.Append(Enter,254); Timeline.Append(Leave,254); Timeline.Append(Boost,254);
	Timeline.Append(Reenter,254); Timeline.Append(Exit,254); Timeline.ExtendTo(170,254);
	TArray<uint8> Bytes; { FMemoryWriter Writer(Bytes); TestTrue(TEXT("Recovery timeline serializes"),Timeline.Serialize(Writer)); }
	FHALMovementTimeline Received; { FMemoryReader Reader(Bytes); TestTrue(TEXT("Recovery timeline deserializes"),Received.Serialize(Reader)); }
	FHALMovementSnapshot Old; Received.Query(Mid,Old);
	TestTrue(TEXT("Replaying after reentry/expiry uses the original curve"),Old.EvaluateScales(Mid).Equals(Boost.EvaluateScales(Mid),1.e-6));
	Received.Trim(12); Received.Query(160,Old);
	TestTrue(TEXT("Pruned checkpoint retains curve origin before the window"),Old.Equals(Exit));
	const auto Definition = Old;
	Old.AreaGrip.From = .8f;
	TestFalse(TEXT("Different recovery origin is a history mismatch"),Old.Equals(Definition));
	FVehiclePhysicsHistoryState Low,High,Interpolated;
	Low.LocalFrame=Low.ServerFrame=101; Low.State.MovementTimeline.Append(Enter,254); Low.State.MovementSnapshot=Enter;
	High.LocalFrame=High.ServerFrame=171; High.State.MovementTimeline=Timeline; High.State.MovementSnapshot=Exit;
	High.State.MovementConsumedScales=Exit.EvaluateScales(170);
	Interpolated.LocalFrame=Interpolated.ServerFrame=Mid+1; Interpolated.InterpolateData(Low,High);
	TestTrue(TEXT("State interpolation evaluates F-1 within the authoritative curve"),Interpolated.State.MovementConsumedScales.Equals(Boost.EvaluateScales(Mid),1.e-6));
	Bytes.Reset(); bool bSuccess=false;
	{ FMemoryWriter Writer(Bytes); High.NetSerialize(Writer,nullptr,bSuccess); }
	TestTrue(TEXT("Curve plus consumed physics values serialize"),bSuccess);
	FVehiclePhysicsHistoryState RoundTrip;
	{ FMemoryReader Reader(Bytes); RoundTrip.NetSerialize(Reader,nullptr,bSuccess); }
	TestTrue(TEXT("Consumed scales survive wire round trip"),bSuccess && RoundTrip.State.MovementConsumedScales.Equals(High.State.MovementConsumedScales,1.e-6));
	RoundTrip.State.MovementConsumedScales.Z-=.1;
	TestFalse(TEXT("Equal curve/revision with different actually consumed values requests correction"),High.CompareData(RoundTrip));
	// Integrating Stun must preserve an in-flight regional recovery rather than
	// restarting it, suppressing the permission-only change, or erasing old frames.
	auto Stunned = Leave; Stunned.Revision = 3; Stunned.bControlsBlocked = true; Stunned.Stamp(&Leave,115,Step);
	auto Released = Stunned; Released.Revision = 4; Released.bControlsBlocked = false; Released.Stamp(&Stunned,121,Step);
	TestEqual(TEXT("Stun entry preserves recovery origin"),Stunned.AreaGrip.StartFrame,Leave.AreaGrip.StartFrame);
	TestTrue(TEXT("Recovery keeps advancing while controls are blocked"),Stunned.EvaluateScales(120).Equals(Leave.EvaluateScales(120),1.e-6));
	TestTrue(TEXT("Stun expiry does not restart or reset recovery"),Released.EvaluateScales(122).Equals(Leave.EvaluateScales(122),1.e-6));
	FHALMovementTimeline Combined;
	Combined.Append(Enter,254); Combined.Append(Leave,254); Combined.Append(Stunned,254); Combined.Append(Released,254); Combined.ExtendTo(140,254);
	Bytes.Reset(); { FMemoryWriter Writer(Bytes); TestTrue(TEXT("Combined history serializes"),Combined.Serialize(Writer)); }
	FHALMovementTimeline CombinedRead;
	{ FMemoryReader Reader(Bytes); TestTrue(TEXT("Combined history deserializes"),CombinedRead.Serialize(Reader)); }
	CombinedRead.Query(114,Old); TestFalse(TEXT("Pre-stun recovery remains controllable"),Old.bControlsBlocked);
	CombinedRead.Query(120,Old);
	TestTrue(TEXT("Replay restores both historical stun and recovering grip"),Old.bControlsBlocked && Old.EvaluateScales(120).Equals(Leave.EvaluateScales(120),1.e-6));
	CombinedRead.Query(121,Old); TestFalse(TEXT("Expiry restores permission at exact frame"),Old.bControlsBlocked);
	TestTrue(TEXT("Expiry retains original recovery duration"),Old.AreaGrip.StartFrame==Leave.AreaGrip.StartFrame && Old.AreaGrip.DurationFrames==Leave.AreaGrip.DurationFrames);
	auto PermissionConflict=Stunned; PermissionConflict.bControlsBlocked=false;
	TestFalse(TEXT("Matching regional curve cannot hide a Stun mismatch"),Stunned.Equals(PermissionConflict));
	Bytes[0]=2;
	FHALMovementTimeline IndependentOldVersion;
	{ FMemoryReader Reader(Bytes); TestFalse(TEXT("Reject either incompatible independently authored v2 protocol"),IndependentOldVersion.Serialize(Reader)); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALAreaGASTest, "HAL.FutureCreation.MovementAreas.SourceLifecycleAndIndependentBuffs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALAreaGASTest::RunTest(const FString& Parameters)
{
	FAreaTestWorld F;
	auto* State = F.World->SpawnActor<AHALPlayerState>(); auto* ASC = State->GetHALAbilitySystemComponent();
	auto* Avatar = F.Source(); auto* Receiver = F.Receiver(Avatar);
	auto* SourceA = F.Source(); auto* SourceB = F.Source(); auto* SourceIce = F.Source();
	FHALMovementAreaConfig A,B,Ice; A.AccelerationMultiplier=.7f; A.SpeedLimitMultiplier=.3f;
	B.AccelerationMultiplier=.4f; B.SpeedLimitMultiplier=.8f; Ice.Kind=EHALMovementAreaKind::Slippery;
	TestTrue(TEXT("Area can register before ASC readiness"),Receiver->RegisterArea(SourceA,A));
	ASC->InitAbilityActorInfo(State,Avatar); Receiver->Bind(ASC);
	Receiver->RegisterArea(SourceB,B); Receiver->RegisterArea(SourceIce,Ice); Receiver->RegisterArea(SourceA,A);
	const auto* Values = ASC->GetSet<UHALMovementAttributeSet>();
	TestEqual(TEXT("Repeated overlap is idempotent"),Receiver->GetAreaCount(),3);
	TestTrue(TEXT("Only strongest acceleration contribution"),FMath::IsNearlyEqual(Values->GetAreaAccelerationDelta(),-.6f));
	TestTrue(TEXT("Different source wins speed"),FMath::IsNearlyEqual(Values->GetAreaSpeedDelta(),-.7f));
	TestTrue(TEXT("Slippery GE coexists"),FMath::IsNearlyEqual(Values->GetAreaGripDelta(),-.8f));
	TestEqual(TEXT("Only one selected slow GE"),ASC->GetTagCount(HALGameplayTags::State_Movement_SlowArea),1);
	const auto Boost=ASC->ApplyGameplayEffectToSelf(GetDefault<UHALMovementBoostEffect>(),1,ASC->MakeEffectContext());
	TestEqual(TEXT("Ordinary buff contribution remains independent"),Values->GetAccelerationMultiplierDelta(),.5f);
	SourceB->Destroy();
	TestTrue(TEXT("Destroy winning area selects remaining source"),FMath::IsNearlyEqual(Values->GetAreaAccelerationDelta(),-.3f));
	Receiver->UnregisterArea(SourceA); Receiver->UnregisterArea(SourceA);
	TestEqual(TEXT("Final slow removal clears only the selected slow delta"),Values->GetAreaSpeedDelta(),0.f);
	TestTrue(TEXT("Slow removal retains slippery family"),ASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_SlipperyArea));
	TestEqual(TEXT("Area exit does not hard-reset ordinary buff"),Values->GetAccelerationMultiplierDelta(),.5f);
	Receiver->Unbind(); TestEqual(TEXT("Unbind clears old Avatar regional effect"),Values->GetAreaGripDelta(),0.f);
	Receiver->Bind(ASC); TestTrue(TEXT("Rebind while still inside restores contribution"),Values->GetAreaGripDelta()<-.79f);
	auto* NewAvatar=F.Source(); auto* NewReceiver=F.Receiver(NewAvatar);
	ASC->InitAbilityActorInfo(State,NewAvatar); NewReceiver->Bind(ASC); NewReceiver->RegisterArea(SourceA,A);
	Receiver->Unbind(); SourceIce->Destroy();
	TestTrue(TEXT("Delayed old Avatar cleanup cannot clear replacement's slow effect"),Values->GetAreaSpeedDelta()<-.69f);
	TestFalse(TEXT("Old Avatar cannot bind replacement ASC"),Receiver->RegisterArea(nullptr,A));
	ASC->RemoveActiveGameplayEffect(Boost); NewAvatar->Destroy();
	TestEqual(TEXT("Avatar end clears all regional deltas"),Values->GetAreaSpeedDelta(),0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALAreaOverlapTest, "HAL.FutureCreation.MovementAreas.RealRootOverlapEnableAndDestroy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALAreaOverlapTest::RunTest(const FString& Parameters)
{
	FAreaTestWorld F;
	auto* VehicleClass=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
	if (!TestNotNull(TEXT("Authored vehicle class loads"),VehicleClass)) { return false; }
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Vehicle=F.World->SpawnActor<ATestVehiclePawn>(VehicleClass,FVector(0,0,10000),FRotator::ZeroRotator,Spawn);
	Vehicle->PreInitializeComponents(); Vehicle->InitializeComponents(); Vehicle->PostInitializeComponents();
	Vehicle->DispatchBeginPlay();
	TestTrue(TEXT("Authored collision and DA initialized"),Vehicle->IsConfigurationValid());
	// This isolated world does not advance a solver. Use its real root as a
	// kinematic query body so an immediate GT move has an immediately queryable pose.
	CastChecked<UPrimitiveComponent>(Vehicle->GetRootComponent())->SetSimulatePhysics(false);
	auto* Receiver=Vehicle->FindComponentByClass<UHALMovementAreaReceiver>();
	auto* Area=F.World->SpawnActor<AHALMovementArea>(FVector(0,0,10000),FRotator::ZeroRotator,Spawn);
	Area->GetAreaBounds()->SetBoxExtent(FVector(1000));
	Area->PreInitializeComponents(); Area->InitializeComponents(); Area->PostInitializeComponents(); Area->DispatchBeginPlay();
	TestEqual(TEXT("Startup scan registers real body already inside"),Receiver->GetAreaCount(),1);
	TestEqual(TEXT("Area never blocks a physical body"),Area->GetAreaBounds()->GetCollisionResponseToChannel(ECC_PhysicsBody),ECR_Overlap);
	// Component delegates require World BeginPlay, and Actor::ProcessEvent
	// additionally requires initialized world actors. DispatchBeginPlay alone is insufficient.
	F.World->InitializeActorsForPlay(FURL());
	F.World->SetBegunPlay(true);
	TestTrue(TEXT("World enables actual dynamic overlap dispatch"),F.World->HasBegunPlay() && F.World->AreActorsInitialized());
	Vehicle->SetActorLocation(FVector(5000,0,10000),false,nullptr,ETeleportType::TeleportPhysics);
	TestTrue(TEXT("Real root moved outside the volume"),Vehicle->GetActorLocation().Equals(FVector(5000,0,10000),.01));
	TestEqual(TEXT("Real root exit unregisters"),Receiver->GetAreaCount(),0);
	Vehicle->SetActorLocation(FVector(0,0,10000),false,nullptr,ETeleportType::TeleportPhysics);
	TestEqual(TEXT("Real root entry registers once"),Receiver->GetAreaCount(),1);
	Area->SetAreaEnabled(false); TestEqual(TEXT("Disable clears existing occupants"),Receiver->GetAreaCount(),0);
	Area->SetAreaEnabled(true); TestEqual(TEXT("Enable scans stationary occupant"),Receiver->GetAreaCount(),1);
	Area->Destroy(); TestEqual(TEXT("Destroy clears occupant without needing EndOverlap"),Receiver->GetAreaCount(),0);
	return true;
}
#endif
