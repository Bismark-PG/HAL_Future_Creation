#include "HALCombatTypes.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

bool FVehicleCombatConfig::Validate(FString& Error) const
{
	if ((Version != EHALCombatConfigVersion::LegacyFixedDamage && Version != EHALCombatConfigVersion::AttackSnapshotV1)
		|| !FMath::IsFinite(AttackPower) || AttackPower < 0.0f
		|| !FMath::IsFinite(OutputMultiplierDelta)
		|| !FMath::IsFinite(DamageTakenMultiplier) || DamageTakenMultiplier < 0.0f)
	{
		Error = TEXT("Combat needs a known migration version, finite nonnegative attack/damage-taken values and a finite output delta.");
		return false;
	}
	return true;
}

bool FHALAttackSourceSnapshot::CalculateRawDamage(float& OutDamage) const
{
	OutDamage = 0.0f;
	double Damage = 0.0;
	if (DamageMode == EHALDamageMode::SourceFixedDamage)
	{
		if (!FMath::IsFinite(FixedDamage) || FixedDamage < 0.0f) { return false; }
		Damage = FixedDamage;
	}
	else if (DamageMode == EHALDamageMode::VehicleAttackScaled)
	{
		if (!FMath::IsFinite(CapturedAttackPower) || CapturedAttackPower < 0.0f
			|| !FMath::IsFinite(ActionMultiplier) || !FMath::IsFinite(MultiplierDeltaSum)) { return false; }
		Damage = double(CapturedAttackPower) * FMath::Max(0.0, double(ActionMultiplier) + MultiplierDeltaSum);
	}
	else { return false; }
	if (!FMath::IsFinite(Damage) || Damage > MAX_flt) { return false; }
	OutDamage = float(Damage);
	return true;
}

bool FHALCombatAction::Begin(AActor& AuthorityOwner, const FHALAttackSourceSnapshot& Source)
{
	float Damage;
	UWorld* OwnerWorld = AuthorityOwner.GetWorld();
	if (bActive || !IsValid(&AuthorityOwner) || !AuthorityOwner.HasAuthority() || !OwnerWorld || OwnerWorld->GetNetMode() == NM_Client
		|| !Source.CalculateRawDamage(Damage)) { return false; }
	Snapshot = Source;
	Snapshot.AttackId = FGuid::NewGuid();
	World = OwnerWorld;
	PulseSequence = 1;
	AcceptedTargets.Reset();
	bActive = true;
	return true;
}

bool FHALCombatAction::AdvancePulse(uint32 NewSequence)
{
	if (!bActive || NewSequence <= PulseSequence) { return false; }
	PulseSequence = NewSequence;
	AcceptedTargets.Reset();
	return true;
}

void FHALCombatAction::Close()
{
	bActive = false;
	AcceptedTargets.Reset();
	World.Reset();
}

EHALHitRejectReason FHALCombatAction::CheckRequest(const FHALCombatHitRequest& Request) const
{
	if (!bActive) { return EHALHitRejectReason::ClosedAction; }
	if (!World.IsValid() || World->GetNetMode() == NM_Client) { return EHALHitRejectReason::NonAuthority; }
	if (Request.AttackId != Snapshot.AttackId || Request.PulseSequence != PulseSequence)
	{
		return EHALHitRejectReason::InvalidAction;
	}
	if (!IsValid(Request.Hit.TargetActor) || Request.Hit.TargetActor->GetWorld() != World.Get())
	{
		return EHALHitRejectReason::InvalidTarget;
	}
	if (!Request.Hit.TargetActor->HasAuthority()) { return EHALHitRejectReason::NonAuthority; }
	if (AcceptedTargets.Contains(Request.Hit.TargetActor)) { return EHALHitRejectReason::Duplicate; }
	return AcceptedTargets.Num() >= MaximumTargetsPerPulse
		? EHALHitRejectReason::CapacityExceeded : EHALHitRejectReason::None;
}

void FHALCombatAction::ConsumeTarget(AActor* Target)
{
	AcceptedTargets.Add(Target);
}
