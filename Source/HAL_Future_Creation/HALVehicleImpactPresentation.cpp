#include "HALVehiclePresentationComponent.h"
#include "HALVehiclePresentationConfig.h"
#include "HALCombatTypes.h"
#include "VehicleRecoveryComponent.h"
#include "ArcadeVehicleMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarImpactVisualLog(TEXT("hal.VehicleImpactVisualLog"), 0,
		TEXT("0=off, 1=authority accepted impact visual events/consumption; no physics or damage changes."));
	bool IsNewerImpactSequence(uint32 Value, uint32 Previous) { return Value && int32(Value - Previous) > 0; }
	float SmoothImpactUnit(float Value) { const float T = FMath::Clamp(Value, 0.f, 1.f); return T * T * (3 - 2 * T); }
}

bool HALVehiclePresentation::IsImpactReboundEligible(const FHALCombatHitResult& Result)
{
	return Result.bHitAccepted && Result.RejectReason == EHALHitRejectReason::None && Result.bImpactApplied
		&& Result.ImpactOutcome == EHALImpactOutcome::Applied && !Result.bSupportSupplementSuppressed
		&& !Result.ImpactSummary.bImmune && Result.ImpactSummary.EffectiveTier != EHALImpactTier::None
		&& HALImpact::IsValidTier(Result.ImpactSummary.EffectiveTier);
}

bool HALVehiclePresentation::ImpactLocalDirection(const FVector& WorldDirection, const FQuat& Rotation, float Minimum, FVector& OutLocal)
{
	OutLocal = FVector::ZeroVector;
	if (WorldDirection.ContainsNaN() || Rotation.ContainsNaN() || !Rotation.IsNormalized() || !FMath::IsFinite(Minimum) || Minimum <= 0 || Minimum > 1) { return false; }
	const FVector Direction = WorldDirection.GetSafeNormal();
	if (Direction.SizeSquared2D() < FMath::Square(Minimum)) { return false; }
	OutLocal = Rotation.UnrotateVector(Direction);
	return FVector2D(OutLocal.X, OutLocal.Y).SizeSquared() >= UE_KINDA_SMALL_NUMBER;
}

float HALVehiclePresentation::ImpactEnvelope(const FHALImpactReboundTier& Tier, float Age)
{
	if (!FMath::IsFinite(Age) || Age < 0 || Tier.RiseSeconds <= 0 || Tier.ReturnSeconds <= 0) { return 0; }
	return Age < Tier.RiseSeconds ? SmoothImpactUnit(Age / Tier.RiseSeconds) : 1 - SmoothImpactUnit((Age - Tier.RiseSeconds) / Tier.ReturnSeconds);
}

bool HALVehiclePresentation::AcceptImpact(const FHALImpactReboundConfig& Config, const FHALImpactVisualEvent& Event,
	float Age, bool bPoseAllowed, FHALImpactVisualState& State)
{
	const auto* Tier = Config.GetTier(Event.Tier);
	const FVector Direction(Event.LocalDirection);
	if (!Config.bEnabled || !Tier || !IsNewerImpactSequence(Event.Sequence, State.LastSeenSequence) || !FMath::IsFinite(Age) || Age < 0
		|| Direction.ContainsNaN() || !Direction.IsNormalized() || FVector2D(Direction.X, Direction.Y).SizeSquared() < UE_KINDA_SMALL_NUMBER) { return false; }
	// Retire identities even when their packet is late or the current pose cannot display them.
	State.LastSeenSequence = Event.Sequence;
	if (!bPoseAllowed || Age > Config.MaximumEventAgeSeconds || Age >= Tier->RiseSeconds + Tier->ReturnSeconds) { return false; }
	if (State.Pulses.Num() == 8) { State.Pulses.RemoveAt(0); }
	auto& Pulse = State.Pulses.AddDefaulted_GetRef();
	Pulse.Tier = Event.Tier; Pulse.Direction = FVector2D(Direction.X, Direction.Y); Pulse.Age = Age;
	++State.StartedCount;
	return true;
}

void HALVehiclePresentation::FadeImpact(const FHALImpactReboundConfig& Config, FHALImpactVisualState& State)
{
	for (auto& Pulse : State.Pulses)
	{
		if (Pulse.FadeAge < 0)
		{
			const auto* Tier = Config.GetTier(Pulse.Tier);
			Pulse.FadeStart = Tier ? ImpactEnvelope(*Tier, Pulse.Age) : 0;
			Pulse.FadeAge = 0;
		}
	}
}

FHALImpactVisualOutput HALVehiclePresentation::AdvanceImpact(const FHALImpactReboundConfig& Config, float Delta, FHALImpactVisualState& State)
{
	FHALImpactVisualOutput Out;
	if (!Config.bEnabled || !FMath::IsFinite(Delta) || Delta < 0) { State.Pulses.Reset(); return Out; }
	for (int32 Index = State.Pulses.Num() - 1; Index >= 0; --Index)
	{
		auto& Pulse = State.Pulses[Index];
		const auto* Tier = Config.GetTier(Pulse.Tier);
		if (!Tier) { State.Pulses.RemoveAt(Index); continue; }
		Pulse.Age += Delta;
		float Value = ImpactEnvelope(*Tier, Pulse.Age);
		if (Pulse.FadeAge >= 0)
		{ Pulse.FadeAge += Delta; Value = Pulse.FadeStart * (1 - SmoothImpactUnit(Pulse.FadeAge / Config.PoseFadeSeconds)); }
		if (Pulse.FadeAge >= 0 ? Pulse.FadeAge >= Config.PoseFadeSeconds : Pulse.Age >= Tier->RiseSeconds + Tier->ReturnSeconds)
		{ State.Pulses.RemoveAt(Index); continue; }
		Out.Degrees.X += Value * Pulse.Direction.X * Tier->PitchDegrees * Config.PitchSign;
		Out.Degrees.Y += Value * Pulse.Direction.Y * Tier->RollDegrees * Config.RollSign;
		// Suppress opposing driving/shot lean per affected axis at the impact peak, then restore it.
		if (Tier->PitchDegrees > 0 && FMath::Abs(Pulse.Direction.X) > UE_KINDA_SMALL_NUMBER) { Out.Weights.X = FMath::Max(Out.Weights.X, double(Value)); }
		if (Tier->RollDegrees > 0 && FMath::Abs(Pulse.Direction.Y) > UE_KINDA_SMALL_NUMBER) { Out.Weights.Y = FMath::Max(Out.Weights.Y, double(Value)); }
	}
	return Out;
}

FRotator HALVehiclePresentation::ComposeImpactRotation(const FHALChassisPresentationConfig& Chassis,
	const FHALChassisPresentationState& Driving, const FHALLaunchReboundConfig& LaunchConfig, float LaunchPitch,
	const FHALImpactVisualOutput& Impact, float& OutPitch, float& OutRoll)
{
	float BasePitch = 0;
	ComposeChassisRotation(Chassis, Driving, LaunchConfig, LaunchPitch, BasePitch);
	FHALChassisPresentationState Combined;
	OutPitch = Combined.PitchDegrees = FMath::Clamp(float(BasePitch * (1 - FMath::Clamp(Impact.Weights.X, 0., 1.)) + Impact.Degrees.X), -Chassis.TotalPitchLimitDegrees, Chassis.TotalPitchLimitDegrees);
	OutRoll = Combined.RollDegrees = FMath::Clamp(float(Driving.RollDegrees * (1 - FMath::Clamp(Impact.Weights.Y, 0., 1.)) + Impact.Degrees.Y), -Chassis.TotalRollLimitDegrees, Chassis.TotalRollLimitDegrees);
	return ChassisRotation(Chassis, Combined);
}

bool UHALVehiclePresentationComponent::IsImpactPoseAllowed() const
{
	if (!Config || !Body || Body->GetUpVector().Z < Config->ImpactRebound.MinimumVehicleUpZ) { return false; }
	if (const auto* Recovery = GetOwner()->FindComponentByClass<UVehicleRecoveryComponent>(); Recovery && Recovery->IsExecuting()) { return false; }
	const auto* Movement = GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>();
	return !Movement || !Movement->GetStabilityObservation().Recovery.bActive;
}

void UHALVehiclePresentationComponent::PublishAcceptedImpact(const FHALCombatHitResult& Result, const FVector& WorldDirection)
{
	check(IsInGameThread());
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority() || !bConfigured || !Config || !Config->ImpactRebound.bEnabled
		|| !HALVehiclePresentation::IsImpactReboundEligible(Result)) { return; }
	FVector Local;
	if (!HALVehiclePresentation::ImpactLocalDirection(WorldDirection, GetOwner()->GetActorQuat(), Config->ImpactRebound.MinimumHorizontalDirection, Local)) { return; }
	FHALImpactVisualEvent Event;
	Event.Sequence = ++NextImpactVisualSequence;
	if (!Event.Sequence) { Event.Sequence = ++NextImpactVisualSequence; }
	Event.ServerTimeSeconds = float(GetLaunchServerTime()); Event.Tier = Result.ImpactSummary.EffectiveTier; Event.LocalDirection = Local;
	if (CVarImpactVisualLog.GetValueOnGameThread())
	{ UE_LOG(LogTemp, Display, TEXT("HAL ImpactVisual: %s Publish Seq=%u Tier=%d Local=%s Attack=%s Pulse=%u"), *GetNameSafe(GetOwner()), Event.Sequence, int32(Event.Tier), *Local.ToCompactString(), *Result.AttackId.ToString(), Result.PulseSequence); }
	MulticastImpactRebound(Event);
}

void UHALVehiclePresentationComponent::MulticastImpactRebound_Implementation(FHALImpactVisualEvent Event)
{ ConsumeImpactRebound(Event); }

void UHALVehiclePresentationComponent::ConsumeImpactRebound(const FHALImpactVisualEvent& Event)
{
	check(IsInGameThread());
	if (!bConfigured || !Config || !GetWorld() || !FMath::IsFinite(Event.ServerTimeSeconds)) { return; }
	const double RawAge = GetLaunchServerTime() - Event.ServerTimeSeconds;
	if (!FMath::IsFinite(RawAge) || RawAge < -.25) { return; }
	const float Age = float(FMath::Max(0., RawAge));
	if (GetWorld()->GetTimeSeconds() - Age + UE_KINDA_SMALL_NUMBER < LaunchResetLocalTime) { return; }
	const bool bStarted = HALVehiclePresentation::AcceptImpact(Config->ImpactRebound, Event, Age, IsImpactPoseAllowed(), ImpactState);
	if (CVarImpactVisualLog.GetValueOnGameThread())
	{ UE_LOG(LogTemp, Display, TEXT("HAL ImpactVisual: %s Consume Seq=%u Tier=%d Age=%.3f Started=%d Starts=%u"), *GetNameSafe(GetOwner()), Event.Sequence, int32(Event.Tier), Age, bStarted ? 1 : 0, ImpactState.StartedCount); }
}

FHALImpactVisualOutput UHALVehiclePresentationComponent::UpdateImpactRebound(float DeltaTime)
{
	if (!IsImpactPoseAllowed()) { HALVehiclePresentation::FadeImpact(Config->ImpactRebound, ImpactState); }
	return HALVehiclePresentation::AdvanceImpact(Config->ImpactRebound, DeltaTime, ImpactState);
}
