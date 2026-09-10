// Fill out your copyright notice in the Description page of Project Settings.


#include "TestVehiclePawn.h"

// Sets default values
ATestVehiclePawn::ATestVehiclePawn()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ATestVehiclePawn::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATestVehiclePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ATestVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

