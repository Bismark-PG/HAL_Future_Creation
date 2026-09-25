#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../VehicleNetPhysicsData.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleNetPhysicsInputValidationTest,
	"HAL.FutureCreation.Vehicle.PhysicsInput.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleNetPhysicsInputValidationTest::RunTest(const FString& Parameters)
{
	const UPhysicsSettings* PhysicsSettings = UPhysicsSettings::Get();
	TestTrue(TEXT("Chaos async fixed physics is enabled"), PhysicsSettings->bTickPhysicsAsync);
	TestTrue(TEXT("Network physics prediction is enabled"), PhysicsSettings->PhysicsPrediction.bEnablePhysicsPrediction);
	TestTrue(TEXT("Physics step is 60 Hz"), FMath::IsNearlyEqual(
		PhysicsSettings->AsyncFixedTimeStepSize, 1.0f / 60.0f, 0.000001f));
	FVehicleNetInputData Input;
	Input.Cmd.Steering = std::numeric_limits<float>::quiet_NaN();
	Input.Cmd.Throttle = std::numeric_limits<float>::infinity();
	Input.Cmd.Brake = -0.5f;
	Input.Cmd.bLaunch = true;
	Input.Sanitize();
	TestEqual(TEXT("NaN steering is neutral"), Input.Cmd.Steering, 0.0f);
	TestEqual(TEXT("Infinite throttle is neutral"), Input.Cmd.Throttle, 0.0f);
	TestEqual(TEXT("Brake is clamped"), Input.Cmd.Brake, 0.0f);
	TestFalse(TEXT("Launch is not repeated as continuous physics input"), Input.Cmd.bLaunch);
	Input.Cmd.Steering = -2.0f;
	Input.Cmd.Throttle = 2.0f;
	Input.Sanitize();
	TestEqual(TEXT("Steering range"), Input.Cmd.Steering, -1.0f);
	TestEqual(TEXT("Throttle range"), Input.Cmd.Throttle, 1.0f);
	Input.InputSequence = 1;
	TestTrue(TEXT("Sanitized continuous input can be submitted"), Input.IsValidContinuousInput());
	Input.Cmd.Throttle = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("Server rejects infinite throttle instead of clamping"), Input.IsValidContinuousInput());
	Input.Cmd.Throttle = 1.01f;
	TestFalse(TEXT("Server rejects out-of-range throttle"), Input.IsValidContinuousInput());
	Input.Cmd.Throttle = 1.0f;
	Input.Cmd.bLaunch = true;
	TestFalse(TEXT("Launch is rejected from continuous input"), Input.IsValidContinuousInput());
	Input.Cmd.bLaunch = false;
	TestTrue(TEXT("Newer input sequence accepted"), FVehicleNetInputData::IsNewerSequence(2, 1));
	TestFalse(TEXT("Duplicate input sequence rejected"), FVehicleNetInputData::IsNewerSequence(1, 1));
	TestFalse(TEXT("Reordered input sequence rejected"), FVehicleNetInputData::IsNewerSequence(1, 2));
	TestTrue(TEXT("Sequence wrap remains ordered"), FVehicleNetInputData::IsNewerSequence(1, MAX_uint32));
	TestFalse(TEXT("Zero sequence is reserved"), FVehicleNetInputData::IsNewerSequence(0, MAX_uint32));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehiclePhysicsHistorySerializationTest,
	"HAL.FutureCreation.Vehicle.PhysicsInput.HistorySerialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehiclePhysicsHistorySerializationTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Legacy vehicle keeps local command"), VehicleNetworkPhysics::ShouldUseLocalPendingInput(false, true, false));
	TestTrue(TEXT("History vehicle drives immediately from local command"), VehicleNetworkPhysics::ShouldUseLocalPendingInput(true, true, false));
	TestFalse(TEXT("Server remote vehicle consumes received history"), VehicleNetworkPhysics::ShouldUseLocalPendingInput(true, false, false));
	TestFalse(TEXT("Local rewind replays recorded input"), VehicleNetworkPhysics::ShouldUseLocalPendingInput(true, true, true));
	FVehiclePhysicsHistoryInput Source;
	Source.ServerFrame = 123;
	Source.Input.Cmd.Throttle = 0.75f;
	Source.Input.Cmd.Steering = -0.5f;
	Source.Input.Cmd.bHandbrake = true;
	Source.Input.InputSequence = 42;
	TArray<uint8> Buffer;
	bool bSuccess = false;
	{
		FMemoryWriter Writer(Buffer);
		Source.NetSerialize(Writer, nullptr, bSuccess);
	}
	TestTrue(TEXT("Input history writes"), bSuccess);
	FVehiclePhysicsHistoryInput Read;
	{
		FMemoryReader Reader(Buffer);
		Read.NetSerialize(Reader, nullptr, bSuccess);
	}
	TestTrue(TEXT("Input history reads"), bSuccess);
	TestEqual(TEXT("Server physics frame preserved"), Read.ServerFrame, Source.ServerFrame);
	TestEqual(TEXT("Input sequence preserved"), Read.Input.InputSequence, Source.Input.InputSequence);
	TestEqual(TEXT("Throttle preserved"), Read.Input.Cmd.Throttle, Source.Input.Cmd.Throttle);
	TestEqual(TEXT("Steering preserved"), Read.Input.Cmd.Steering, Source.Input.Cmd.Steering);
	TestTrue(TEXT("Handbrake preserved"), Read.Input.Cmd.bHandbrake);
	TestFalse(TEXT("Launch cannot enter input history"), Read.Input.Cmd.bLaunch);
	Read.DecayData(1.0f);
	TestEqual(TEXT("Expired history input releases throttle"), Read.Input.Cmd.Throttle, 0.0f);
	TestEqual(TEXT("Expired history input releases steering"), Read.Input.Cmd.Steering, 0.0f);
	TestFalse(TEXT("Expired history input releases handbrake"), Read.Input.Cmd.bHandbrake);

	FVehiclePhysicsHistoryState StateSource;
	StateSource.ServerFrame = 456;
	StateSource.State.Position = FVector(120.0, -45.0, 18.0);
	StateSource.State.LinearVelocity = FVector(500.0, 0.0, 0.0);
	StateSource.State.bWallEscapeActive = true;
	StateSource.State.WallEscapeBlend = 0.5f;
	StateSource.State.WallEscapeNormals.Add(FVector::ForwardVector);
	Buffer.Reset();
	{
		FMemoryWriter Writer(Buffer);
		StateSource.NetSerialize(Writer, nullptr, bSuccess);
	}
	TestTrue(TEXT("State history writes"), bSuccess);
	FVehiclePhysicsHistoryState StateRead;
	{
		FMemoryReader Reader(Buffer);
		StateRead.NetSerialize(Reader, nullptr, bSuccess);
	}
	TestTrue(TEXT("State history reads"), bSuccess);
	TestEqual(TEXT("State server frame preserved"), StateRead.ServerFrame, StateSource.ServerFrame);
	TestTrue(TEXT("Position preserved"), StateRead.State.Position.Equals(StateSource.State.Position));
	TestTrue(TEXT("Velocity preserved"), StateRead.State.LinearVelocity.Equals(StateSource.State.LinearVelocity));
	TestTrue(TEXT("Wall memory preserved"), StateRead.State.bWallEscapeActive
		&& StateRead.State.WallEscapeNormals.Num() == 1
		&& StateRead.State.WallEscapeNormals[0].Equals(FVector::ForwardVector));
	return true;
}

#endif
