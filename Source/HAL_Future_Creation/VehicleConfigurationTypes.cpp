#include "VehicleConfigurationTypes.h"

bool FArcadeDriveConfig::Validate(FString& Error) const
{
	if (!FMath::IsFinite(ForwardAcceleration) || ForwardAcceleration < 0.0f) { Error = TEXT("FArcadeDriveConfig.ForwardAcceleration is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(ReverseAcceleration) || ReverseAcceleration < 0.0f) { Error = TEXT("FArcadeDriveConfig.ReverseAcceleration is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(BrakeDeceleration) || BrakeDeceleration < 0.0f) { Error = TEXT("FArcadeDriveConfig.BrakeDeceleration is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(MaxForwardSpeed) || MaxForwardSpeed < 0.0f) { Error = TEXT("FArcadeDriveConfig.MaxForwardSpeed is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(MaxReverseSpeed) || MaxReverseSpeed < 0.0f) { Error = TEXT("FArcadeDriveConfig.MaxReverseSpeed is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(ReverseEngageSpeed) || ReverseEngageSpeed < 0.0f) { Error = TEXT("FArcadeDriveConfig.ReverseEngageSpeed is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(CoastingDragRate) || CoastingDragRate < 0.0f) { Error = TEXT("FArcadeDriveConfig.CoastingDragRate is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(OverspeedCorrectionRate) || OverspeedCorrectionRate < 0.0f) { Error = TEXT("FArcadeDriveConfig.OverspeedCorrectionRate is outside its valid finite range."); return false; }
	return true;
}

bool FArcadeGripConfig::Validate(FString& Error) const
{
	if (!FMath::IsFinite(LateralGripRate) || LateralGripRate < 0.0f) { Error = TEXT("FArcadeGripConfig.LateralGripRate is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(HandbrakeGripRate) || HandbrakeGripRate < 0.0f) { Error = TEXT("FArcadeGripConfig.HandbrakeGripRate is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(MaxLateralGripAcceleration) || MaxLateralGripAcceleration < 0.0f) { Error = TEXT("FArcadeGripConfig.MaxLateralGripAcceleration is outside its valid finite range."); return false; }
	return true;
}

bool FArcadeSteeringConfig::Validate(FString& Error) const
{
	if (!FMath::IsFinite(SteeringAngularAcceleration) || SteeringAngularAcceleration < 0.0f) { Error = TEXT("FArcadeSteeringConfig.SteeringAngularAcceleration is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(FullSteeringSpeed) || FullSteeringSpeed < 1.0f) { Error = TEXT("FArcadeSteeringConfig.FullSteeringSpeed is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(HighSpeedSteeringStart) || HighSpeedSteeringStart < 0.0f) { Error = TEXT("FArcadeSteeringConfig.HighSpeedSteeringStart is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(HighSpeedSteeringScale) || HighSpeedSteeringScale < 0.0f || HighSpeedSteeringScale > 1.0f) { Error = TEXT("FArcadeSteeringConfig.HighSpeedSteeringScale is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(HandbrakeSteeringMultiplier) || HandbrakeSteeringMultiplier < 0.0f) { Error = TEXT("FArcadeSteeringConfig.HandbrakeSteeringMultiplier is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(HandbrakeMinimumSteeringAuthority) || HandbrakeMinimumSteeringAuthority < 0.0f || HandbrakeMinimumSteeringAuthority > 1.0f) { Error = TEXT("FArcadeSteeringConfig.HandbrakeMinimumSteeringAuthority is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(HandbrakeReverseSteeringSpeed) || HandbrakeReverseSteeringSpeed < 0.0f) { Error = TEXT("FArcadeSteeringConfig.HandbrakeReverseSteeringSpeed is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(YawDampingRate) || YawDampingRate < 0.0f) { Error = TEXT("FArcadeSteeringConfig.YawDampingRate is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(HandbrakeYawDampingRate) || HandbrakeYawDampingRate < 0.0f) { Error = TEXT("FArcadeSteeringConfig.HandbrakeYawDampingRate is outside its valid finite range."); return false; }
	return true;
}

bool FArcadeWallEscapeConfig::Validate(FString& Error) const
{
	if (!FMath::IsFinite(WallEscapeEnterSpeed) || WallEscapeEnterSpeed < 0.0f) { Error = TEXT("FArcadeWallEscapeConfig.WallEscapeEnterSpeed is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(WallEscapeExitSpeed) || WallEscapeExitSpeed < 0.0f) { Error = TEXT("FArcadeWallEscapeConfig.WallEscapeExitSpeed is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(WallEscapeProbeDistance) || WallEscapeProbeDistance < 1.0f || WallEscapeProbeDistance > 100.0f) { Error = TEXT("FArcadeWallEscapeConfig.WallEscapeProbeDistance is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(WallEscapeMinimumSteering) || WallEscapeMinimumSteering < 0.01f || WallEscapeMinimumSteering > 1.0f) { Error = TEXT("FArcadeWallEscapeConfig.WallEscapeMinimumSteering is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(WallEscapeIntoWallThrottleScale) || WallEscapeIntoWallThrottleScale < 0.0f || WallEscapeIntoWallThrottleScale > 1.0f) { Error = TEXT("FArcadeWallEscapeConfig.WallEscapeIntoWallThrottleScale is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(WallEscapeTargetYawRate) || WallEscapeTargetYawRate < 0.0f) { Error = TEXT("FArcadeWallEscapeConfig.WallEscapeTargetYawRate is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(WallEscapeYawResponseRate) || WallEscapeYawResponseRate < 0.0f) { Error = TEXT("FArcadeWallEscapeConfig.WallEscapeYawResponseRate is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(WallEscapeMaxYawAcceleration) || WallEscapeMaxYawAcceleration < 0.0f) { Error = TEXT("FArcadeWallEscapeConfig.WallEscapeMaxYawAcceleration is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(WallEscapeBlendInTime) || WallEscapeBlendInTime < 0.0f) { Error = TEXT("FArcadeWallEscapeConfig.WallEscapeBlendInTime is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(WallEscapeBlendOutTime) || WallEscapeBlendOutTime < 0.0f) { Error = TEXT("FArcadeWallEscapeConfig.WallEscapeBlendOutTime is outside its valid finite range."); return false; }
	if (WallEscapeExitSpeed < WallEscapeEnterSpeed) { Error = TEXT("WallEscapeExitSpeed must be >= WallEscapeEnterSpeed."); return false; }
	return true;
}

bool FArcadeDebugConfig::Validate(FString& Error) const
{
	return true;
}

bool FArcadeGroundConfig::Validate(FString& Error) const
{
	if (!FMath::IsFinite(GroundTraceExtraDistance) || GroundTraceExtraDistance < 0.0f) { Error = TEXT("FArcadeGroundConfig.GroundTraceExtraDistance is outside its valid finite range."); return false; }
	if (GroundTraceChannel >= ECC_MAX) { Error = TEXT("GroundTraceChannel is not a collision channel."); return false; }
	return true;
}

bool FArcadeVehicleConfig::Validate(FString& Error) const
{
	return Drive.Validate(Error) && Grip.Validate(Error) && Steering.Validate(Error) && WallEscape.Validate(Error) && Debug.Validate(Error) && Ground.Validate(Error);
}

bool FBallControlAcquisitionConfig::Validate(FString& Error) const
{
	if (!FMath::IsFinite(AcquisitionRadius) || AcquisitionRadius < 0.0f) { Error = TEXT("FBallControlAcquisitionConfig.AcquisitionRadius is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(AcquisitionCheckInterval) || AcquisitionCheckInterval < 0.01f) { Error = TEXT("FBallControlAcquisitionConfig.AcquisitionCheckInterval is outside its valid finite range."); return false; }
	if (LineOfSightTraceChannel >= ECC_MAX) { Error = TEXT("LineOfSightTraceChannel is not a collision channel."); return false; }
	return true;
}

bool FBallControlFollowConfig::Validate(FString& Error) const
{
	if (!FMath::IsFinite(MaxControlledDistance) || MaxControlledDistance < 0.0f) { Error = TEXT("FBallControlFollowConfig.MaxControlledDistance is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(ControlPositionStrength) || ControlPositionStrength < 0.0f) { Error = TEXT("FBallControlFollowConfig.ControlPositionStrength is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(ControlVelocityStrength) || ControlVelocityStrength < 0.0f) { Error = TEXT("FBallControlFollowConfig.ControlVelocityStrength is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(ControlMaxForce) || ControlMaxForce < 0.0f) { Error = TEXT("FBallControlFollowConfig.ControlMaxForce is outside its valid finite range."); return false; }
	return true;
}

bool FBallControlLaunchConfig::Validate(FString& Error) const
{
	if (!FMath::IsFinite(LaunchSpeedIncrement) || LaunchSpeedIncrement < 0.0f) { Error = TEXT("FBallControlLaunchConfig.LaunchSpeedIncrement is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(BaseRecoilDeltaSpeed) || BaseRecoilDeltaSpeed < 0.0f) { Error = TEXT("FBallControlLaunchConfig.BaseRecoilDeltaSpeed is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(MovingRecoilFraction) || MovingRecoilFraction < 0.0f || MovingRecoilFraction > 1.0f) { Error = TEXT("FBallControlLaunchConfig.MovingRecoilFraction is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(MaxRecoilDeltaSpeed) || MaxRecoilDeltaSpeed < 0.0f) { Error = TEXT("FBallControlLaunchConfig.MaxRecoilDeltaSpeed is outside its valid finite range."); return false; }
	if (MaxRecoilDeltaSpeed < BaseRecoilDeltaSpeed) { Error = TEXT("MaxRecoilDeltaSpeed must be >= BaseRecoilDeltaSpeed."); return false; }
	return true;
}

bool FBallControlForcedReleaseConfig::Validate(FString& Error) const
{
	if (!FMath::IsFinite(DropCollisionImpulseThreshold) || DropCollisionImpulseThreshold < 0.0f) { Error = TEXT("FBallControlForcedReleaseConfig.DropCollisionImpulseThreshold is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(DropReacquireLockDuration) || DropReacquireLockDuration < 0.0f) { Error = TEXT("FBallControlForcedReleaseConfig.DropReacquireLockDuration is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(DropBallImpulse) || DropBallImpulse < 0.0f) { Error = TEXT("FBallControlForcedReleaseConfig.DropBallImpulse is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(DropBallUpwardImpulse) || DropBallUpwardImpulse < 0.0f) { Error = TEXT("FBallControlForcedReleaseConfig.DropBallUpwardImpulse is outside its valid finite range."); return false; }
	return true;
}

bool FBallControlDebugConfig::Validate(FString& Error) const
{
	return true;
}

bool FBallControlConfig::Validate(FString& Error) const
{
	return Acquisition.Validate(Error) && Control.Validate(Error) && Launch.Validate(Error) && ForcedRelease.Validate(Error) && Debug.Validate(Error);
}

bool FBallStateConfig::Validate(FString& Error) const
{
	if (!FMath::IsFinite(LowSpeedThreshold) || LowSpeedThreshold < 0.0f) { Error = TEXT("FBallStateConfig.LowSpeedThreshold is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(LowSpeedRequiredDuration) || LowSpeedRequiredDuration < 0.0f) { Error = TEXT("FBallStateConfig.LowSpeedRequiredDuration is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(LowSpeedCheckInterval) || LowSpeedCheckInterval < 0.01f) { Error = TEXT("FBallStateConfig.LowSpeedCheckInterval is outside its valid finite range."); return false; }
	return true;
}

bool FBallAcquisitionConfig::Validate(FString& Error) const
{
	if (!FMath::IsFinite(PostLaunchPickupLockDuration) || PostLaunchPickupLockDuration < 0.0f) { Error = TEXT("FBallAcquisitionConfig.PostLaunchPickupLockDuration is outside its valid finite range."); return false; }
	return true;
}

bool FBallDamageConfig::Validate(FString& Error) const
{
	if (!FMath::IsFinite(VehicleHitDamage) || VehicleHitDamage < 0.0f) { Error = TEXT("FBallDamageConfig.VehicleHitDamage is outside its valid finite range."); return false; }
	if (!FMath::IsFinite(KnockbackStrengthMultiplier) || KnockbackStrengthMultiplier < 0.0f) { Error = TEXT("FBallDamageConfig.KnockbackStrengthMultiplier is outside its valid finite range."); return false; }
	return true;
}

bool FBallDebugConfig::Validate(FString& Error) const
{
	return true;
}

bool FBallGameplayConfig::Validate(FString& Error) const
{
	return State.Validate(Error) && Acquisition.Validate(Error) && Damage.Validate(Error) && Debug.Validate(Error);
}

bool FVehicleInitialHealthConfig::Validate(FString& Error) const
{
	if (!FMath::IsFinite(MaxHP) || MaxHP < 1.0f) { Error = TEXT("FVehicleInitialHealthConfig.MaxHP is outside its valid finite range."); return false; }
	return true;
}

bool FVehicleHealthDebugConfig::Validate(FString& Error) const
{
	return true;
}

bool FVehicleHealthConfig::Validate(FString& Error) const
{
	return Health.Validate(Error) && Debug.Validate(Error);
}
