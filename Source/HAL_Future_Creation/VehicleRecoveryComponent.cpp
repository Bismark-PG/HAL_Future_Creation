#include "VehicleRecoveryComponent.h"
#include "HALAbilitySystemComponent.h"
#include "ArcadeVehicleMovementComponent.h"
#include "VehicleStabilityRules.h"
#include "BallControlComponent.h"
#include "HALCombatTypes.h"
#include "HALImpactReceiverComponent.h"
#include "HALImpactSourceComponent.h"
#include "HALMatchGameState.h"
#include "VehicleHealthComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

static TAutoConsoleVariable<int32> CVarHALRecoveryLog(TEXT("hal.VehicleRecoveryLog"),0,
	TEXT("Vehicle recovery authority phase transitions: 0=off, 1=on."));

UVehicleRecoveryComponent::UVehicleRecoveryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}
void UVehicleRecoveryComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner()->HasAuthority()) { return; }
	auto* Shared = GetWorld()->GetSubsystem<UVehicleStabilitySubsystem>();
	if (!Shared || !Shared->InitializeRules()) { return; }
	Rules = Shared->GetRecoveryRules();
	if (!Rules.bEnabled) { return; }
	LastPhysicsTime = LastUpdateTime = GetWorld()->GetTimeSeconds();
	GetWorld()->GetTimerManager().SetTimer(Timer,this,&ThisClass::UpdateRecovery,.05f,true);
}
void UVehicleRecoveryComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorld()->GetTimerManager().ClearTimer(Timer);
	Super::EndPlay(Reason);
}
void UVehicleRecoveryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UVehicleRecoveryComponent,Status);
}

void UVehicleRecoveryComponent::LogTransition(EVehicleRecoveryPhase Before, FName PreviousBlockReason) const
{
	if ((Before != Status.Phase || PreviousBlockReason != Status.BlockReason) && CVarHALRecoveryLog.GetValueOnGameThread() > 0)
	{
		const auto* Movement = GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>();
		const auto Sample = Movement ? Movement->GetStabilityObservation() : FVehicleStabilityObservation();
		const double Tilt = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Sample.Up,Sample.GroundNormal),-1.,1.)));
		UE_LOG(LogTemp,Log,TEXT("Vehicle recovery: Pawn=%s Phase=%d->%d Sequence=%lld Hold=%.3f Cooldown=%.3f Outcome=%s Block=%s Ground=%d Tilt=%.2f Speed=%.2f AngularDeg=%.2f Held=%d Frame=%d"),
			*GetNameSafe(GetOwner()),int32(Before),int32(Status.Phase),Status.Sequence,Status.HoldProgress,
			Status.CooldownRemaining,*Status.LastOutcome.ToString(),*Status.BlockReason.ToString(),Sample.bNearGround,Tilt,
			Sample.Speed,FMath::RadiansToDegrees(Sample.AngularSpeed),Sample.bRecoveryHeld,ObservedPhysicsFrame);
	}
}

bool UVehicleRecoveryComponent::Start(const FVehicleStabilityObservation& Sample)
{
	if (UHALAbilitySystemComponent::IsActorStunned(GetOwner())) { return false; }
	auto* Movement = GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>();
	FVehicleRecoveryPhysicsState Command;
	Command.Sequence = ++NextSequence;
	if (!Command.Sequence) { Command.Sequence = ++NextSequence; }
	Command.bActive = true; Command.TargetUp = Sample.GroundNormal;
	Command.FallbackAxis = FVector::VectorPlaneProject(Sample.Forward,Command.TargetUp).GetSafeNormal();
	if (Command.FallbackAxis.IsNearlyZero())
	{ Command.FallbackAxis = FVector::VectorPlaneProject(FVector::RightVector,Command.TargetUp).GetSafeNormal(); }
	if (!Movement || !Movement->QueueRecovery(Command)) { return false; }
	// This is the accepted authority execution boundary, not the held-input boundary.
	Status.Sequence = Command.Sequence; Status.Phase = EVehicleRecoveryPhase::Executing;
	Status.LastOutcome = TEXT("Started"); Status.HoldProgress = 0; Clock.Hold = 0;
	Status.BlockReason = NAME_None;
	if (auto* Balls = GetOwner()->FindComponentByClass<UBallControlComponent>()) { Balls->ReleaseForRecovery(); }
	GetOwner()->ForceNetUpdate();
	return true;
}

void UVehicleRecoveryComponent::Finish(FName Outcome)
{
	const auto Before = Status.Phase;
	if (IsExecuting())
	{
		FVehicleRecoveryPhysicsState Stop;
		Stop.Sequence = ++NextSequence;
		if (!Stop.Sequence) { Stop.Sequence = ++NextSequence; }
		if (auto* Movement = GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>()) { Movement->QueueRecovery(Stop); }
		Status.Sequence = Stop.Sequence;
		Clock.Finish(Rules); Status.CooldownRemaining = Clock.Cooldown;
		Status.Phase = EVehicleRecoveryPhase::Cooldown;
	}
	else { Clock.Flipped = Clock.Hold = 0; Status.Phase = EVehicleRecoveryPhase::Idle; }
	Status.HoldProgress = 0; Status.LastOutcome = Outcome;
	Status.BlockReason = Clock.Cooldown > 0 ? FName(TEXT("Cooldown")) : NAME_None;
	GetOwner()->ForceNetUpdate(); LogTransition(Before);
}

void UVehicleRecoveryComponent::CancelForOwnerExit()
{
	if (!GetOwner()->HasAuthority()) { return; }
	bOwnerExiting = true; Finish(TEXT("OwnerExit"));
	GetWorld()->GetTimerManager().ClearTimer(Timer);
}

void UVehicleRecoveryComponent::ResumeForOwner()
{
	if (!HasBegunPlay() || !GetOwner()->HasAuthority() || !Rules.bEnabled) { return; }
	bOwnerExiting = false;
	LastUpdateTime = LastPhysicsTime = GetWorld()->GetTimeSeconds();
	Clock.Flipped = Clock.Hold = 0;
	GetWorld()->GetTimerManager().SetTimer(Timer,this,&ThisClass::UpdateRecovery,.05f,true);
}

void UVehicleRecoveryComponent::UpdateRecovery()
{
	const auto Before = Status.Phase;
	const FName PreviousBlockReason = Status.BlockReason;
	const double Now = GetWorld()->GetTimeSeconds();
	const float Delta = FMath::Max(0.f,float(Now-LastUpdateTime)); LastUpdateTime = Now;
	auto* Movement = GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>();
	const auto Sample = Movement ? Movement->GetStabilityObservation() : FVehicleStabilityObservation();
	if (Sample.bValid && Sample.PhysicsFrame != ObservedPhysicsFrame)
	{ ObservedPhysicsFrame = Sample.PhysicsFrame; LastPhysicsTime = Now; }
	const auto* Pawn = Cast<APawn>(GetOwner());
	const auto* Match = GetWorld()->GetGameState<AHALMatchGameState>();
	const auto* Health = GetOwner()->FindComponentByClass<UVehicleHealthComponent>();
	const bool bPermitted = !bOwnerExiting && Pawn && Pawn->GetController() && Pawn->GetController()->GetPawn() == Pawn
		&& !UHALAbilitySystemComponent::IsActorStunned(GetOwner())
		&& (!Match || Match->GetMatchPhase() == EHALMatchPhase::Playing)
		&& Health && Health->IsHealthReady() && Health->GetCurrentHP() > 0 && Sample.bValid && Now-LastPhysicsTime <= .25;
	Status.bDrivingAllowed = Sample.bDrivingAllowed; Status.bNearGround = Sample.bNearGround;
	if (!bPermitted)
	{
		if (IsExecuting()) { Finish(TEXT("InvalidOwnerOrPhysics")); }
		else { Clock.Advance(Rules,false,false,Delta); Status.HoldProgress = 0;
			Status.CooldownRemaining = Clock.Cooldown; Status.Phase = Clock.Cooldown > 0 ? EVehicleRecoveryPhase::Cooldown : EVehicleRecoveryPhase::Idle;
			Status.BlockReason = UHALAbilitySystemComponent::IsActorStunned(GetOwner()) ? FName(TEXT("Stunned")) : FName(TEXT("OwnerOrPhysics"));
			LogTransition(Before,PreviousBlockReason); }
		return;
	}
	if (IsExecuting())
	{
		// Wait for the queued action to be consumed before evaluating its physical outcome.
		if (Sample.Recovery.Sequence != Status.Sequence) { return; }
		// Rolling the convex proxy can briefly lift it out of the short support probe.
		// Start still requires stable ground; execution gets only its existing bounded two seconds.
		if (Rules.GroundIsValid(Sample.bNearGround,Sample.GroundNormal)
			&& FVector::DotProduct(Sample.Up,Sample.GroundNormal) >= FMath::Cos(FMath::DegreesToRadians(Rules.SuccessTilt))
			&& Sample.AngularSpeed <= FMath::DegreesToRadians(Rules.MaximumAngularSpeed)) { Finish(TEXT("Succeeded")); return; }
		if (!Sample.Recovery.bActive) { Finish(TEXT("Timeout")); }
		return;
	}
	Status.BlockReason = Rules.EligibilityBlockReason(Sample.bNearGround,Sample.Up,Sample.GroundNormal,Sample.Speed,Sample.AngularSpeed);
	const bool bEligible = Status.BlockReason.IsNone();
	// A long GT stall cannot award unattended hold progress from one old observation.
	const bool bStart = Clock.Advance(Rules,bEligible && Delta <= .1f,Sample.bRecoveryHeld && Delta <= .1f,Delta);
	Status.CooldownRemaining = Clock.Cooldown;
	Status.HoldProgress = Clock.Hold/Rules.HoldSeconds;
	Status.Phase = Clock.Cooldown > 0 ? EVehicleRecoveryPhase::Cooldown
		: Clock.Hold > 0 ? EVehicleRecoveryPhase::Holding
		: bEligible && Clock.Flipped+KINDA_SMALL_NUMBER >= Rules.FlippedSeconds ? EVehicleRecoveryPhase::Eligible : EVehicleRecoveryPhase::Idle;
	if (Clock.Cooldown > 0) { Status.BlockReason = TEXT("Cooldown"); }
	else if (bEligible && Delta > .1f) { Status.BlockReason = TEXT("FrameGap"); }
	else if (bEligible && Clock.Flipped+KINDA_SMALL_NUMBER < Rules.FlippedSeconds) { Status.BlockReason = TEXT("Stabilizing"); }
	else if (bEligible && !Sample.bRecoveryHeld) { Status.BlockReason = TEXT("NotHeld"); }
	if (bStart && !Start(Sample)) { Clock.Hold = 0; Status.HoldProgress = 0; Status.LastOutcome = TEXT("ActionRejected"); }
	LogTransition(Before,PreviousBlockReason);
}

void UVehicleRecoveryComponent::NotifyExternalImpact(const FHALCombatHitRequest& Request, EHALImpactTier EffectiveTier)
{
	if (!GetOwner()->HasAuthority() || !IsExecuting() || uint8(EffectiveTier) < uint8(Rules.MinimumInterruptTier)
		|| !IsValid(Request.Hit.SourceActor) || Request.Hit.SourceActor == GetOwner()) { return; }
	if (Request.Impact.Mode == EHALImpactMode::PhysicalContact)
	{
		const FName Source = HALVehicleStability::ClassifySupportSource(Request.Hit.SourcePhysicsBody,Request.Hit.ImpactPoint);
		if (Source != TEXT("SpecializedSource") && Source != TEXT("MovingSource") && Source != TEXT("SimulatedSource")) { return; }
		// Self-righting can strike an idle car/ball. Require incoming source motion for raw contacts;
		// explicit attacks carry their own external action semantics and need no velocity inference.
		const auto* Body = Request.Hit.SourcePhysicsBody;
		const FVector SourceMotion = IsValid(Body) ? Body->GetPhysicsLinearVelocityAtPoint(Request.Hit.ImpactPoint) : FVector::ZeroVector;
		if (SourceMotion.ContainsNaN() || FVector::DotProduct(SourceMotion,Request.Hit.NormalImpulse.GetSafeNormal()) <= KINDA_SMALL_NUMBER) { return; }
	}
	Finish(TEXT("Interrupted"));
}
