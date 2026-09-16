// Copyright Epic Games, Inc. All Rights Reserved.

#include "BasicBallActor.h"

#include "CombatResolver.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	const TCHAR* LexToString(const EBasicBallState State)
	{
		switch (State)
		{
		case EBasicBallState::Free:
			return TEXT("Free");
		case EBasicBallState::Controlled:
			return TEXT("Controlled");
		case EBasicBallState::Launched:
			return TEXT("Launched");
		default:
			return TEXT("Unknown");
		}
	}
}

ABasicBallActor::ABasicBallActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	PhysicsRoot = CreateDefaultSubobject<USphereComponent>(TEXT("PhysicsRoot"));
	SetRootComponent(PhysicsRoot);
	PhysicsRoot->InitSphereRadius(50.0f);
	PhysicsRoot->SetCollisionProfileName(TEXT("PhysicsActor"));
	PhysicsRoot->SetSimulatePhysics(true);
	PhysicsRoot->SetEnableGravity(true);
	PhysicsRoot->SetLinearDamping(0.25f);
	PhysicsRoot->SetAngularDamping(0.1f);
	PhysicsRoot->SetMassOverrideInKg(NAME_None, 35.0f, true);
	PhysicsRoot->SetNotifyRigidBodyCollision(true);
	PhysicsRoot->BodyInstance.bUseCCD = true;
	PhysicsRoot->OnComponentHit.AddDynamic(this, &ThisClass::OnPhysicsRootHit);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(PhysicsRoot);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetSimulatePhysics(false);
}

void ABasicBallActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABasicBallActor, BallState);
	DOREPLIFETIME(ABasicBallActor, ControlledBy);
	DOREPLIFETIME(ABasicBallActor, LaunchedBy);
}

bool ABasicBallActor::CanBeControlledBy(const AActor* CandidateVehicle) const
{
	if (!IsValid(CandidateVehicle) || BallState != EBasicBallState::Free)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const double CurrentTime = World ? World->GetTimeSeconds() : 0.0;
	if (CurrentTime < GlobalPickupLockedUntil)
	{
		return false;
	}

	return CandidateVehicle != ReacquireLockedVehicle.Get() || CurrentTime >= ReacquireLockedUntil;
}

bool ABasicBallActor::BeginControl(AActor* NewHolder)
{
	if (!HasAuthority() || !CanBeControlledBy(NewHolder))
	{
		return false;
	}

	StopLowSpeedMonitor();
	ControlledBy = NewHolder;
	LaunchedBy = nullptr;
	ReacquireLockedVehicle.Reset();
	ReacquireLockedUntil = 0.0;
	GlobalPickupLockedUntil = 0.0;
	SetBallState(EBasicBallState::Controlled);
	return true;
}

bool ABasicBallActor::LaunchFromControl(
	AActor* ExpectedHolder,
	AActor* NewLauncher,
	const FVector& InitialVelocity)
{
	if (!HasAuthority()
		|| BallState != EBasicBallState::Controlled
		|| ControlledBy != ExpectedHolder
		|| !IsValid(NewLauncher))
	{
		return false;
	}

	ControlledBy = nullptr;
	LaunchedBy = NewLauncher;
	ReacquireLockedVehicle.Reset();
	ReacquireLockedUntil = 0.0;
	GlobalPickupLockedUntil = 0.0;
	SetBallState(EBasicBallState::Launched);

	PhysicsRoot->WakeAllRigidBodies();
	PhysicsRoot->SetPhysicsLinearVelocity(InitialVelocity);
	AccumulatedLowSpeedTime = 0.0f;
	StartLowSpeedMonitor();
	return true;
}

bool ABasicBallActor::ReleaseFromControl(AActor* ExpectedHolder, const float ReacquireLockDuration)
{
	if (!HasAuthority()
		|| BallState != EBasicBallState::Controlled
		|| ControlledBy != ExpectedHolder)
	{
		return false;
	}

	UWorld* World = GetWorld();
	ControlledBy = nullptr;
	LaunchedBy = nullptr;
	ReacquireLockedVehicle = ExpectedHolder;
	ReacquireLockedUntil = (World ? World->GetTimeSeconds() : 0.0) + FMath::Max(0.0f, ReacquireLockDuration);
	GlobalPickupLockedUntil = 0.0;
	SetBallState(EBasicBallState::Free);
	return true;
}

void ABasicBallActor::AddReleaseImpulse(const FVector& Impulse)
{
	if (HasAuthority() && PhysicsRoot && PhysicsRoot->IsSimulatingPhysics())
	{
		PhysicsRoot->WakeAllRigidBodies();
		PhysicsRoot->AddImpulse(Impulse);
	}
}

void ABasicBallActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopLowSpeedMonitor();
	Super::EndPlay(EndPlayReason);
}

void ABasicBallActor::OnRep_BallState()
{
	// Presentation hooks will be added separately; replicated state is explicit now.
}

void ABasicBallActor::SetBallState(const EBasicBallState NewState)
{
	if (BallState == NewState)
	{
		return;
	}

	const EBasicBallState PreviousState = BallState;
	BallState = NewState;
	ForceNetUpdate();

	if (bLogStateChanges)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Ball %s: %s -> %s"),
			*GetName(),
			LexToString(PreviousState),
			LexToString(NewState));
	}
}

void ABasicBallActor::StartLowSpeedMonitor()
{
	if (UWorld* World = GetWorld())
	{
		const float Interval = FMath::Max(0.01f, LowSpeedCheckInterval);
		World->GetTimerManager().SetTimer(
			LowSpeedTimerHandle,
			this,
			&ThisClass::CheckLaunchedLowSpeed,
			Interval,
			true);
	}
}

void ABasicBallActor::StopLowSpeedMonitor()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LowSpeedTimerHandle);
	}
	AccumulatedLowSpeedTime = 0.0f;
}

void ABasicBallActor::CheckLaunchedLowSpeed()
{
	if (!HasAuthority() || BallState != EBasicBallState::Launched)
	{
		StopLowSpeedMonitor();
		return;
	}

	const float Interval = FMath::Max(0.01f, LowSpeedCheckInterval);
	const float SpeedSquared = PhysicsRoot->GetPhysicsLinearVelocity().SizeSquared();
	if (SpeedSquared <= FMath::Square(LowSpeedThreshold))
	{
		AccumulatedLowSpeedTime += Interval;
		if (AccumulatedLowSpeedTime >= LowSpeedRequiredDuration)
		{
			FinishLaunchAsFree();
		}
	}
	else
	{
		AccumulatedLowSpeedTime = 0.0f;
	}
}

void ABasicBallActor::FinishLaunchAsFree()
{
	UWorld* World = GetWorld();
	StopLowSpeedMonitor();
	ControlledBy = nullptr;
	LaunchedBy = nullptr;
	ReacquireLockedVehicle.Reset();
	ReacquireLockedUntil = 0.0;
	GlobalPickupLockedUntil =
		(World ? World->GetTimeSeconds() : 0.0) + FMath::Max(0.0f, PostLaunchPickupLockDuration);
	SetBallState(EBasicBallState::Free);
}

void ABasicBallActor::OnPhysicsRootHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!HasAuthority() || bResolvingDamageHit || BallState != EBasicBallState::Launched
		|| !IsValid(OtherActor) || OtherActor == LaunchedBy)
	{
		return;
	}

	bResolvingDamageHit = true;
	FVehicleHitContext Context;
	Context.SourceActor = this;
	Context.TargetActor = OtherActor;
	Context.InstigatorActor = LaunchedBy;
	Context.TargetPhysicsBody = OtherComponent;
	Context.ImpactPoint = Hit.ImpactPoint;
	Context.ImpactNormal = Hit.ImpactNormal;
	Context.NormalImpulse = NormalImpulse;

	if (FCombatResolver::ResolveVehicleHit(Context, VehicleHitDamage, VehicleHitAdditionalImpulse))
	{
		if (bLogDamageHits)
		{
			UE_LOG(LogTemp, Log, TEXT("Ball %s hit vehicle %s, launcher %s, damage %.1f"),
				*GetName(), *GetNameSafe(OtherActor), *GetNameSafe(LaunchedBy.Get()), VehicleHitDamage);
		}
		FinishLaunchAsFree();
	}
	bResolvingDamageHit = false;
}
