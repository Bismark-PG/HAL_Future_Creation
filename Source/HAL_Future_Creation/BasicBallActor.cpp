// Copyright Epic Games, Inc. All Rights Reserved.

#include "BasicBallActor.h"
#include "UObject/UnrealType.h"
#include "VehicleDefinition.h"
#include "VehicleConfigurationApplication.h"
#include "VehicleKnockbackSettings.h"

#include "CombatResolver.h"
#include "ArcadeVehicleMovementComponent.h"
#include "BallControlComponent.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "Physics/NetworkPhysicsSettingsComponent.h"
#include "TimerManager.h"

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarHALBallNetLog(
	TEXT("hal.BallNetLog"), 0,
	TEXT("Log discrete ball state snapshots on server and clients: 0=off, 1=on."));
#endif

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

	const TCHAR* LexToString(const EVehicleKnockbackTier Tier)
	{
		switch (Tier)
		{
		case EVehicleKnockbackTier::Light:
			return TEXT("Light");
		case EVehicleKnockbackTier::Medium:
			return TEXT("Medium");
		case EVehicleKnockbackTier::Heavy:
			return TEXT("Heavy");
		default:
			return TEXT("Unknown");
		}
	}
}

ABasicBallActor::ABasicBallActor()
{
	bReplicates = true;
	SetReplicateMovement(true);
	SetPhysicsReplicationMode(EPhysicsReplicationMode::PredictiveInterpolation);

	PhysicsRoot = CreateDefaultSubobject<USphereComponent>(TEXT("PhysicsRoot"));
	SetRootComponent(PhysicsRoot);
	PhysicsRoot->InitSphereRadius(50.0f);
	PhysicsRoot->SetCollisionProfileName(TEXT("PhysicsActor"));
	PhysicsRoot->SetSimulatePhysics(true);
	PhysicsRoot->SetEnableGravity(true);
	PhysicsRoot->SetLinearDamping(0.25f);
	PhysicsRoot->SetAngularDamping(0.1f);
	PhysicsRoot->BodyInstance.SetMassOverride(35.0f, true);
	PhysicsRoot->SetNotifyRigidBodyCollision(true);
	PhysicsRoot->BodyInstance.bUseCCD = true;
	PhysicsRoot->OnComponentHit.AddDynamic(this, &ThisClass::OnPhysicsRootHit);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(PhysicsRoot);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetSimulatePhysics(false);
	NetworkPhysicsSettings = CreateDefaultSubobject<UNetworkPhysicsSettingsComponent>(TEXT("BallNetworkPhysicsSettings"));
}

void ABasicBallActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABasicBallActor, RepState);
}

bool ABasicBallActor::CanBeControlledBy(const AActor* CandidateVehicle) const
{
	if (!HasAuthority() || !bConfigurationValid || !VehicleConfiguration::IsReadyForGameplay(CandidateVehicle)
		|| RepState.State != EBasicBallState::Free)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const double CurrentTime = World ? World->GetTimeSeconds() : 0.0;
	if (CurrentTime < RepState.GlobalPickupLockEndServerTime)
	{
		return false;
	}

	return CandidateVehicle != RepState.ReacquireLockedVehicle || CurrentTime >= RepState.ReacquireLockEndServerTime;
}

bool ABasicBallActor::ApplyDefinition(bool bPreview)
{
	if (ConfigurationSource == EVehicleConfigurationSource::Legacy) { return true; }
	FString Error;
	if (ConfigurationSource != EVehicleConfigurationSource::Definition || !Definition || !Definition->Validate(Error))
	{
		UE_LOG(LogTemp, Error, TEXT("%s: invalid BallDefinition %s: %s. No default fallback."),
			*GetName(), *GetNameSafe(Definition), *Error);
		return false;
	}
	PhysicsRoot->SetSphereRadius(Definition->Radius);
	VehicleConfiguration::ApplyBody(*PhysicsRoot, Definition->Physics);
	VehicleConfiguration::ApplyVisual(*VisualMesh, Definition->Visual);
	if (!bPreview)
	{
		LowSpeedThreshold = Definition->Gameplay.State.LowSpeedThreshold;
		LowSpeedRequiredDuration = Definition->Gameplay.State.LowSpeedRequiredDuration;
		LowSpeedCheckInterval = Definition->Gameplay.State.LowSpeedCheckInterval;
		PostLaunchPickupLockDuration = Definition->Gameplay.Acquisition.PostLaunchPickupLockDuration;
		VehicleHitDamage = Definition->Gameplay.Damage.VehicleHitDamage;
		KnockbackStrengthMultiplier = Definition->Gameplay.Damage.KnockbackStrengthMultiplier;
		bLogDamageHits = Definition->Gameplay.Debug.bLogDamageHits;
		bLogStateChanges = Definition->Gameplay.Debug.bLogStateChanges;
	}
	return true;
}

bool ABasicBallActor::BeginControl(AActor* NewHolder)
{
	if (!HasAuthority() || !CanBeControlledBy(NewHolder))
	{
		return false;
	}

	StopLowSpeedMonitor();
	FBallRepState Next;
	Next.State = EBasicBallState::Controlled;
	Next.Holder = NewHolder;
	CommitRepState(Next, NewHolder);
	return true;
}

bool ABasicBallActor::LaunchFromControl(
	AActor* ExpectedHolder,
	AActor* NewLauncher,
	const FVector& InitialVelocity)
{
	if (!HasAuthority()
		|| RepState.State != EBasicBallState::Controlled
		|| RepState.Holder != ExpectedHolder
		|| !IsValid(NewLauncher))
	{
		return false;
	}

	FBallRepState Next;
	Next.State = EBasicBallState::Launched;
	Next.LastLauncherPawn = NewLauncher;
	if (const APawn* LauncherPawn = Cast<APawn>(NewLauncher))
	{
		if (const APlayerState* PlayerState = LauncherPawn->GetPlayerState())
		{
			Next.LastLauncherPlayerId = PlayerState->GetPlayerId();
		}
	}
	CommitRepState(Next, ExpectedHolder);

	PhysicsRoot->WakeAllRigidBodies();
	PhysicsRoot->SetPhysicsLinearVelocity(InitialVelocity);
	AccumulatedLowSpeedTime = 0.0f;
	StartLowSpeedMonitor();
	return true;
}

bool ABasicBallActor::ReleaseFromControl(AActor* ExpectedHolder, const float ReacquireLockDuration)
{
	if (!HasAuthority()
		|| RepState.State != EBasicBallState::Controlled
		|| RepState.Holder != ExpectedHolder)
	{
		return false;
	}

	UWorld* World = GetWorld();
	FBallRepState Next;
	Next.ReacquireLockedVehicle = ExpectedHolder;
	Next.ReacquireLockEndServerTime = (World ? World->GetTimeSeconds() : 0.0f)
		+ FMath::Max(0.0f, ReacquireLockDuration);
	CommitRepState(Next, ExpectedHolder);
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

void ABasicBallActor::BeginPlay()
{
	Super::BeginPlay();
	AuthoredCollisionResponses = PhysicsRoot->GetCollisionResponseToChannels();
	bAuthoredGravityEnabled = PhysicsRoot->IsGravityEnabled();
	ApplyReplicatedPresentation();
}

void ABasicBallActor::OnRep_BallRepState()
{
	if (!HasActorBegunPlay()) { return; }
	ApplyReplicatedPresentation();
#if !UE_BUILD_SHIPPING
	if (CVarHALBallNetLog.GetValueOnGameThread() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Ball state received: %s State=%s Seq=%u Holder=%s Launcher=%s PlayerId=%d ServerFrame=%d Sim=%d Collision=%d Gravity=%d"),
			*GetName(), LexToString(RepState.State), RepState.StateSequence,
			*GetNameSafe(RepState.Holder.Get()), *GetNameSafe(RepState.LastLauncherPawn.Get()),
			RepState.LastLauncherPlayerId, RepState.ServerPhysicsFrame,
			PhysicsRoot->IsSimulatingPhysics() ? 1 : 0,
			static_cast<int32>(PhysicsRoot->GetCollisionEnabled()),
			PhysicsRoot->IsGravityEnabled() ? 1 : 0);
	}
#endif
}

void ABasicBallActor::ApplyReplicatedPresentation()
{
	if (HasAuthority()) { return; }
	if (bHasAppliedRepState
		&& RepState.StateSequence != LastAppliedStateSequence
		&& static_cast<int32>(RepState.StateSequence - LastAppliedStateSequence) <= 0)
	{
		RepState = LastAppliedRepState;
		return;
	}
	bHasAppliedRepState = true;
	LastAppliedStateSequence = RepState.StateSequence;
	LastAppliedRepState = RepState;
	if (RepState.State == EBasicBallState::Controlled)
	{
		if (!bControlledPresentationActive)
		{
			// Keep Chaos simulation alive: the existing replicated rigid-body
			// targets and PI now display the server's actual acquisition path.
			// Keep collision *enabled* so Chaos retains the rigid-body state,
			// but ignore every channel so it cannot push the predicted vehicle.
			PhysicsRoot->SetCollisionResponseToAllChannels(ECR_Ignore);
			PhysicsRoot->SetEnableGravity(false);
			bControlledPresentationActive = true;
			if (!PhysicsRoot->IsSimulatingPhysics())
			{
				UE_LOG(LogTemp, Error, TEXT("%s: controlled client ball has no physics state for authoritative PI. CollisionEnabled=%d"),
					*GetName(), static_cast<int32>(PhysicsRoot->GetCollisionEnabled()));
			}
		}
	}
	else
	{
		StopControlledPresentation();
	}
}

void ABasicBallActor::StopControlledPresentation()
{
	if (!bControlledPresentationActive) { return; }
	// The same physics-replication stream continues across the state change.
	PhysicsRoot->SetEnableGravity(bAuthoredGravityEnabled);
	PhysicsRoot->SetCollisionResponseToChannels(AuthoredCollisionResponses);
	bControlledPresentationActive = false;
}

void ABasicBallActor::CommitRepState(const FBallRepState& NewState, const AActor* PhysicsFrameSource)
{
	check(HasAuthority());
	const EBasicBallState PreviousState = RepState.State;
	FBallRepState Committed = NewState;
	Committed.StateSequence = RepState.StateSequence + 1;
	if (const UArcadeVehicleMovementComponent* Movement = IsValid(PhysicsFrameSource)
		? PhysicsFrameSource->FindComponentByClass<UArcadeVehicleMovementComponent>() : nullptr)
	{
		Committed.ServerPhysicsFrame = Movement->GetLastPhysicsFrame();
	}
	RepState = Committed;
	ForceNetUpdate();

	bool bShouldLogState = bLogStateChanges;
#if !UE_BUILD_SHIPPING
	bShouldLogState |= CVarHALBallNetLog.GetValueOnGameThread() > 0;
#endif
	if (bShouldLogState)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Ball %s: %s -> %s Seq=%u Holder=%s Launcher=%s PlayerId=%d ServerFrame=%d"),
			*GetName(),
			LexToString(PreviousState),
			LexToString(RepState.State),
			RepState.StateSequence,
			*GetNameSafe(RepState.Holder.Get()),
			*GetNameSafe(RepState.LastLauncherPawn.Get()),
			RepState.LastLauncherPlayerId,
			RepState.ServerPhysicsFrame);
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
	if (!HasAuthority() || RepState.State != EBasicBallState::Launched)
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
	FBallRepState Next;
	Next.GlobalPickupLockEndServerTime = (World ? World->GetTimeSeconds() : 0.0f)
		+ FMath::Max(0.0f, PostLaunchPickupLockDuration);
	CommitRepState(Next, nullptr);
}

void ABasicBallActor::OnPhysicsRootHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!HasAuthority() || bResolvingDamageHit || RepState.State != EBasicBallState::Launched
		|| !IsValid(OtherActor) || OtherActor == RepState.LastLauncherPawn)
	{
		return;
	}

	bResolvingDamageHit = true;
	FVehicleHitContext Context;
	Context.SourceActor = this;
	Context.TargetActor = OtherActor;
	Context.InstigatorActor = RepState.LastLauncherPawn;
	Context.TargetPhysicsBody = OtherComponent;
	Context.ImpactPoint = Hit.ImpactPoint;
	Context.ImpactNormal = Hit.ImpactNormal;
	Context.NormalImpulse = NormalImpulse;
	Context.SourceVelocity = PhysicsRoot->GetPhysicsLinearVelocity();
	Context.SourceMass = PhysicsRoot->GetMass();
	if (IsValid(OtherComponent) && OtherComponent->IsSimulatingPhysics())
	{
		Context.TargetVelocityAtImpactPoint =
			OtherComponent->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint);
	}

	const FVehicleHitResolution Resolution = FCombatResolver::ResolveVehicleHit(
		Context,
		VehicleHitDamage,
		KnockbackStrengthMultiplier);
	if (Resolution.bResolved)
	{
		if (bLogDamageHits)
		{
			UE_LOG(LogTemp, Log, TEXT("Ball %s hit vehicle %s, launcher %s, damage %.1f, impact %.1f, tier %s"),
				*GetName(),
				*GetNameSafe(OtherActor),
				*GetNameSafe(RepState.LastLauncherPawn.Get()),
				VehicleHitDamage,
				Resolution.ImpactSpeed,
				LexToString(Resolution.KnockbackTier));
		}
		FinishLaunchAsFree();
	}
	bResolvingDamageHit = false;
}

void ABasicBallActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!HasActorBegunPlay() && !GetWorld()->IsGameWorld()) { ApplyDefinition(true); }
}

void ABasicBallActor::PreInitializeComponents()
{
	if (GetNetMode() != NM_Standalone && GetLocalRole() == ROLE_SimulatedProxy)
	{
		// Preserve PI even if an older Blueprint serialized the actor's former default mode.
		// A per-ball settings asset may override this in its component BeginPlay.
		SetPhysicsReplicationMode(EPhysicsReplicationMode::PredictiveInterpolation);
	}
	bConfigurationValid = ApplyDefinition(false);
	if (!GetMutableDefault<UVehicleKnockbackSettings>()->InitializeRules()) { bConfigurationValid = false; }
	if (!bConfigurationValid)
	{
		PhysicsRoot->SetSimulatePhysics(false);
		PhysicsRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	Super::PreInitializeComponents();
}

#if WITH_EDITOR
bool ABasicBallActor::CanEditChange(const FProperty* Property) const
{
	if (ConfigurationSource == EVehicleConfigurationSource::Definition && Property
		&& Property->GetOwnerClass() == StaticClass() && Property->HasAnyPropertyFlags(CPF_Edit)
		&& Property->GetFName() != GET_MEMBER_NAME_CHECKED(ABasicBallActor, ConfigurationSource)
		&& Property->GetFName() != GET_MEMBER_NAME_CHECKED(ABasicBallActor, Definition)) { return false; }
	return Super::CanEditChange(Property);
}
#endif
