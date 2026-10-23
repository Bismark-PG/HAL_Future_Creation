#include "HALVehiclePresentationComponent.h"
#include "HALVehiclePresentationConfig.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarLaunchVisualLog(TEXT("hal.VehicleLaunchVisualLog"), 0,
		TEXT("0=off, 1=visual launch prediction/accept/reject identities; no physics forces."));
	bool Newer(uint32 Sequence, uint32 Previous) { return Sequence && int32(Sequence - Previous) > 0; }
	float SmoothUnit(float Value) { const float T = FMath::Clamp(Value, 0.f, 1.f); return T * T * (3 - 2 * T); }
	FHALLaunchVisualPulse* FindPulse(FHALLaunchVisualState& State, uint32 Sequence)
	{ return State.Pulses.FindByPredicate([Sequence](const auto& Pulse) { return Pulse.Sequence == Sequence; }); }
	void AddPulse(FHALLaunchVisualState& State, uint32 Sequence, float Age, bool bPredicted)
	{
		if (State.Pulses.Num() == 8) { State.Pulses.RemoveAt(0); }
		auto& Pulse = State.Pulses.AddDefaulted_GetRef();
		Pulse.Sequence = Sequence; Pulse.Age = Age; Pulse.bPredicted = bPredicted; Pulse.bConfirmed = !bPredicted;
		State.LastSeenSequence = Sequence; ++State.StartedCount;
	}
}

float HALVehiclePresentation::LaunchEnvelope(const FHALLaunchReboundConfig& Config, float Age)
{
	if (!FMath::IsFinite(Age) || Age < 0 || Config.RiseSeconds <= 0 || Config.ReturnSeconds <= 0) { return 0; }
	// Finite analytic envelope: zero velocity at start, peak and rest, independent of render rate.
	return Age < Config.RiseSeconds ? SmoothUnit(Age / Config.RiseSeconds)
		: 1 - SmoothUnit((Age - Config.RiseSeconds) / Config.ReturnSeconds);
}

bool HALVehiclePresentation::PredictLaunch(const FHALLaunchReboundConfig& Config, uint32 Sequence, FHALLaunchVisualState& State)
{
	if (!Config.bEnabled || !Newer(Sequence, State.LastSeenSequence)) { return false; }
	AddPulse(State, Sequence, 0, true); ++State.PredictedCount; return true;
}

bool HALVehiclePresentation::AcceptLaunch(const FHALLaunchReboundConfig& Config, uint32 Sequence, float Age, FHALLaunchVisualState& State)
{
	if (!Config.bEnabled || !Sequence || !FMath::IsFinite(Age)) { return false; }
	if (auto* Pulse = FindPulse(State, Sequence))
	{
		if (Pulse->bConfirmed || Pulse->RejectAge >= 0) { return false; }
		Pulse->bConfirmed = true; ++State.ConfirmedCount; return true; // Do not reset prediction's age.
	}
	if (!Newer(Sequence, State.LastSeenSequence) || Age < 0 || Age > Config.MaximumEventAgeSeconds
		|| Age >= Config.RiseSeconds + Config.ReturnSeconds) { return false; }
	AddPulse(State, Sequence, Age, false); ++State.ConfirmedCount; return true;
}

bool HALVehiclePresentation::RejectLaunch(const FHALLaunchReboundConfig& Config, uint32 Sequence, FHALLaunchVisualState& State)
{
	if (!Sequence) { return false; }
	if (auto* Pulse = FindPulse(State, Sequence); Pulse && Pulse->bPredicted && !Pulse->bConfirmed && Pulse->RejectAge < 0)
	{
		Pulse->RejectStart = LaunchEnvelope(Config, Pulse->Age); Pulse->RejectAge = 0;
		++State.RejectedCount; return true;
	}
	if (Newer(Sequence, State.LastSeenSequence)) { State.LastSeenSequence = Sequence; }
	return false;
}

float HALVehiclePresentation::AdvanceLaunch(const FHALLaunchReboundConfig& Config, float Delta, FHALLaunchVisualState& State)
{
	if (!Config.bEnabled || !FMath::IsFinite(Delta) || Delta < 0) { State.Pulses.Reset(); return 0; }
	float Sum = 0;
	for (int32 Index = State.Pulses.Num() - 1; Index >= 0; --Index)
	{
		auto& Pulse = State.Pulses[Index];
		Pulse.Age += Delta;
		float Value = LaunchEnvelope(Config, Pulse.Age);
		if (Pulse.RejectAge >= 0)
		{
			Pulse.RejectAge += Delta;
			Value = Pulse.RejectStart * (1 - SmoothUnit(Pulse.RejectAge / Config.RejectionFadeSeconds));
		}
		if (Pulse.RejectAge >= 0 ? Pulse.RejectAge >= Config.RejectionFadeSeconds : Pulse.Age >= Config.RiseSeconds + Config.ReturnSeconds)
		{ State.Pulses.RemoveAt(Index); }
		else { Sum += Value * Config.PitchDegrees; }
	}
	return Sum;
}

FRotator HALVehiclePresentation::ComposeChassisRotation(const FHALChassisPresentationConfig& Config,
	const FHALChassisPresentationState& Driving, const FHALLaunchReboundConfig& LaunchConfig, float LaunchPitch, float& OutPitch)
{
	FHALChassisPresentationState Combined = Driving;
	// Recoil-induced braking lean must not cancel a configured shot peak. Preserve its filter,
	// but blend its contribution out over the shot envelope and back in as the pulse returns.
	const float Weight = FMath::Abs(LaunchConfig.PitchDegrees) > UE_KINDA_SMALL_NUMBER
		? FMath::Clamp(FMath::Abs(LaunchPitch / LaunchConfig.PitchDegrees), 0.f, 1.f) : 0;
	OutPitch = Combined.PitchDegrees = FMath::Clamp(Driving.PitchDegrees * (1 - Weight) + LaunchPitch, -Config.TotalPitchLimitDegrees, Config.TotalPitchLimitDegrees);
	Combined.RollDegrees = FMath::Clamp(Combined.RollDegrees, -Config.TotalRollLimitDegrees, Config.TotalRollLimitDegrees);
	return ChassisRotation(Config, Combined);
}

double UHALVehiclePresentationComponent::GetLaunchServerTime() const
{
	if (!GetWorld()) { return 0; }
	const auto* State = GetWorld()->GetGameState();
	return State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void UHALVehiclePresentationComponent::PredictLaunchRebound(uint32 Sequence)
{
	check(IsInGameThread());
	const auto* Pawn = Cast<APawn>(GetOwner());
	if (!bConfigured || !Config || !Pawn || Pawn->HasAuthority() || !Pawn->IsLocallyControlled()) { return; }
	const bool bStarted = HALVehiclePresentation::PredictLaunch(Config->LaunchRebound, Sequence, LaunchState);
	if (CVarLaunchVisualLog.GetValueOnGameThread())
	{ UE_LOG(LogTemp, Display, TEXT("HAL LaunchVisual: %s Predict Seq=%u Started=%d"), *GetNameSafe(GetOwner()), Sequence, bStarted ? 1 : 0); }
}

void UHALVehiclePresentationComponent::PublishAcceptedLaunch(uint32 Sequence, float ServerTime)
{
	check(IsInGameThread());
	if (GetOwner() && GetOwner()->HasAuthority() && Sequence && FMath::IsFinite(ServerTime)) { MulticastAcceptedLaunch(Sequence, ServerTime); }
}

void UHALVehiclePresentationComponent::MulticastAcceptedLaunch_Implementation(uint32 Sequence, float ServerTime)
{ ConsumeAcceptedLaunch(Sequence, ServerTime); }

void UHALVehiclePresentationComponent::ConsumeAcceptedLaunch(uint32 Sequence, float ServerTime)
{
	check(IsInGameThread());
	if (!bConfigured || !Config || !GetWorld() || !FMath::IsFinite(ServerTime)) { return; }
	const double RawAge = GetLaunchServerTime() - ServerTime;
	// A small future timestamp can be server-clock interpolation error, never a scheduled gameplay action.
	if (!FMath::IsFinite(RawAge) || RawAge < -.25) { return; }
	const float Age = float(FMath::Max(0.0, RawAge));
	if (GetWorld()->GetTimeSeconds() - Age + UE_KINDA_SMALL_NUMBER < LaunchResetLocalTime) { return; }
	const bool bConsumed = HALVehiclePresentation::AcceptLaunch(Config->LaunchRebound, Sequence, Age, LaunchState);
	if (CVarLaunchVisualLog.GetValueOnGameThread())
	{ UE_LOG(LogTemp, Display, TEXT("HAL LaunchVisual: %s Accept Seq=%u Age=%.3f Consumed=%d Starts=%u Confirms=%u"),
		*GetNameSafe(GetOwner()), Sequence, Age, bConsumed ? 1 : 0, LaunchState.StartedCount, LaunchState.ConfirmedCount); }
}

void UHALVehiclePresentationComponent::ReceiveLaunchResult(uint32 Sequence, bool bAccepted, float ServerTime)
{
	check(IsInGameThread());
	if (bAccepted) { ConsumeAcceptedLaunch(Sequence, ServerTime); return; }
	if (!Config) { return; }
	const bool bCancelled = HALVehiclePresentation::RejectLaunch(Config->LaunchRebound, Sequence, LaunchState);
	if (CVarLaunchVisualLog.GetValueOnGameThread())
	{ UE_LOG(LogTemp, Display, TEXT("HAL LaunchVisual: %s Reject Seq=%u Cancelled=%d"), *GetNameSafe(GetOwner()), Sequence, bCancelled ? 1 : 0); }
}
