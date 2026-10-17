#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../VehicleNetPhysicsData.h"

// Engine-container probes for 10-F-1. The existing wall-memory float is only an
// opaque marker here; these tests do not implement a movement Buff or alter assets.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMovementHistoryContractTest,
	"HAL.FutureCreation.MovementContract.AuthorityHistoryAndRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALMovementHistoryContractTest::RunTest(const FString& Parameters)
{
	using FHistory = TNetRewindHistory<FVehiclePhysicsHistoryState, true>;
	FHistory Predicted(8, true);
	for (int32 Frame = 10; Frame <= 17; ++Frame)
	{
		FVehiclePhysicsHistoryState State;
		State.LocalFrame = Frame;
		State.ServerFrame = Frame + 100;
		Predicted.RecordData(Frame, &State);
	}
	FHistory Received(1, false);
	FVehiclePhysicsHistoryState Authority;
	Authority.LocalFrame = Authority.ServerFrame = 114;
	Authority.State.WallEscapeBlend = 0.5f;
	Received.RecordData(114, &Authority);
	TestEqual(TEXT("Received server checkpoint maps to local frame and requests rewind on mismatch"),
		Predicted.ReceiveNewData(Received, 100, true), 14);
	FVehiclePhysicsHistoryState Read;
	TestTrue(TEXT("Exact authority checkpoint is available"), Predicted.ExtractData(14, false, &Read, true));
	TestTrue(TEXT("Received marker is distinguished from predicted data"), Read.bReceivedData);
	TestEqual(TEXT("Authoritative value replaces prediction at the mapped frame"), Read.State.WallEscapeBlend, 0.5f);
	TestEqual(TEXT("Redundant packet does not request another rewind"), Predicted.ReceiveNewData(Received, 100, true), INDEX_NONE);
	Authority.State.WallEscapeBlend = 0.9f;
	Received.RecordData(114, &Authority);
	Predicted.ReceiveNewData(Received, 100, true);
	Predicted.ExtractData(14, false, &Read, true);
	TestEqual(TEXT("Conflicting repeat cannot overwrite an already received exact checkpoint"), Read.State.WallEscapeBlend, 0.5f);
	TestTrue(TEXT("Older predicted frame remains independent of the newer authority marker"), Predicted.ExtractData(11, false, &Read, true));
	TestEqual(TEXT("New state was not applied retroactively to all old frames"), Read.State.WallEscapeBlend, 0.0f);
	Authority.LocalFrame = Authority.ServerFrame = 120;
	Authority.State.WallEscapeBlend = 0;
	Received.RecordData(120, &Authority);
	TestEqual(TEXT("Checkpoint without a predicted matching frame cannot itself request rewind"), Predicted.ReceiveNewData(Received, 100, true), INDEX_NONE);
	TestFalse(TEXT("Ring reuse does not invent the evicted exact frame"), Predicted.ExtractData(12, false, &Read, true));
	FHistory Empty(8, true);
	TestFalse(TEXT("Missing initial anchor is explicit rather than assumed neutral"), Empty.ExtractData(20, false, &Read, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALMovementActionContractTest,
	"HAL.FutureCreation.MovementContract.AuthorityActionsAndReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALMovementActionContractTest::RunTest(const FString& Parameters)
{
	// Use an existing reflected Authority payload to probe the engine's action
	// history, never its impact handler. EventSequence stands for an opaque revision.
	FActionHistory History;
	History.Initialize(8);
	FVehicleImpactAction First;
	First.EventSequence = 1;
	FVehicleImpactAction Second;
	Second.EventSequence = 2;
	TestEqual(TEXT("Carrier is server-authoritative, not client prediction"), First.GetAuthorStyle(), FNetworkPhysicsActionPayload::EActionAuthorStyle::Authority);
	History.AddAction(TInstancedStruct<FNetworkPhysicsActionPayload>::Make<FVehicleImpactAction>(First), 21, 100, 0x8001, false, true);
	History.AddAction(TInstancedStruct<FNetworkPhysicsActionPayload>::Make<FVehicleImpactAction>(Second), 21, 100, 0x8002, false, true);
	auto* Frame = History.FindFrameData(21);
	if (!TestNotNull(TEXT("Exact action frame retained"), Frame)) { return false; }
	TestEqual(TEXT("Two same-frame changes remain distinct"), Frame->Entries.Num(), 2);
	TestEqual(TEXT("Engine stamps server timeline"), Frame->Entries[0].ActionInstance.Get().ServerFrame, 121);
	TestEqual(TEXT("Insertion order is retained, application still needs revision ordering"), Frame->Entries[1].ActionInstance.Get<FVehicleImpactAction>().EventSequence, uint32(2));
	TestTrue(TEXT("Reliable action metadata is retained"), Frame->Entries[0].bReliable);
	Frame->Entries[0].bApplied = true;
	TestFalse(TEXT("Consumed action is not applied again in forward simulation"), FActionHistory::IsUsableEntry(Frame->Entries[0], false));
	TestTrue(TEXT("Consumed action remains available during resimulation"), FActionHistory::IsUsableEntry(Frame->Entries[0], true));
	TestNull(TEXT("Late action is not automatically moved to the current frame"), History.FindFrameData(24));
	History.AddAction(TInstancedStruct<FNetworkPhysicsActionPayload>::Make<FVehicleImpactAction>(Second), 29, 100, 0x8003, false, true);
	TestNull(TEXT("Old action slot expires with bounded history; state checkpoint is required"), History.FindFrameData(21));
	return true;
}

#endif
