// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestVehiclePawn.h"
#include "UObject/UnrealType.h"
#include "VehicleDefinition.h"
#include "VehicleConfigurationApplication.h"
#include "VehicleKnockbackSettings.h"

#include "ArcadeVehicleMovementComponent.h"
#include "BallControlComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "VehicleHealthComponent.h"

ATestVehiclePawn::ATestVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// A new subobject name avoids reusing the old Blueprint's Box component template.
	CollisionRoot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VehiclePhysicsBody"));
	SetRootComponent(CollisionRoot);
	CollisionRoot->SetMobility(EComponentMobility::Movable);
	CollisionRoot->SetHiddenInGame(true);
	CollisionRoot->SetCollisionProfileName(TEXT("PhysicsActor"));
	CollisionRoot->SetSimulatePhysics(true);
	CollisionRoot->SetEnableGravity(true);
	CollisionRoot->SetLinearDamping(0.15f);
	CollisionRoot->SetAngularDamping(0.8f);
	CollisionRoot->BodyInstance.SetMassOverride(800.0f, true);
	CollisionRoot->SetNotifyRigidBodyCollision(true);
	CollisionRoot->BodyInstance.bUseCCD = true;
	CollisionRoot->OnComponentHit.AddDynamic(this, &ThisClass::OnCollisionRootHit);

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

	BallControlPoint = CreateDefaultSubobject<USceneComponent>(TEXT("BallControlPoint"));
	BallControlPoint->SetupAttachment(CollisionRoot);
	BallControlPoint->SetRelativeLocation(FVector(170.0f, 0.0f, 20.0f));

	BallConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("BallConstraint"));
	BallConstraint->SetupAttachment(BallControlPoint);
	BallConstraint->SetRelativeTransform(FTransform::Identity);
	BallConstraint->SetHiddenInGame(true);

	BallControl = CreateDefaultSubobject<UBallControlComponent>(TEXT("BallControl"));
	BallControl->SetVehicleComponents(CollisionRoot, BallControlPoint, BallConstraint);
	Health = CreateDefaultSubobject<UVehicleHealthComponent>(TEXT("Health"));

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

void ATestVehiclePawn::BeginPlay()
{
	const UStaticMesh* PhysicsMesh = CollisionRoot->GetStaticMesh();
	const UBodySetup* BodySetup = PhysicsMesh ? PhysicsMesh->GetBodySetup() : nullptr;
	if (!BodySetup
		|| BodySetup->AggGeom.GetElementCount() == 0
		|| BodySetup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s: assign a Static Mesh with simple collision to VehiclePhysicsBody. "
				"Use Simple And Complex (not Use Complex Collision As Simple). Vehicle physics is disabled."),
			*GetNameSafe(this));
		CollisionRoot->SetSimulatePhysics(false);
		CollisionRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	else if (BodySetup->AggGeom.ConvexElems.Num() == 0)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s: VehiclePhysicsBody has simple collision but no convex hull. "
				"Generate a rounded convex hull for the wall-sliding test."),
			*GetNameSafe(this));
	}

	Super::BeginPlay();
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
	if (!bConfigurationValid) { return; }

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
	if (!bConfigurationValid || !IsLocallyControlled() || !DefaultMappingContext)
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

bool ATestVehiclePawn::ApplyDefinition(bool bPreview)
{
	if (ConfigurationSource == EVehicleConfigurationSource::Legacy) { return true; }
	FString Error;
	if (ConfigurationSource != EVehicleConfigurationSource::Definition || !Definition || !Definition->Validate(Error)
		|| Definition->CollisionShape != EVehicleCollisionShape::ConvexMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: invalid VehicleDefinition %s: %s. No default fallback."),
			*GetName(), *GetNameSafe(Definition), *Error);
		return false;
	}
	CollisionRoot->SetStaticMesh(Definition->CollisionMesh);
	VehicleConfiguration::ApplyBody(*CollisionRoot, Definition->Physics);
	VehicleConfiguration::ApplyVisual(*VisualMesh, Definition->Visual);
	BallControlPoint->SetRelativeTransform(Definition->BallControlPoint);
	ForwardArrow->SetRelativeTransform(Definition->ForwardArrowTransform);
	ForwardArrow->ArrowColor = Definition->ForwardArrowColor;
	ForwardArrow->ArrowSize = Definition->ForwardArrowSize;
	ForwardArrow->ArrowLength = Definition->ForwardArrowLength;
	ForwardArrow->SetVisibility(Definition->bForwardArrowVisible);
	ForwardArrow->SetHiddenInGame(Definition->bForwardArrowHiddenInGame);
	const UVehicleLocalConfig* Local = Definition->LocalConfig;
	VehicleConfiguration::ApplyCamera(*CameraBoom, *TopDownCamera, Local->Camera);
	if (!bPreview)
	{
		verify(ArcadeMovement->ApplyConfiguration(Definition->Movement));
		verify(BallControl->ApplyConfiguration(Definition->BallControl));
		verify(Health->ApplyConfiguration(Definition->Health));
		DefaultMappingContext = Local->DefaultMappingContext;
		SteeringAction = Local->SteeringAction;
		ThrottleAction = Local->ThrottleAction;
		BrakeAction = Local->BrakeAction;
		HandbrakeAction = Local->HandbrakeAction;
		LaunchAction = Local->LaunchAction;
		UE_LOG(LogTemp, Log, TEXT("%s: using VehicleDefinition %s."), *GetName(), *GetNameSafe(Definition));
	}
	return true;
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

	if (BallControl)
	{
		BallControl->LaunchHeldBall();
	}
}

void ATestVehiclePawn::OnLaunchCompleted(const FInputActionValue& Value)
{
	CurrentInputCommand.bLaunch = false;
	PushInputCommand();
}

void ATestVehiclePawn::OnCollisionRootHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (BallControl)
	{
		BallControl->HandleVehicleCollision(NormalImpulse, Hit);
	}
}


void ATestVehiclePawn::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!HasActorBegunPlay() && !GetWorld()->IsGameWorld()) { ApplyDefinition(true); }
}

void ATestVehiclePawn::PreInitializeComponents()
{
	bConfigurationValid = ApplyDefinition(false);
	if (!GetMutableDefault<UVehicleKnockbackSettings>()->InitializeRules(GetWorld())) { bConfigurationValid = false; }
	if (!bConfigurationValid)
	{
		CollisionRoot->SetSimulatePhysics(false);
		CollisionRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ArcadeMovement->SetComponentTickEnabled(false);
	}
	Super::PreInitializeComponents();
}

#if WITH_EDITOR
bool ATestVehiclePawn::CanEditChange(const FProperty* Property) const
{
	if (ConfigurationSource == EVehicleConfigurationSource::Definition && Property
		&& Property->GetOwnerClass() == StaticClass() && Property->HasAnyPropertyFlags(CPF_Edit)
		&& Property->GetFName() != GET_MEMBER_NAME_CHECKED(ATestVehiclePawn, ConfigurationSource)
		&& Property->GetFName() != GET_MEMBER_NAME_CHECKED(ATestVehiclePawn, Definition)) { return false; }
	return Super::CanEditChange(Property);
}
#endif