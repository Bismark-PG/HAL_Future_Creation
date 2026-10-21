#include "HALVehiclePresentationConfig.h"
#include "HALVehicleAnimInstance.h"
#include "Animation/AnimClassInterface.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/DataValidation.h"

bool FHALChassisPresentationConfig::Validate(FString& Error) const
{
	if (uint8(PitchAxis) > uint8(EHALBoneAxis::Z) || uint8(RollAxis) > uint8(EHALBoneAxis::Z) || PitchAxis == RollAxis
		|| !FMath::IsFinite(PitchSign) || FMath::Abs(PitchSign) != 1 || !FMath::IsFinite(RollSign) || FMath::Abs(RollSign) != 1
		|| !FMath::IsFinite(MaxPitchDegrees) || MaxPitchDegrees < 0 || MaxPitchDegrees > 10
		|| !FMath::IsFinite(MaxRollDegrees) || MaxRollDegrees < 0 || MaxRollDegrees > 10
		|| !FMath::IsFinite(FullPitchAcceleration) || FullPitchAcceleration < 1 || FullPitchAcceleration > 10000
		|| !FMath::IsFinite(FullRollAcceleration) || FullRollAcceleration < 1 || FullRollAcceleration > 10000
		|| !FMath::IsFinite(ResponseSeconds) || ResponseSeconds < .01f || ResponseSeconds > 2
		|| !FMath::IsFinite(AccelerationDeadZone) || AccelerationDeadZone < 0 || AccelerationDeadZone > 500
		|| AccelerationDeadZone >= FMath::Min(FullPitchAcceleration, FullRollAcceleration)
		|| !FMath::IsFinite(CorrectionResetDistanceCm) || CorrectionResetDistanceCm < 1 || CorrectionResetDistanceCm > 1000
		|| !FMath::IsFinite(MaximumSampleAcceleration) || MaximumSampleAcceleration < 1000 || MaximumSampleAcceleration > 100000
		|| MaximumSampleAcceleration < FMath::Max(FullPitchAcceleration, FullRollAcceleration))
	{
		Error = TEXT("Chassis requires distinct valid axes, +/-1 signs, finite bounded lean/sampling settings and dead zone below full acceleration.");
		return false;
	}
	return true;
}

UHALVehiclePresentationConfig::UHALVehiclePresentationConfig()
{
	const TCHAR* Names[] = { TEXT("FL"), TEXT("FR"), TEXT("BL"), TEXT("BR") };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		auto& Wheel = Wheels.AddDefaulted_GetRef();
		Wheel.Wheel = EHALVehicleWheel(Index);
		Wheel.SuspensionBone = FName(*FString::Printf(TEXT("Wheel_%s_suspension_jnt"), Names[Index]));
		Wheel.SteeringBone = FName(*FString::Printf(TEXT("Wheel_%s_steer_jnt"), Names[Index]));
		Wheel.SpinBone = FName(*FString::Printf(TEXT("Wheel_%s_spin_jnt"), Names[Index]));
		Wheel.bSteers = Index < 2;
	}
}

bool UHALVehiclePresentationConfig::ValidateSettings(FString& Error) const
{
	if (!Chassis.Validate(Error)) { return false; }
	auto Fail = [&Error](const TCHAR* Message) { Error = Message; return false; };
	const FVector Scale = RelativeTransform.GetScale3D();
	if (RelativeTransform.ContainsNaN() || !RelativeTransform.GetRotation().IsNormalized()
		|| Scale.GetMin() <= 0 || !FMath::IsNearlyEqual(Scale.X, Scale.Y) || !FMath::IsNearlyEqual(Scale.X, Scale.Z))
	{ return Fail(TEXT("Presentation RelativeTransform requires finite values, normalized rotation and positive uniform scale.")); }
	if (!FMath::IsFinite(MaxSteeringDegrees) || MaxSteeringDegrees < 0 || MaxSteeringDegrees > 60
		|| !FMath::IsFinite(SteeringResponseSeconds) || SteeringResponseSeconds < 0 || SteeringResponseSeconds > 2
		|| !FMath::IsFinite(RollingDeadZoneCmPerSecond) || RollingDeadZoneCmPerSecond < 0 || RollingDeadZoneCmPerSecond > 100
		|| !FMath::IsFinite(MaxSampleDeltaSeconds) || MaxSampleDeltaSeconds < 0.034f || MaxSampleDeltaSeconds > 0.25f
		|| !FMath::IsFinite(TeleportResetDistanceCm) || TeleportResetDistanceCm < 1)
	{ return Fail(TEXT("Presentation sampling/steering values are outside their finite ranges.")); }
	if (Wheels.Num() != 4 || RootBone.IsNone() || ChassisBone.IsNone() || RootBone == ChassisBone)
	{ return Fail(TEXT("Presentation requires Root, Chassis and exactly four wheel slots.")); }
	TSet<uint8> Slots;
	TSet<FName> Bones { RootBone, ChassisBone };
	for (const auto& Wheel : Wheels)
	{
		const uint8 Slot = uint8(Wheel.Wheel);
		if (Slot >= 4 || Slots.Contains(Slot)) { return Fail(TEXT("Wheel slots must uniquely cover FL/FR/BL/BR.")); }
		Slots.Add(Slot);
		if (!FMath::IsFinite(Wheel.RadiusCm) || Wheel.RadiusCm < 0.1f
			|| uint8(Wheel.SteeringAxis) > uint8(EHALBoneAxis::Z) || uint8(Wheel.SpinAxis) > uint8(EHALBoneAxis::Z)
			|| !FMath::IsFinite(Wheel.SteeringSign) || FMath::Abs(Wheel.SteeringSign) != 1.f
			|| !FMath::IsFinite(Wheel.SpinSign) || FMath::Abs(Wheel.SpinSign) != 1.f)
		{ return Fail(TEXT("Every wheel needs a measured positive world radius, valid axes and signs of exactly +1/-1.")); }
		for (const FName Bone : { Wheel.SuspensionBone, Wheel.SteeringBone, Wheel.SpinBone })
		{
			if (Bone.IsNone() || Bones.Contains(Bone)) { return Fail(TEXT("Control bones must be named and unique across all wheel slots, Root and Chassis.")); }
			Bones.Add(Bone);
		}
	}
	return true;
}

bool UHALVehiclePresentationConfig::ValidateRig(const FReferenceSkeleton& Skeleton, FString& Error) const
{
	if (!ValidateSettings(Error)) { return false; }
	const int32 Root = Skeleton.FindBoneIndex(RootBone);
	const int32 ChassisIndex = Skeleton.FindBoneIndex(ChassisBone);
	if (Root == INDEX_NONE || ChassisIndex == INDEX_NONE || Skeleton.GetParentIndex(Root) != INDEX_NONE
		|| Skeleton.GetParentIndex(ChassisIndex) != Root)
	{ Error = TEXT("Rig needs a root bone and Chassis directly below that root."); return false; }
	for (const auto& Wheel : Wheels)
	{
		const int32 Suspension = Skeleton.FindBoneIndex(Wheel.SuspensionBone);
		const int32 Steering = Skeleton.FindBoneIndex(Wheel.SteeringBone);
		const int32 Spin = Skeleton.FindBoneIndex(Wheel.SpinBone);
		if (Suspension == INDEX_NONE || Steering == INDEX_NONE || Spin == INDEX_NONE
			|| Skeleton.GetParentIndex(Suspension) != Root || Skeleton.GetParentIndex(Steering) != Suspension
			|| Skeleton.GetParentIndex(Spin) != Steering)
		{ Error = FString::Printf(TEXT("Wheel %d must be Root -> suspension -> steer -> spin; missing or incorrectly parented control bone."), uint8(Wheel.Wheel)); return false; }
	}
	return true;
}

bool UHALVehiclePresentationConfig::Validate(FString& Error) const
{
	if (!bReadyForUse) { Error = TEXT("Presentation is not marked ReadyForUse after model/axis/radius calibration."); return false; }
	if (!ValidateSettings(Error)) { return false; }
	if (!Mesh || !Mesh->GetSkeleton() || !AnimClass || AnimClass->HasAnyClassFlags(CLASS_Abstract))
	{ Error = TEXT("Presentation requires a Skeletal Mesh with Skeleton and a concrete HALVehicleAnimInstance class."); return false; }
	const IAnimClassInterface* Animation = IAnimClassInterface::GetFromClass(AnimClass);
	if (Animation && Animation->GetTargetSkeleton() != Mesh->GetSkeleton())
	{ Error = TEXT("AnimBP must target this mesh's exact Skeleton; cross-skeleton reuse is not implicit."); return false; }
	return ValidateRig(Mesh->GetRefSkeleton(), Error);
}

#if WITH_EDITOR
EDataValidationResult UHALVehiclePresentationConfig::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);
	FString Error;
	if (!Validate(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
	return EDataValidationResult::Valid;
}
#endif
