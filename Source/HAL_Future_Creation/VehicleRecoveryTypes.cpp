#include "VehicleRecoveryTypes.h"

bool FVehicleRecoveryConfig::Validate(FString& Error) const
{
	const auto In = [](float V, float A, float B) { return FMath::IsFinite(V) && V >= A && V <= B; };
	if (!In(MaximumGroundSlope,0,80) || !In(DriveEnterTilt,1,85) || !In(DriveExitTilt,1,89)
		|| DriveEnterTilt >= DriveExitTilt || !In(DriveStableSeconds,0,1) || !In(SupportDistance,0,100)
		|| !In(MinimumRecoveryTilt,1,180) || MinimumRecoveryTilt < DriveExitTilt || !In(FlippedSeconds,0,10)
		|| !In(MaximumLinearSpeed,1,1000) || !In(MaximumAngularSpeed,1,360) || !In(HoldSeconds,.1f,10)
		|| !In(MaximumExecutionSeconds,.1f,5) || !In(CooldownSeconds,0,30)
		|| (MinimumInterruptTier != EHALImpactTier::Medium && MinimumInterruptTier != EHALImpactTier::Heavy)
		|| !In(SuccessTilt,1,40) || SuccessTilt >= DriveEnterTilt || !In(AngularPositionGain,1,100)
		|| !In(AngularDampingGain,0,30) || !In(MaximumAngularAcceleration,1,100) || !In(MaximumRecoveryAngularSpeed,.1f,10))
	{ Error = TEXT("Invalid Recovery: finite ranges, SuccessTilt < DriveEnterTilt < DriveExitTilt <= MinimumRecoveryTilt, and Medium/Heavy interrupt required"); return false; }
	return true;
}

FString FVehicleRecoveryConfig::SignatureKey() const
{
	FString Key = FString::Printf(TEXT(":RecoveryV2:%d:%d"), bEnabled, int32(MinimumInterruptTier));
	for (float V : {MaximumGroundSlope, DriveEnterTilt, DriveExitTilt, DriveStableSeconds, SupportDistance,
		MinimumRecoveryTilt, FlippedSeconds, MaximumLinearSpeed, MaximumAngularSpeed, HoldSeconds, MaximumExecutionSeconds,
		CooldownSeconds, SuccessTilt, AngularPositionGain, AngularDampingGain, MaximumAngularAcceleration, MaximumRecoveryAngularSpeed})
	{ Key += FString::Printf(TEXT(":%.9g"), V); }
	return Key;
}

bool FVehicleRecoveryConfig::GroundIsValid(bool bNear, const FVector& Normal) const
{
	return bNear && !Normal.ContainsNaN() && Normal.IsNormalized()
		&& Normal.Z >= FMath::Cos(FMath::DegreesToRadians(MaximumGroundSlope));
}

bool FVehicleRecoveryConfig::CanDrive(bool bPreviouslyAllowed, const FVector& Up, const FVector& Normal) const
{
	return !Up.ContainsNaN() && Up.IsNormalized() && FVector::DotProduct(Up, Normal)
		>= FMath::Cos(FMath::DegreesToRadians(bPreviouslyAllowed ? DriveExitTilt : DriveEnterTilt));
}

bool FVehicleRecoveryConfig::IsEligible(bool bNear, const FVector& Up, const FVector& Normal, float Speed, float AngularRadians) const
{
	return EligibilityBlockReason(bNear,Up,Normal,Speed,AngularRadians).IsNone();
}

FName FVehicleRecoveryConfig::EligibilityBlockReason(bool bNear, const FVector& Up, const FVector& Normal, float Speed, float AngularRadians) const
{
	if (!bEnabled) { return TEXT("Disabled"); }
	if (!bNear) { return TEXT("NoGround"); }
	if (!GroundIsValid(bNear,Normal)) { return TEXT("SteepGround"); }
	if (Up.ContainsNaN() || !Up.IsNormalized()) { return TEXT("InvalidPose"); }
	if (FVector::DotProduct(Up,Normal) > FMath::Cos(FMath::DegreesToRadians(MinimumRecoveryTilt))) { return TEXT("Tilt"); }
	if (!FMath::IsFinite(Speed) || Speed < 0 || Speed > MaximumLinearSpeed) { return TEXT("LinearSpeed"); }
	if (!FMath::IsFinite(AngularRadians) || AngularRadians < 0 || AngularRadians > FMath::DegreesToRadians(MaximumAngularSpeed)) { return TEXT("AngularSpeed"); }
	return NAME_None;
}

bool FVehicleRecoveryPhysicsState::IsValid() const
{
	return (!bActive || Sequence != 0) && !TargetUp.ContainsNaN() && TargetUp.IsNormalized()
		&& !FallbackAxis.ContainsNaN() && FallbackAxis.IsNormalized()
		&& FMath::Abs(FVector::DotProduct(TargetUp, FallbackAxis)) < .01
		&& FMath::IsFinite(ElapsedSeconds) && ElapsedSeconds >= 0 && ElapsedSeconds <= 5.1f;
}
bool FVehicleRecoveryPhysicsState::Equals(const FVehicleRecoveryPhysicsState& Other) const
{
	return Sequence == Other.Sequence && bActive == Other.bActive && TargetUp.Equals(Other.TargetUp,.001)
		&& FallbackAxis.Equals(Other.FallbackAxis,.001) && FMath::IsNearlyEqual(ElapsedSeconds,Other.ElapsedSeconds,.025f);
}
void FVehicleRecoveryPhysicsState::Serialize(FArchive& Ar)
{
	Ar << Sequence << bActive << TargetUp << FallbackAxis << ElapsedSeconds;
	if (!IsValid()) { Ar.SetError(); }
}

bool FVehicleRecoveryClock::Advance(const FVehicleRecoveryConfig& Config, bool bEligible, bool bHeld, float DeltaTime)
{
	if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0) { return false; }
	const float PreviousFlipped = Flipped, PreviousCooldown = Cooldown;
	Cooldown = FMath::Max(0.f, Cooldown-DeltaTime);
	Flipped = bEligible ? FMath::Min(Config.FlippedSeconds, Flipped+DeltaTime) : 0;
	if (!bEligible || !bHeld || Cooldown > 0 || Flipped+KINDA_SMALL_NUMBER < Config.FlippedSeconds)
	{ Hold = 0; return false; }
	// Eligibility time and hold time never consume the same slice of game time.
	const float ValidHoldDelta = FMath::Min(FMath::Clamp(PreviousFlipped+DeltaTime-Config.FlippedSeconds, 0.f, DeltaTime),
		FMath::Clamp(DeltaTime-PreviousCooldown,0.f,DeltaTime));
	Hold = FMath::Min(Config.HoldSeconds, Hold+ValidHoldDelta);
	return Hold+KINDA_SMALL_NUMBER >= Config.HoldSeconds;
}

FVector HALVehicleRecovery::AngularAcceleration(const FVehicleRecoveryConfig& Config, const FVehicleRecoveryPhysicsState& State,
	const FVector& Up, const FVector& AngularVelocity, float DeltaTime)
{
	if (!State.bActive || !State.IsValid() || Up.ContainsNaN() || AngularVelocity.ContainsNaN() || DeltaTime <= 0) { return FVector::ZeroVector; }
	const double Dot = FMath::Clamp(FVector::DotProduct(Up,State.TargetUp), -1., 1.);
	FVector Cross = FVector::CrossProduct(Up,State.TargetUp);
	const double Sin = Cross.Size();
	const FVector Axis = Sin > .001 ? Cross/Sin : State.FallbackAxis;
	const FVector W = FVector::VectorPlaneProject(AngularVelocity,State.TargetUp);
	const FVector Desired = (Axis * (FMath::Atan2(Sin,Dot)*Config.AngularPositionGain) - W*Config.AngularDampingGain)
		.GetClampedToMaxSize(Config.MaximumAngularAcceleration);
	const FVector NextW = (W+Desired*DeltaTime).GetClampedToMaxSize(FMath::Max(double(Config.MaximumRecoveryAngularSpeed), W.Size()));
	return FVector::VectorPlaneProject((NextW-W)/DeltaTime, State.TargetUp).GetClampedToMaxSize(Config.MaximumAngularAcceleration);
}
