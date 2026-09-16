// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PassiveTestVehicle.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UVehicleHealthComponent;

/** Pushable, damageable test vehicle without input, movement forces, or ball acquisition. */
UCLASS()
class HAL_FUTURE_CREATION_API APassiveTestVehicle : public AActor
{
	GENERATED_BODY()

public:
	APassiveTestVehicle();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Components")
	TObjectPtr<UBoxComponent> CollisionRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Components")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Components")
	TObjectPtr<UVehicleHealthComponent> Health;
};
