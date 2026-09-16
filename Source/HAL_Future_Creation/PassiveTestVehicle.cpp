// Copyright Epic Games, Inc. All Rights Reserved.

#include "PassiveTestVehicle.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "VehicleHealthComponent.h"

APassiveTestVehicle::APassiveTestVehicle()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	CollisionRoot = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionRoot"));
	SetRootComponent(CollisionRoot);
	CollisionRoot->InitBoxExtent(FVector(100.0f, 70.0f, 35.0f));
	CollisionRoot->SetCollisionProfileName(TEXT("PhysicsActor"));
	CollisionRoot->SetSimulatePhysics(true);
	CollisionRoot->SetEnableGravity(true);
	CollisionRoot->SetLinearDamping(0.15f);
	CollisionRoot->SetAngularDamping(0.8f);
	CollisionRoot->SetMassOverrideInKg(NAME_None, 800.0f, true);
	CollisionRoot->BodyInstance.bUseCCD = true;

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(CollisionRoot);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetSimulatePhysics(false);

	Health = CreateDefaultSubobject<UVehicleHealthComponent>(TEXT("Health"));
}
