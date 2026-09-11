// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestVehiclePawn.h"

#include "ArcadeVehicleMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"

ATestVehiclePawn::ATestVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	CollisionRoot = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionRoot"));
	SetRootComponent(CollisionRoot);
	CollisionRoot->InitBoxExtent(FVector(100.0f, 70.0f, 35.0f));
	CollisionRoot->SetCollisionProfileName(TEXT("PhysicsActor"));
	CollisionRoot->SetSimulatePhysics(true);
	CollisionRoot->SetEnableGravity(true);
	CollisionRoot->SetLinearDamping(0.15f);
	CollisionRoot->SetAngularDamping(0.8f);
	CollisionRoot->SetMassOverrideInKg(NAME_None, 800.0f, true);
	CollisionRoot->SetNotifyRigidBodyCollision(true);
	CollisionRoot->BodyInstance.bUseCCD = true;

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(CollisionRoot);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetSimulatePhysics(false);

	ForwardArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("ForwardArrow"));
	ForwardArrow->SetupAttachment(CollisionRoot);
	ForwardArrow->SetRelativeLocation(FVector(110.0f, 0.0f, 0.0f));
	ForwardArrow->ArrowColor = FColor::Cyan;
	ForwardArrow->ArrowSize = 1.5f;
	ForwardArrow->SetHiddenInGame(false);

	ArcadeMovement = CreateDefaultSubobject<UArcadeVehicleMovementComponent>(TEXT("ArcadeMovement"));
	ArcadeMovement->SetUpdatedPrimitive(CollisionRoot);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(CollisionRoot);
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->SetRelativeRotation(FRotator(-60.0f, 0.0f, 0.0f));
	CameraBoom->TargetArmLength = 1200.0f;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 8.0f;

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCamera->bUsePawnControlRotation = false;
}

void ATestVehiclePawn::PawnClientRestart()
{
	Super::PawnClientRestart();

	ResetInputCommand();
	AddDefaultInputContext();
}

void ATestVehiclePawn::UnPossessed()
{
	RemoveDefaultInputContext();
	ResetInputCommand();

	Super::UnPossessed();
}

void ATestVehiclePawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveDefaultInputContext();
	ResetInputCommand();

	Super::EndPlay(EndPlayReason);
}

void ATestVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Error, TEXT("%s requires an Enhanced Input component."), *GetNameSafe(this));
		return;
	}

	if (SteeringAction)
	{
		EnhancedInput->BindAction(SteeringAction, ETriggerEvent::Triggered, this, &ThisClass::OnSteeringInput);
		EnhancedInput->BindAction(SteeringAction, ETriggerEvent::Completed, this, &ThisClass::OnSteeringCompleted);
	}
	if (ThrottleAction)
	{
		EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Started, this, &ThisClass::OnThrottleStarted);
		EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &ThisClass::OnThrottleCompleted);
	}
	if (BrakeAction)
	{
		EnhancedInput->BindAction(BrakeAction, ETriggerEvent::Started, this, &ThisClass::OnBrakeStarted);
		EnhancedInput->BindAction(BrakeAction, ETriggerEvent::Completed, this, &ThisClass::OnBrakeCompleted);
	}
	if (HandbrakeAction)
	{
		EnhancedInput->BindAction(HandbrakeAction, ETriggerEvent::Started, this, &ThisClass::OnHandbrakeStarted);
		EnhancedInput->BindAction(HandbrakeAction, ETriggerEvent::Completed, this, &ThisClass::OnHandbrakeCompleted);
	}
	if (LaunchAction)
	{
		EnhancedInput->BindAction(LaunchAction, ETriggerEvent::Started, this, &ThisClass::OnLaunchStarted);
		EnhancedInput->BindAction(LaunchAction, ETriggerEvent::Completed, this, &ThisClass::OnLaunchCompleted);
	}

	if (!DefaultMappingContext || !SteeringAction || !ThrottleAction || !BrakeAction || !HandbrakeAction || !LaunchAction)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s has unassigned Enhanced Input assets. Configure its Vehicle|Input defaults."),
			*GetNameSafe(this));
	}
}

void ATestVehiclePawn::AddDefaultInputContext()
{
	if (!IsLocallyControlled() || !DefaultMappingContext)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	if (!LocalPlayer)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
	{
		InputSubsystem->RemoveMappingContext(DefaultMappingContext);
		InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
	}
}

void ATestVehiclePawn::RemoveDefaultInputContext()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	if (!LocalPlayer)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
	{
		if (DefaultMappingContext)
		{
			InputSubsystem->RemoveMappingContext(DefaultMappingContext);
		}
	}
}

void ATestVehiclePawn::PushInputCommand()
{
	CurrentInputCommand.Sanitize();
	if (ArcadeMovement)
	{
		ArcadeMovement->SetInputCommand(CurrentInputCommand);
	}
}

void ATestVehiclePawn::ResetInputCommand()
{
	CurrentInputCommand.Reset();
	if (ArcadeMovement)
	{
		ArcadeMovement->ResetInputCommand();
	}
}

void ATestVehiclePawn::OnSteeringInput(const FInputActionValue& Value)
{
	CurrentInputCommand.Steering = Value.Get<float>();
	PushInputCommand();
}

void ATestVehiclePawn::OnSteeringCompleted(const FInputActionValue& Value)
{
	CurrentInputCommand.Steering = 0.0f;
	PushInputCommand();
}

void ATestVehiclePawn::OnThrottleStarted(const FInputActionValue& Value)
{
	CurrentInputCommand.Throttle = 1.0f;
	PushInputCommand();
}

void ATestVehiclePawn::OnThrottleCompleted(const FInputActionValue& Value)
{
	CurrentInputCommand.Throttle = 0.0f;
	PushInputCommand();
}

void ATestVehiclePawn::OnBrakeStarted(const FInputActionValue& Value)
{
	CurrentInputCommand.Brake = 1.0f;
	PushInputCommand();
}

void ATestVehiclePawn::OnBrakeCompleted(const FInputActionValue& Value)
{
	CurrentInputCommand.Brake = 0.0f;
	PushInputCommand();
}

void ATestVehiclePawn::OnHandbrakeStarted(const FInputActionValue& Value)
{
	CurrentInputCommand.bHandbrake = true;
	PushInputCommand();
}

void ATestVehiclePawn::OnHandbrakeCompleted(const FInputActionValue& Value)
{
	CurrentInputCommand.bHandbrake = false;
	PushInputCommand();
}

void ATestVehiclePawn::OnLaunchStarted(const FInputActionValue& Value)
{
	CurrentInputCommand.bLaunch = true;
	PushInputCommand();
}

void ATestVehiclePawn::OnLaunchCompleted(const FInputActionValue& Value)
{
	CurrentInputCommand.bLaunch = false;
	PushInputCommand();
}

