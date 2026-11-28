#include "VehicleRecoveryComponent.h"
#include "HALRecoveryPromptWidget.h"
#include "HALHookComponent.h"
#include "HALGameFlowPermissions.h"
#include "HALAbilitySystemComponent.h"
#include "ArcadeVehicleMovementComponent.h"
#include "VehicleStabilityRules.h"
#include "BallControlComponent.h"
#include "HALCombatTypes.h"
#include "HALImpactReceiverComponent.h"
#include "HALImpactSourceComponent.h"
#include "HALMatchGameState.h"
#include "HALVehicleTestGameMode.h"
#include "VehicleHealthComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
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
	auto* Shared = GetWorld()->GetSubsystem<UVehicleStabilitySubsystem>();
	if (!Shared || !Shared->InitializeRules()) { return; }
	Rules = Shared->GetRecoveryRules();
	bRulesReady = Rules.bEnabled;
	if (!bRulesReady || !GetOwner()->HasAuthority()) { return; }
	LastPhysicsTime = LastUpdateTime = GetWorld()->GetTimeSeconds();
	GetWorld()->GetTimerManager().SetTimer(Timer,this,&ThisClass::UpdateRecovery,.05f,true);
}
void UVehicleRecoveryComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Prompt) { Prompt->RemoveFromParent(); Prompt = nullptr; }
	// GC can finish a component after its world has already been torn down.
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(Timer); }
	else { Timer.Invalidate(); }
	Super::EndPlay(Reason);
}
void UVehicleRecoveryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UVehicleRecoveryComponent,Status);
}

float UVehicleRecoveryComponent::GetDisplayProgress() const
{
	if (IsExecuting()) { return 1; }
	return FMath::Min(1.f, Status.HoldProgress + float(PendingDisplayPresses.Num()) / Rules.RequiredPresses);
}

void UVehicleRecoveryComponent::PressRecovery()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!bRulesReady || !Pawn || !Pawn->IsLocallyControlled() || !HALGameFlow::Permissions(GetWorld(),Pawn).bRecovery) { return; }
	if (IsExecuting() || Status.Phase == EVehicleRecoveryPhase::Cooldown) { return; }
	// The owner may already see a valid overturned pose before its first status packet arrives.
	const auto* Movement = Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>();
	const auto Sample = Movement ? Movement->GetStabilityObservation() : FVehicleStabilityObservation();
	const bool bPredict = Status.Phase == EVehicleRecoveryPhase::Eligible || Status.Phase == EVehicleRecoveryPhase::Holding ||
		(Sample.bValid && Rules.IsEligible(Sample.bNearGround,Sample.Up,Sample.GroundNormal,Sample.Speed,Sample.AngularSpeed));
	if (int32(Status.AcknowledgedPress - LocalPressId) > 0) { LocalPressId = Status.AcknowledgedPress; }
	++LocalPressId;
	if (bPredict)
	{
		LastLocalPressTime = GetWorld()->GetTimeSeconds();
		if (!Pawn->HasAuthority())
		{
			if (PendingDisplayPresses.Num() == Rules.RequiredPresses) { PendingDisplayPresses.RemoveAt(0); }
			PendingDisplayPresses.Add(LocalPressId);
			RefreshLocalPrompt(true);
		}
	}
	ServerPressRecovery(LocalPressId);
}

void UVehicleRecoveryComponent::ServerPressRecovery_Implementation(uint32 PressId)
{
	if (!bRulesReady || bOwnerExiting || int32(PressId - Status.AcknowledgedPress) <= 0) { return; }
	// The serial only reconciles the owner's visual prediction. A request is worth at most one press.
	Status.AcknowledgedPress = PressId;
	EvaluateRecovery(true);
	GetOwner()->ForceNetUpdate();
	RefreshLocalPrompt();
}

void UVehicleRecoveryComponent::OnRep_Status()
{
	PendingDisplayPresses.RemoveAll([this](uint32 Id) { return int32(Id - Status.AcknowledgedPress) <= 0; });
	if (IsExecuting() || Status.Phase == EVehicleRecoveryPhase::Cooldown) { PendingDisplayPresses.Reset(); }
	RefreshLocalPrompt();
}

void UVehicleRecoveryComponent::RefreshLocalPrompt(bool bAnticipate)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const bool bShow = PC && PC->IsLocalController() && PC->GetLocalPlayer() && PC->GetPawn() == Pawn &&
		!bOwnerExiting && !GetWorld()->bIsTearingDown && (bAnticipate || IsExecuting() ||
		Status.Phase == EVehicleRecoveryPhase::Eligible || Status.Phase == EVehicleRecoveryPhase::Holding);
	if (bShow && !Prompt)
	{
		Prompt = CreateWidget<UHALRecoveryPromptWidget>(PC,UHALRecoveryPromptWidget::StaticClass());
		if (Prompt)
		{
			Prompt->SetVisibility(ESlateVisibility::HitTestInvisible);
			Prompt->ForceVolatile(true);
			Prompt->AddToPlayerScreen(18);
			Prompt->SetAnchorsInViewport(FAnchors(0,0,1,1));
		}
	}
	else if (!bShow && Prompt) { Prompt->RemoveFromParent(); Prompt = nullptr; }
	if (bShow && IsExecuting() && !bShowingExecution) { ExecutionCueTime = GetWorld()->GetTimeSeconds(); }
	bShowingExecution = bShow && IsExecuting();
}

void UVehicleRecoveryComponent::LogTransition(EVehicleRecoveryPhase Before, FName PreviousBlockReason) const
{
	if ((Before != Status.Phase || PreviousBlockReason != Status.BlockReason) && CVarHALRecoveryLog.GetValueOnGameThread() > 0)
	{
		const auto* Movement = GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>();
		const auto Sample = Movement ? Movement->GetStabilityObservation() : FVehicleStabilityObservation();
		const double Tilt = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Sample.Up,Sample.GroundNormal),-1.,1.)));
		UE_LOG(LogTemp,Log,TEXT("Vehicle recovery: Pawn=%s Phase=%d->%d Sequence=%lld TapProgress=%.3f Cooldown=%.3f Outcome=%s Block=%s Ground=%d Tilt=%.2f Speed=%.2f AngularDeg=%.2f Held=%d Frame=%d"),
			*GetNameSafe(GetOwner()),int32(Before),int32(Status.Phase),Status.Sequence,Status.HoldProgress,
			Status.CooldownRemaining,*Status.LastOutcome.ToString(),*Status.BlockReason.ToString(),Sample.bNearGround,Tilt,
			Sample.Speed,FMath::RadiansToDegrees(Sample.AngularSpeed),Sample.bRecoveryHeld,ObservedPhysicsFrame);
	}
}

bool UVehicleRecoveryComponent::Start(const FVehicleStabilityObservation& Sample)
{
	if (UHALAbilitySystemComponent::IsActorStunned(GetOwner()) || UHALHookComponent::BlocksActionsOn(GetOwner())) { return false; }
	auto* Movement = GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>();
	FVehicleRecoveryPhysicsState Command;
	Command.Sequence = ++NextSequence;
	if (!Command.Sequence) { Command.Sequence = ++NextSequence; }
	Command.bActive = true; Command.TargetUp = Sample.GroundNormal;
	Command.FallbackAxis = FVector::VectorPlaneProject(Sample.Forward,Command.TargetUp).GetSafeNormal();
	if (Command.FallbackAxis.IsNearlyZero())
	{ Command.FallbackAxis = FVector::VectorPlaneProject(FVector::RightVector,Command.TargetUp).GetSafeNormal(); }
	if (!Movement || !Movement->QueueRecovery(Command)) { return false; }
	// Ball release belongs to the accepted physics action.
	Status.Sequence = Command.Sequence; Status.Phase = EVehicleRecoveryPhase::Executing;
	Status.LastOutcome = TEXT("Started"); Status.HoldProgress = 1; Clock.ResetAttempt();
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
	else { Clock.ResetAttempt(); Status.Phase = EVehicleRecoveryPhase::Idle; }
	Status.HoldProgress = 0; Status.LastOutcome = Outcome;
	Status.BlockReason = Clock.Cooldown > 0 ? FName(TEXT("Cooldown")) : NAME_None;
	GetOwner()->ForceNetUpdate(); LogTransition(Before);
	RefreshLocalPrompt();
}

void UVehicleRecoveryComponent::CancelForOwnerExit()
{
	PendingDisplayPresses.Reset();
	if (Prompt) { Prompt->RemoveFromParent(); Prompt = nullptr; }
	if (!GetOwner()->HasAuthority()) { return; }
	bOwnerExiting = true; Finish(TEXT("OwnerExit"));
	GetWorld()->GetTimerManager().ClearTimer(Timer);
}

void UVehicleRecoveryComponent::ResumeForOwner()
{
	if (!HasBegunPlay() || !GetOwner()->HasAuthority() || !Rules.bEnabled) { return; }
	bOwnerExiting = false;
	LastUpdateTime = LastPhysicsTime = GetWorld()->GetTimeSeconds();
	Clock.ResetAttempt();
	Status.AcknowledgedPress = LocalPressId = 0;
	GetWorld()->GetTimerManager().SetTimer(Timer,this,&ThisClass::UpdateRecovery,.05f,true);
}

void UVehicleRecoveryComponent::UpdateRecovery()
{
	EvaluateRecovery(false);
	RefreshLocalPrompt();
}

void UVehicleRecoveryComponent::EvaluateRecovery(bool bPressed)
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
	const auto* Health = GetOwner()->FindComponentByClass<UVehicleHealthComponent>();
	const bool bHealthReady = Health && Health->IsHealthReady();
	const bool bDefeated = bHealthReady && Health->GetCurrentHP() <= 0.f && !AHALVehicleTestGameMode::AllowsRecoveryAtZeroHP(Pawn);
	const bool bStunned = UHALAbilitySystemComponent::IsActorStunned(GetOwner());
	const bool bPermitted = !bOwnerExiting && Pawn && Pawn->GetController() && Pawn->GetController()->GetPawn() == Pawn
		&& !bStunned && !UHALHookComponent::BlocksActionsOn(GetOwner())
		&& HALGameFlow::Permissions(GetWorld(),Pawn).bRecovery
		&& bHealthReady && !bDefeated && Sample.bValid && Now-LastPhysicsTime <= .25;
	Status.bDrivingAllowed = Sample.bDrivingAllowed; Status.bNearGround = Sample.bNearGround;
	if (!bPermitted)
	{
		if (IsExecuting()) { Finish(TEXT("InvalidOwnerOrPhysics")); }
		else { Clock.Advance(Rules,false,false,false,Delta); Status.HoldProgress = 0;
			Status.CooldownRemaining = Clock.Cooldown; Status.Phase = Clock.Cooldown > 0 ? EVehicleRecoveryPhase::Cooldown : EVehicleRecoveryPhase::Idle;
			Status.BlockReason = bDefeated ? FName(TEXT("ZeroHP")) : bStunned ? FName(TEXT("Stunned")) : FName(TEXT("OwnerOrPhysics"));
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
			&& Sample.AngularSpeed <= FMath::DegreesToRadians(Rules.SuccessAngularSpeed)) { Finish(TEXT("Succeeded")); return; }
		if (!Sample.Recovery.bActive) { Finish(TEXT("Timeout")); }
		return;
	}
	Status.BlockReason = Rules.EligibilityBlockReason(Sample.bNearGround,Sample.Up,Sample.GroundNormal,Sample.Speed,Sample.AngularSpeed);
	const bool bEligible = Status.BlockReason.IsNone();
	const bool bBriefSupportLoss = Status.BlockReason == TEXT("NoGround") && Sample.Speed <= Rules.MaximumLinearSpeed
		&& Sample.AngularSpeed <= FMath::DegreesToRadians(Rules.MaximumAngularSpeed);
	const bool bStart = Clock.Advance(Rules,bEligible,bBriefSupportLoss,bPressed,Delta);
	Status.CooldownRemaining = Clock.Cooldown;
	Status.HoldProgress = Clock.Presses / Rules.RequiredPresses;
	Status.Phase = Clock.Cooldown > 0 ? EVehicleRecoveryPhase::Cooldown
		: Clock.Presses > 0 ? EVehicleRecoveryPhase::Holding
		: Clock.bAttemptActive ? EVehicleRecoveryPhase::Eligible : EVehicleRecoveryPhase::Idle;
	if (Clock.Cooldown > 0) { Status.BlockReason = TEXT("Cooldown"); }
	else if (bEligible && Delta > .1f) { Status.BlockReason = TEXT("FrameGap"); }
	else if (bEligible && Clock.Flipped+KINDA_SMALL_NUMBER < Rules.FlippedSeconds) { Status.BlockReason = TEXT("Stabilizing"); }
	if (bStart && !Start(Sample)) { Clock.ResetAttempt(); Status.HoldProgress = 0; Status.Phase = EVehicleRecoveryPhase::Idle; Status.LastOutcome = TEXT("ActionRejected"); }
	if (Status.Phase != Before) { GetOwner()->ForceNetUpdate(); }
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
