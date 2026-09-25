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
	return true;
}

#endif
