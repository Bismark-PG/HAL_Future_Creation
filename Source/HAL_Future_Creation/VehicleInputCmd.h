// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VehicleInputCmd.generated.h"

/**
 * Input snapshot consumed by vehicle gameplay code.
 *
 * Keeping input collection separate from movement execution gives a future
 * server-authoritative input path one stable payload to submit and replay.
 */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FVehicleInputCmd
{
	GENERATED_BODY()

	/** Horizontal steering input in the range [-1, 1]. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle Input")
	float Steering = 0.0f;

	/** Forward throttle input in the range [0, 1]. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle Input")
	float Throttle = 0.0f;

	/** Brake input in the range [0, 1]; becomes reverse near a stop. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle Input")
	float Brake = 0.0f;

	/** Whether the drift / handbrake control is held. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle Input")
	bool bHandbrake = false;

	/** Whether the launch control is held. Reserved for the ball phase. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle Input")
	bool bLaunch = false;

	void Sanitize()
	{
		Steering = FMath::IsFinite(Steering) ? FMath::Clamp(Steering, -1.0f, 1.0f) : 0.0f;
		Throttle = FMath::IsFinite(Throttle) ? FMath::Clamp(Throttle, 0.0f, 1.0f) : 0.0f;
		Brake = FMath::IsFinite(Brake) ? FMath::Clamp(Brake, 0.0f, 1.0f) : 0.0f;
	}

	void Reset()
	{
		*this = FVehicleInputCmd();
	}
};
