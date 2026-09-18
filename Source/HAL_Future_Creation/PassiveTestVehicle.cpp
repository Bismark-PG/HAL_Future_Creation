// Copyright Epic Games, Inc. All Rights Reserved.

#include "PassiveTestVehicle.h"
#include "VehicleDefinition.h"
#include "VehicleConfigurationApplication.h"
#include "VehicleKnockbackSettings.h"

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
	CollisionRoot->BodyInstance.SetMassOverride(800.0f, true);
	CollisionRoot->BodyInstance.bUseCCD = true;

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(CollisionRoot);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetSimulatePhysics(false);

	Health = CreateDefaultSubobject<UVehicleHealthComponent>(TEXT("Health"));
}

bool APassiveTestVehicle::ApplyDefinition(bool bPreview)
{
	if (ConfigurationSource == EVehicleConfigurationSource::Legacy) { return true; }
	FString Error;
	if (ConfigurationSource != EVehicleConfigurationSource::Definition || !Definition || !Definition->Validate(Error)
		|| Definition->CollisionShape != EVehicleCollisionShape::Box)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: invalid passive VehicleDefinition %s: %s. No default fallback."),
			*GetName(), *GetNameSafe(Definition), *Error);
		return false;
	}
	CollisionRoot->SetBoxExtent(Definition->BoxExtent);
	VehicleConfiguration::ApplyBody(*CollisionRoot, Definition->Physics);
	VehicleConfiguration::ApplyVisual(*VisualMesh, Definition->Visual);
	if (!bPreview) { verify(Health->ApplyConfiguration(Definition->Health)); }
	return true;
}

void APassiveTestVehicle::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!HasActorBegunPlay() && !GetWorld()->IsGameWorld()) { ApplyDefinition(true); }
}

void APassiveTestVehicle::PreInitializeComponents()
{
	bConfigurationValid = ApplyDefinition(false);
	if (!GetMutableDefault<UVehicleKnockbackSettings>()->InitializeRules(GetWorld())) { bConfigurationValid = false; }
	if (!bConfigurationValid)
	{
		CollisionRoot->SetSimulatePhysics(false);
		CollisionRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	Super::PreInitializeComponents();
}
