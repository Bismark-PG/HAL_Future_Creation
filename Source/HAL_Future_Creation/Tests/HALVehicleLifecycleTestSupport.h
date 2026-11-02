#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "../TestVehiclePawn.h"
#include "../VehicleDefinition.h"
#include "../BallControlComponent.h"
#include "../BasicBallActor.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "UObject/UnrealType.h"

struct FHALVehicleLifecycleTestAccess
{
	static void Configure(ATestVehiclePawn* Pawn, bool B, bool bChangeMovement = true)
	{
		auto* Definition = DuplicateObject<UVehicleDefinition>(Pawn->GetVehicleDefinition(),Pawn);
		Definition->Health.Health.MaxHP = B ? 220 : 100;
		Definition->Combat.AttackPower = B ? 75 : 20;
		Definition->Combat.OutputMultiplierDelta = B ? .25f : 0;
		Definition->Combat.DamageTakenMultiplier = B ? .7f : 1;
		Definition->Impact.TierReduction = B ? 1 : 0;
		if (bChangeMovement) { Definition->Movement.Drive.ForwardAcceleration = B ? 3600 : 1800; }
		FindFProperty<FObjectProperty>(Pawn->GetClass(),TEXT("Definition"))->SetObjectPropertyValue_InContainer(Pawn,Definition);
	}
	static bool Acquire(ATestVehiclePawn* Pawn, ABasicBallActor* Ball)
	{ return Pawn->FindComponentByClass<UBallControlComponent>()->AcquireBall(Ball); }
	static float Acceleration(ATestVehiclePawn* Pawn)
	{ auto* M = Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>(); return FindFProperty<FFloatProperty>(M->GetClass(),TEXT("ForwardAcceleration"))->GetPropertyValue_InContainer(M); }
};
#endif
