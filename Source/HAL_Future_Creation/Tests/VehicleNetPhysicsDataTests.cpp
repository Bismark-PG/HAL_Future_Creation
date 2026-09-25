#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../VehicleNetPhysicsData.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleNetPhysicsInputValidationTest,
	"HAL.FutureCreation.Vehicle.PhysicsInput.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleNetPhysicsInputValidationTest::RunTest(const FString& Parameters)
{
	const UPhysicsSettings* PhysicsSettings = UPhysicsSettings::Get();
	TestTrue(TEXT("Chaos async fixed physics is enabled"), PhysicsSettings->bTickPhysicsAsync);
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

#endif
