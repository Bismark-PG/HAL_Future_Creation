// Copyright Epic Games, Inc. All Rights Reserved.

#include "BasicBallActor.h"
#include "HALTornado.h"
#include "HALWaterCannon.h"
#include "CoreGlobals.h"
#include "HALBattleTopComponent.h"
#include "HALBullChargeComponent.h"
#include "HALGameFlowPermissions.h"
#include "HALAbilitySystemComponent.h"
#include "HALBallPresentationComponent.h"
#include "UObject/UnrealType.h"
#include "VehicleDefinition.h"
#include "VehicleConfigurationApplication.h"
#include "VehicleKnockbackSettings.h"

#include "CombatResolver.h"
#include "ArcadeVehicleMovementComponent.h"
#include "BallControlComponent.h"
#include "HALGameplayTags.h"
#include "HALMagnetBeamPresentationComponent.h"
#include "HALBallComicFXComponent.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/ActorChannel.h"
#include "Engine/NetDriver.h"
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
	// The top-down camera can discount a ball at the side of the screen even while its holder is turning.
	constexpr float HeldBallPriorityMultiplier = 4.0f;

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
	BallPresentation = CreateDefaultSubobject<UHALBallPresentationComponent>(TEXT("BallPresentation"));
	MagnetBeam = CreateDefaultSubobject<UHALMagnetBeamPresentationComponent>(TEXT("MagnetBeam"));
	ComicFX = CreateDefaultSubobject<UHALBallComicFXComponent>(TEXT("ComicFX"));
}

void ABasicBallActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABasicBallActor, bPrimaryCameraTarget);

	DOREPLIFETIME(ABasicBallActor, RepState);
}

float ABasicBallActor::GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, AActor* Viewer,
	AActor* ViewTarget, UActorChannel* InChannel, float Time, bool bLowBandwidth)
{
	const float BasePriority = Super::GetNetPriority(ViewPos, ViewDir, Viewer, ViewTarget, InChannel, Time, bLowBandwidth);
	const bool bReleasePending = InChannel && LastControlEndNetSeconds >= 0.0
		&& InChannel->LastUpdateTime <= LastControlEndNetSeconds;
	if (RepState.State == EBasicBallState::Controlled || bReleasePending)
	{
		// Keep UE's per-connection time-since-last-update weighting, including for remote holders.
		return FMath::Max(BasePriority, NetPriority * HeldBallPriorityMultiplier * Time);
	}
	return BasePriority;
}

bool ABasicBallActor::CanBeControlledBy(const AActor* CandidateVehicle) const
{
	if (!AllowsAutomaticPickup()) { return false; }
	if (!HALGameFlow::Permissions(GetWorld(),Cast<APawn>(CandidateVehicle)).bBallControl) { return false; }
	if (UHALAbilitySystemComponent::IsActorStunned(CandidateVehicle)) { return false; }
	if (!HasAuthority() || !CanUseBall() || !bConfigurationValid || !VehicleConfiguration::IsReadyForGameplay(CandidateVehicle)
		|| RepState.State != EBasicBallState::Free)
	{
		return false;
	}
	return PassesPickupLocks(CandidateVehicle);
}

bool ABasicBallActor::PassesPickupLocks(const AActor* CandidateVehicle) const
{
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
		RuntimeDamageConfig = Definition->Gameplay.Damage;
		RuntimeImpactConfig = Definition->Gameplay.Impact;
		bLogDamageHits = Definition->Gameplay.Debug.bLogDamageHits;
		bLogStateChanges = Definition->Gameplay.Debug.bLogStateChanges;
		BallPresentation->Configure(Definition->Presentation);
	}
	return true;
}

bool ABasicBallActor::BeginControl(AActor* NewHolder)
{
	if (!HasAuthority() || !CanBeControlledBy(NewHolder))
	{
		return false;
	}

	CommitContactHolder(*NewHolder);
	return true;
}

void ABasicBallActor::CommitContactHolder(AActor& Holder)
{
	StopLowSpeedMonitor();
	FBallRepState Next;
	Next.State = EBasicBallState::Controlled;
	Next.Holder = &Holder;
	CommitRepState(Next, &Holder);
}

bool ABasicBallActor::LaunchFromControl(
	AActor* ExpectedHolder,
	AActor* NewLauncher,
	const FVector& InitialVelocity)
{
	if (!HALGameFlow::Permissions(GetWorld(),Cast<APawn>(NewLauncher)).bBallControl) { return false; }
	if (UHALAbilitySystemComponent::IsActorStunned(ExpectedHolder)) { return false; }
	if (!HasAuthority()
		|| RepState.State != EBasicBallState::Controlled
		|| RepState.Holder != ExpectedHolder
		|| !IsValid(NewLauncher) || InitialVelocity.ContainsNaN())
	{
		return false;
	}
	return CommitLaunch(NewLauncher, InitialVelocity, ExpectedHolder);
}

bool ABasicBallActor::LaunchFreeFromBattleTop(AActor* NewLauncher, const FVector& InitialVelocity)
{
	return UHALBattleTopComponent::IsActiveOn(NewLauncher) && LaunchFreeFromVehicleSkill(NewLauncher, InitialVelocity);
}

bool ABasicBallActor::LaunchFreeFromVehicleSkill(AActor* NewLauncher, const FVector& InitialVelocity)
{
	if (!HasAuthority() || !bConfigurationValid || RepState.State != EBasicBallState::Free
		|| LastBecameFreeGameFrame == GFrameCounter || !VehicleConfiguration::IsReadyForGameplay(NewLauncher)
		|| (!UHALBattleTopComponent::IsActiveOn(NewLauncher) && !UHALBullChargeComponent::IsActiveOn(NewLauncher))
		|| !HALGameFlow::Permissions(GetWorld(), Cast<APawn>(NewLauncher)).bBallControl
		|| !PassesPickupLocks(NewLauncher) || InitialVelocity.ContainsNaN() || InitialVelocity.IsNearlyZero()) { return false; }
	return CommitLaunch(NewLauncher, InitialVelocity, NewLauncher);
}

bool ABasicBallActor::LaunchFromWaterCannon(AHALWaterCannon& Source, const FVector& Velocity)
{
	AActor* Launcher = Source.GetOwner();
	if (!HasAuthority() || !bConfigurationValid || bResolvingDamageHit || !Source.AuthorizesBallContact(*this)
		|| !HALGameFlow::Permissions(GetWorld(),Cast<APawn>(Launcher)).bBallControl
		|| Velocity.ContainsNaN() || Velocity.IsNearlyZero()) { return false; }
	if (RepState.State == EBasicBallState::Free && (LastBecameFreeGameFrame == GFrameCounter || !PassesPickupLocks(Launcher))) { return false; }
	if (RepState.State != EBasicBallState::Free && RepState.State != EBasicBallState::Launched) { return false; }
	return CommitLaunch(Launcher, Velocity, Launcher);
}

bool ABasicBallActor::DeflectByTornado(AHALTornado& Source, const FVector& InitialVelocity)
{
	if (!HasAuthority() || !bConfigurationValid || bResolvingDamageHit || !Source.AuthorizesBallContact(*this)
		|| InitialVelocity.ContainsNaN() || InitialVelocity.IsNearlyZero()) { return false; }
	if (RepState.State == EBasicBallState::Free)
	{
		if (LastBecameFreeGameFrame == GFrameCounter || !PassesPickupLocks(Source.GetOwner())) { return false; }
		return CommitLaunch(Source.GetOwner(), InitialVelocity, Source.GetOwner());
	}
	if (RepState.State != EBasicBallState::Launched) { return false; }
	StopLowSpeedMonitor();
	FBallRepState Free;
	Free.GlobalPickupLockEndServerTime = GetWorld()->GetTimeSeconds() + Source.DisarmPickupLock();
	// Atomic state commit retires LaunchAction and its damage payload before the new velocity.
	CommitRepState(Free, Source.GetOwner());
	PhysicsRoot->WakeAllRigidBodies(); PhysicsRoot->SetPhysicsLinearVelocity(InitialVelocity);
	AccumulatedLowSpeedTime = 0;
	return true;
}

bool ABasicBallActor::CommitLaunch(AActor* NewLauncher, const FVector& InitialVelocity, const AActor* FrameSource)
{
	if (!AllowsNormalLaunch() || !CanUseBall()) { return false; }
	FHALAttackSourceSnapshot Attack;
	FHALCollisionImpactRules ImpactRules;
	if (!GetMutableDefault<UVehicleKnockbackSettings>()->InitializeRules()
		|| !GetDefault<UVehicleKnockbackSettings>()->GetCollisionImpactRules(ImpactRules)
		|| (ImpactRules.Version == EHALImpactVersion::TieredV1 && RuntimeImpactConfig.Version != ImpactRules.Version)) { return false; }
	if (RuntimeDamageConfig.Version == EHALCombatConfigVersion::AttackSnapshotV1)
	{
		if (!FCombatResolver::CaptureVehicleAttack(*this, *NewLauncher, RuntimeDamageConfig.ActionMultiplier,
			RuntimeDamageConfig.ActionMultiplierDelta, Attack))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: scaled launch rejected; migrate the vehicle combat config before the ball."), *GetName());
			return false;
		}
	}
	else
	{
		Attack.SourceActor = this;
		Attack.InstigatorActor = NewLauncher;
		Attack.FixedDamage = VehicleHitDamage;
		if (const APawn* Pawn = Cast<APawn>(NewLauncher))
		{
			Attack.InstigatorPlayerState = Pawn->GetPlayerState();
			if (const APlayerState* State = Pawn->GetPlayerState()) { Attack.InstigatorPlayerId = State->GetPlayerId(); }
		}
	}
	Attack.SourceTag = HALGameplayTags::Combat_Source_Ball;
	Attack.Impact = RuntimeImpactConfig;
	Attack.EffectTags = RuntimeDamageConfig.EffectTags;
	Attack.bCanHitInstigator = false;
	return DeliverLaunch(Attack, NewLauncher, InitialVelocity, FrameSource);
}

bool ABasicBallActor::DeliverLaunch(const FHALAttackSourceSnapshot& Attack, AActor* NewLauncher, const FVector& InitialVelocity, const AActor* FrameSource)
{
	// A water intercept changes attack ownership without inserting an observable Free state.
	if (!LaunchAction.Replace(*this, Attack)) { return false; }
	FBallRepState Next;
	Next.State = EBasicBallState::Launched;
	Next.AttackSnapshot = LaunchAction.GetSnapshot();
	Next.LastLauncherPawn = NewLauncher;
	if (const APawn* LauncherPawn = Cast<APawn>(NewLauncher))
	{
		if (const APlayerState* PlayerState = LauncherPawn->GetPlayerState())
		{
			Next.LastLauncherPlayerId = PlayerState->GetPlayerId();
		}
	}
	CommitRepState(Next, FrameSource);

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

bool ABasicBallActor::HandleVehicleExit(const AActor* LeavingVehicle)
{
	if (!HasAuthority() || !LeavingVehicle) { return false; }
	FBallRepState Next = RepState;
	bool bChanged = false;
	if (Next.Holder == LeavingVehicle)
	{
		Next.Holder = nullptr;
		if (Next.State == EBasicBallState::Controlled)
		{
			Next.State = EBasicBallState::Free;
			const UWorld* World = GetWorld();
			Next.GlobalPickupLockEndServerTime = (World ? World->GetTimeSeconds() : 0.0f)
				+ FMath::Max(0.0f, PostLaunchPickupLockDuration);
		}
		bChanged = true;
	}
	if (Next.ReacquireLockedVehicle == LeavingVehicle)
	{
		Next.ReacquireLockedVehicle = nullptr;
		Next.ReacquireLockEndServerTime = 0.0f;
		bChanged = true;
	}
	if (Next.LastLauncherPawn == LeavingVehicle)
	{
		Next.LastLauncherPawn = nullptr;
		// LastLauncherPlayerId remains stable until this launch actually finishes.
		bChanged = true;
	}
	if (Next.AttackSnapshot.InstigatorActor.Get() == LeavingVehicle)
	{
		Next.AttackSnapshot.InstigatorActor.Reset();
		Next.AttackSnapshot.InstigatorPlayerState.Reset();
		bChanged = true;
	}
	if (!bChanged) { return false; }
	Next.ServerPhysicsFrame = INDEX_NONE;
	CommitRepState(Next, nullptr);
	UE_LOG(LogTemp, Log, TEXT("Ball vehicle-exit cleanup: %s Leaving=%s State=%s Seq=%u Holder=%s Launcher=%s PlayerId=%d"),
		*GetNameSafe(this), *GetNameSafe(LeavingVehicle), LexToString(RepState.State),
		RepState.StateSequence, *GetNameSafe(RepState.Holder.Get()),
		*GetNameSafe(RepState.LastLauncherPawn.Get()), RepState.LastLauncherPlayerId);
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
	if (EndPlayReason == EEndPlayReason::Destroyed && HasAuthority() && IsValid(RepState.Holder))
	{
		if (auto* Control = RepState.Holder->FindComponentByClass<UBallControlComponent>())
		{
			Control->OnHeldBallDestroyed(this);
		}
	}
	LaunchAction.Close();
	StopLowSpeedMonitor();
	Super::EndPlay(EndPlayReason);
}

float ABasicBallActor::GetConfiguredSphereRadius() const
{
	if (ConfigurationSource == EVehicleConfigurationSource::Definition && Definition)
	{
		return Definition->GetScaledPhysicsRadius();
	}
	return PhysicsRoot->GetScaledSphereRadius();
}

void ABasicBallActor::SetPrimaryMatchBall(bool bPrimary)
{
	if (HasAuthority() && bPrimaryCameraTarget != bPrimary) { bPrimaryCameraTarget = bPrimary; ForceNetUpdate(); }
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
	// After ApplyReplicatedPresentation, so an out-of-order snapshot it rejected does not
	// restart the beam on a state the server has already moved past.
	if (MagnetBeam) { MagnetBeam->SyncToBallState(RepState.State, RepState.Holder); }
	if (ComicFX) { ComicFX->SyncToBallState(RepState); }
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
	if (PreviousState == EBasicBallState::Controlled && NewState.State != EBasicBallState::Controlled)
	{
		if (const UNetDriver* Driver = GetNetDriver())
		{
			LastControlEndNetSeconds = Driver->GetElapsedTime();
		}
	}
	if (NewState.State == EBasicBallState::Free && RepState.State != EBasicBallState::Free)
	{ LastBecameFreeGameFrame = GFrameCounter; }
	if (NewState.State != EBasicBallState::Launched) { LaunchAction.Close(); }
	Committed.StateSequence = RepState.StateSequence + 1;
	if (const UArcadeVehicleMovementComponent* Movement = IsValid(PhysicsFrameSource)
		? PhysicsFrameSource->FindComponentByClass<UArcadeVehicleMovementComponent>() : nullptr)
	{
		Committed.ServerPhysicsFrame = Movement->GetLastPhysicsFrame();
	}
	RepState = Committed;
	OnAuthorityBallStateChanged(PreviousState);
	if (BallPresentation) { BallPresentation->NotifyStateChanged(); }
	// The client path below returns early on authority, so the listen-server host would
	// otherwise be the one player who never sees who holds the ball.
	if (MagnetBeam) { MagnetBeam->SyncToBallState(RepState.State, RepState.Holder); }
	if (ComicFX) { ComicFX->SyncToBallState(RepState); }
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
	GroundRollStartedAt = -1.;
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
	GroundRollStartedAt = -1.;
}

bool ABasicBallActor::HasGroundSupport() const
{
	const float Radius = PhysicsRoot->GetScaledSphereRadius();
	const float Skin = FMath::Min(2.f, Radius * .05f);
	const FVector Center = PhysicsRoot->GetComponentLocation();
	// A slightly inset sphere finds ramps and floor seams without treating a side wall as support.
	FCollisionObjectQueryParams Surfaces(ECC_WorldStatic);
	Surfaces.AddObjectTypesToQuery(ECC_WorldDynamic);
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(Hits, Center, Center - FVector::UpVector * (3.f * Skin),
		FQuat::Identity, Surfaces, FCollisionShape::MakeSphere(Radius - Skin), FCollisionQueryParams(SCENE_QUERY_STAT(BallGroundRoll), false, this));
	return Hits.ContainsByPredicate([this](const FHitResult& Hit)
	{
		const UPrimitiveComponent* Floor = Hit.GetComponent();
		return Floor && Hit.ImpactNormal.Z >= UE_INV_SQRT_2 && Floor->IsPhysicsCollisionEnabled()
			&& Floor->GetCollisionResponseToChannel(PhysicsRoot->GetCollisionObjectType()) == ECR_Block
			&& PhysicsRoot->GetCollisionResponseToChannel(Floor->GetCollisionObjectType()) == ECR_Block;
	});
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
	if (ConfigurationSource == EVehicleConfigurationSource::Definition && Definition)
	{
		const FBallStateConfig& State = Definition->Gameplay.State;
		if (State.GroundRollReleaseSpeed > 0.f && SpeedSquared <= FMath::Square(State.GroundRollReleaseSpeed) && HasGroundSupport())
		{
			const double Now = GetWorld()->GetTimeSeconds();
			if (GroundRollStartedAt < 0.) { GroundRollStartedAt = Now; }
			if (Now - GroundRollStartedAt >= State.GroundRollRequiredDuration)
			{
				FinishLaunchAsFree(Definition->Gameplay.Acquisition.GroundRollPickupLockDuration);
			}
			AccumulatedLowSpeedTime = 0.f;
			return;
		}
	}
	GroundRollStartedAt = -1.;
	if (SpeedSquared <= FMath::Square(LowSpeedThreshold))
	{
		AccumulatedLowSpeedTime += Interval;
		if (AccumulatedLowSpeedTime >= LowSpeedRequiredDuration)
		{
			FinishLaunchAsFree(PostLaunchPickupLockDuration);
		}
	}
	else
	{
		AccumulatedLowSpeedTime = 0.0f;
	}
}

void ABasicBallActor::FinishLaunchAsFree(float PickupLockSeconds)
{
	UWorld* World = GetWorld();
	StopLowSpeedMonitor();
	FBallRepState Next;
	Next.GlobalPickupLockEndServerTime = (World ? World->GetTimeSeconds() : 0.0f)
		+ FMath::Max(0.0f, PickupLockSeconds);
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

	TGuardValue<bool> Resolving(bResolvingDamageHit, true);
	ResolveLaunchedContact(OtherActor, OtherComponent, NormalImpulse, Hit);
}

void ABasicBallActor::ResolveLaunchedContact(AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	FVehicleHitContext Context;
	Context.SourceActor = this;
	Context.TargetActor = OtherActor;
	Context.InstigatorActor = RepState.LastLauncherPawn;
	Context.InstigatorPlayerId = RepState.LastLauncherPlayerId;
	uint32 CandidateEventSequence = LastResolvedHitEventSequence + 1;
	if (CandidateEventSequence == 0) { ++CandidateEventSequence; }
	Context.ServerHitEventSequence = CandidateEventSequence;
	Context.TargetPhysicsBody = OtherComponent;
	Context.SourcePhysicsBody = PhysicsRoot;
	Context.ImpactPoint = Hit.ImpactPoint;
	Context.ImpactNormal = Hit.ImpactNormal;
	Context.NormalImpulse = -NormalImpulse; // Hit callback delivers impulse on the ball; receiver needs impulse on target.
	Context.SourceVelocity = PhysicsRoot->GetPhysicsLinearVelocity();
	Context.SourceMass = PhysicsRoot->GetMass();
	if (IsValid(OtherComponent) && OtherComponent->IsSimulatingPhysics())
	{
		Context.TargetVelocityAtImpactPoint =
			OtherComponent->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint);
	}

	FHALCombatHitRequest Request;
	Request.AttackId = LaunchAction.GetSnapshot().AttackId;
	Request.PulseSequence = LaunchAction.GetPulseSequence();
	Request.Hit = Context;
	Request.bApplyImpact = true;
	if (LaunchAction.GetSnapshot().Impact.Version == EHALImpactVersion::TieredV1)
	{
		Request.Impact.Mode = EHALImpactMode::ExplicitTier;
		Request.Impact.Tier = LaunchAction.GetSnapshot().Impact.Tier;
		Request.Impact.PushPreset = LaunchAction.GetSnapshot().Impact.PushPreset;
	}
	Request.KnockbackStrengthMultiplier = KnockbackStrengthMultiplier;
	const FHALCombatHitResult Resolution = FCombatResolver::ResolveCombatHit(LaunchAction, Request);
	if (Resolution.bHitAccepted)
	{
		LastResolvedHitEventSequence = CandidateEventSequence;
		bool bShouldLogHit = bLogDamageHits;
#if !UE_BUILD_SHIPPING
		bShouldLogHit |= CVarHALBallNetLog.GetValueOnGameThread() > 0;
#endif
		if (bShouldLogHit)
		{
			UE_LOG(LogTemp, Log, TEXT("Ball %s hit vehicle %s, launcher %s, PlayerId=%d EventSeq=%u damage %.1f, impact %.1f, tier %s"),
				*GetName(),
				*GetNameSafe(OtherActor),
				*GetNameSafe(RepState.LastLauncherPawn.Get()),
				RepState.LastLauncherPlayerId,
				LastResolvedHitEventSequence,
				Resolution.ResolvedDamage,
				Resolution.ImpactSpeed,
				LexToString(Resolution.KnockbackTier));
		}
		OnAcceptedVehicleHit();
	}
}

void ABasicBallActor::OnAcceptedVehicleHit()
{
	FinishLaunchAsFree(PostLaunchPickupLockDuration);
}

void ABasicBallActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!HasActorBegunPlay() && !GetWorld()->IsGameWorld()) { ApplyDefinition(true); }
}

void ABasicBallActor::PreInitializeComponents()
{
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		// Blueprint editor and thumbnail previews spawn the actor into a world that never simulates.
		// OnConstruction already applies the preview configuration there, and the authoritative checks
		// below assume gameplay components a preview actor is not guaranteed to have.
		Super::PreInitializeComponents();
		return;
	}
	if (GetNetMode() != NM_Standalone && GetLocalRole() == ROLE_SimulatedProxy)
	{
		// Preserve PI even if an older Blueprint serialized the actor's former default mode.
		// A per-ball settings asset may override this in its component BeginPlay.
		SetPhysicsReplicationMode(EPhysicsReplicationMode::PredictiveInterpolation);
	}
	bConfigurationValid = ApplyDefinition(false);
	if (!GetMutableDefault<UVehicleKnockbackSettings>()->InitializeRules()) { bConfigurationValid = false; }
	FHALCollisionImpactRules ImpactRules;
	if (GetDefault<UVehicleKnockbackSettings>()->GetCollisionImpactRules(ImpactRules)
		&& ImpactRules.Version == EHALImpactVersion::TieredV1 && RuntimeImpactConfig.Version != ImpactRules.Version)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: TieredV1 rules require migrated BallDefinition.Gameplay.Impact. No default fallback."), *GetName());
		bConfigurationValid = false;
	}
	if (!bConfigurationValid)
	{
		if (!PhysicsRoot)
		{
			UE_LOG(LogTemp, Error, TEXT("%s: invalid configuration and no physics root to disable."), *GetName());
		}
		else
		{
			PhysicsRoot->SetSimulatePhysics(false);
			PhysicsRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	Super::PreInitializeComponents();
}

#if WITH_EDITOR
bool ABasicBallActor::CanEditChange(const FProperty* Property) const
{
	if (ConfigurationSource == EVehicleConfigurationSource::Definition && Property
		&& Property->GetOwnerClass() == StaticClass() && Property->HasAnyPropertyFlags(CPF_Edit)
		&& Property->GetFName() != GET_MEMBER_NAME_CHECKED(ABasicBallActor, ConfigurationSource)
		&& Property->GetFName() != GET_MEMBER_NAME_CHECKED(ABasicBallActor, Definition)
		// Per-level identity is not a legacy tuning value owned by BallDefinition.
		&& Property->GetFName() != GET_MEMBER_NAME_CHECKED(ABasicBallActor, bPrimaryCameraTarget)) { return false; }
	return Super::CanEditChange(Property);
}
#endif
