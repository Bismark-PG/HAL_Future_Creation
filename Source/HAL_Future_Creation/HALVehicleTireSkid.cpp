#include "HALVehiclePresentationComponent.h"
#include "HALVehiclePresentationConfig.h"
#include "HALAbilitySystemComponent.h"
#include "ArcadeVehicleMovementComponent.h"
#include "TestVehiclePawn.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitter.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

namespace
{
	TAutoConsoleVariable<int32> CVarTireSkidDebug(TEXT("hal.VehicleTireSkidDebug"), 0,
		TEXT("Cosmetic tire skid state transitions: 0=off, 1=on. No gameplay or physics changes."));
}

bool FHALTireSkidConfig::Validate(FString& Error) const
{
	if (!bEnabled) { return true; }
	auto Fail = [&Error](const TCHAR* Message) { Error = Message; return false; };
	if (!FMath::IsFinite(StartSpeed) || StartSpeed < 1 || StartSpeed > 10000
		|| !FMath::IsFinite(StopSpeed) || StopSpeed < 0 || StopSpeed > StartSpeed
		|| !FMath::IsFinite(DriftStartLateralSpeed) || DriftStartLateralSpeed < 1 || DriftStartLateralSpeed > 10000
		|| !FMath::IsFinite(DriftStopLateralSpeed) || DriftStopLateralSpeed < 0 || DriftStopLateralSpeed > DriftStartLateralSpeed
		|| !FMath::IsFinite(MaximumGroundGapCm) || MaximumGroundGapCm < 0 || MaximumGroundGapCm > 50
		|| !FMath::IsFinite(SurfaceOffsetCm) || SurfaceOffsetCm < 0 || SurfaceOffsetCm > 10
		|| ContactOffsetCm.ContainsNaN() || ContactOffsetCm.GetAbsMax() > 100
		|| !FMath::IsFinite(MaximumSegmentDistanceCm) || MaximumSegmentDistanceCm < 10 || MaximumSegmentDistanceCm > 1000
		|| !FMath::IsFinite(SlipRatio) || SlipRatio <= 0 || SlipRatio > 1
		|| !FMath::IsFinite(MaximumFadeSeconds) || MaximumFadeSeconds < .1f || MaximumFadeSeconds > 30)
	{ return Fail(TEXT("Tire skid requires finite ranges and stop thresholds no larger than start thresholds.")); }
	TSet<uint8> Slots;
	for (const auto Wheel : Wheels)
	{
		const uint8 Slot = uint8(Wheel);
		if (Slot >= 4 || Slots.Contains(Slot)) { return Fail(TEXT("Tire skid wheel slots must be valid and unique.")); }
		Slots.Add(Slot);
	}
	if (Slots.IsEmpty() || !System) { return Fail(TEXT("Enabled tire skid needs a Niagara System and at least one wheel.")); }
	const auto& Parameters = System->GetExposedParameters();
	if (SlipRatioParameter.IsNone() || RibbonIDParameter.IsNone()
		|| Parameters.IndexOf(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), SlipRatioParameter)) == INDEX_NONE
		|| Parameters.IndexOf(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), RibbonIDParameter)) == INDEX_NONE
		|| (!SurfaceNormalParameter.IsNone()
			&& Parameters.IndexOf(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), SurfaceNormalParameter)) == INDEX_NONE))
	{ return Fail(TEXT("Tire skid Niagara requires matching float SlipRatio, int RibbonID and optional vector SurfaceNormal User parameters.")); }
	for (const auto& Handle : System->GetEmitterHandles())
	{
		if (const auto* Data = Handle.GetEmitterData(); Data && Data->bLocalSpace)
		{ return Fail(TEXT("Tire skid emitters must use world space so old ground marks do not follow the car.")); }
	}
	return true;
}

uint8 HALVehiclePresentation::TireSkidInputs(float Brake, float Throttle, bool bHandbrake)
{
	return (bHandbrake ? 1 : 0) | (Brake > UE_KINDA_SMALL_NUMBER ? 2 : Throttle > UE_KINDA_SMALL_NUMBER ? 4 : 0);
}

uint8 HALVehiclePresentation::TireSkidReasons(const FHALTireSkidConfig& Config, uint8 Inputs,
	float ForwardSpeed, float LateralSpeed, uint8 Previous, bool bAllowed)
{
	if (!Config.bEnabled || !bAllowed || !FMath::IsFinite(ForwardSpeed) || !FMath::IsFinite(LateralSpeed)) { return 0; }
	uint8 Result = 0;
	const float Speed = FMath::Sqrt(FMath::Square(ForwardSpeed) + FMath::Square(LateralSpeed));
	if ((Inputs & 1) && Speed > UE_KINDA_SMALL_NUMBER && FMath::Abs(LateralSpeed) > UE_KINDA_SMALL_NUMBER
		&& Speed >= ((Previous & 1) ? Config.StopSpeed : Config.StartSpeed)
		&& FMath::Abs(LateralSpeed) >= ((Previous & 1) ? Config.DriftStopLateralSpeed : Config.DriftStartLateralSpeed))
	{ Result |= 1; }
	const float BrakeSpeed = (Previous & 2) ? Config.StopSpeed : Config.StartSpeed;
	if (((Inputs & 2) && ForwardSpeed > BrakeSpeed) || ((Inputs & 4) && ForwardSpeed < -BrakeSpeed)) { Result |= 2; }
	return Result;
}

uint8 UHALVehiclePresentationComponent::ReadTireSkidInputs() const
{
	const auto* Pawn = Cast<ATestVehiclePawn>(GetOwner());
	if (!Pawn || (!Pawn->GetController() && Pawn->HasAuthority())
		|| UHALAbilitySystemComponent::IsActorStunned(Pawn)) { return 0; }
	if (Pawn->IsLocallyControlled())
	{
		const auto& Input = Pawn->GetCurrentInputCommand();
		return HALVehiclePresentation::TireSkidInputs(Input.Brake, Input.Throttle, Input.bHandbrake);
	}
	if (Pawn->HasAuthority())
	{
		const auto* Movement = Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>();
		const auto Input = Movement ? Movement->GetPresentationInput() : FHALVehiclePresentationInput();
		return Input.bValid ? Input.TireSkidInputs : 0;
	}
	return ObserverTireSkidInputs;
}

void UHALVehiclePresentationComponent::StopTireSkid(int32 Slot, bool bImmediate)
{
	auto* Component = TireSkidComponents[Slot].Get();
	TireSkidComponents[Slot] = nullptr;
	if (!IsValid(Component)) { return; }
	if (bImmediate) { Component->DestroyComponent(); return; }
	Component->Deactivate();
	FadingTireSkids.Add({ Component, GetWorld()->GetTimeSeconds() + Config->TireSkid.MaximumFadeSeconds });
	// Bound retained segments even if a malformed effect never completes after Deactivate.
	if (FadingTireSkids.Num() > 16)
	{
		if (auto* Old = FadingTireSkids[0].Component.Get()) { Old->DestroyComponent(); }
		FadingTireSkids.RemoveAt(0);
	}
}

void UHALVehiclePresentationComponent::ClearTireSkids()
{
	for (int32 Slot = 0; Slot < 4; ++Slot) { StopTireSkid(Slot, true); }
	for (const auto& Old : FadingTireSkids) { if (auto* Component = Old.Component.Get()) { Component->DestroyComponent(); } }
	FadingTireSkids.Reset();
	for (auto& Support : WheelSupports) { Support = {}; }
}

void UHALVehiclePresentationComponent::UpdateTireSkids(const FVector& Velocity, bool bDrivingAllowed)
{
	if (!bTireSkidReady)
	{
		Frame.TireSkidReasons = Frame.TireSkidWheelMask = 0;
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	FadingTireSkids.RemoveAll([Now](const FHALFadingTireSkid& Old)
	{
		if (!Old.Component.IsValid()) { return true; }
		if (Now < Old.Deadline) { return false; }
		Old.Component->DestroyComponent(); return true;
	});
	const uint8 PreviousMask = Frame.TireSkidWheelMask, PreviousReasons = Frame.TireSkidReasons;
	const auto& Settings = Config->TireSkid;
	Frame.TireSkidReasons = HALVehiclePresentation::TireSkidReasons(Settings, ReadTireSkidInputs(),
		Frame.ForwardSpeedCmPerSecond, FVector::DotProduct(Velocity, Body->GetRightVector()), PreviousReasons,
		bTireSkidReady && bDrivingAllowed);
	Frame.TireSkidWheelMask = 0;
	for (const auto& Wheel : Config->Wheels)
	{
		const int32 Slot = uint8(Wheel.Wheel);
		const auto& Support = WheelSupports[Slot];
		const FVector Center = Mesh->GetComponentTransform().TransformPosition(ReferenceWheelCenters[Slot])
			+ Body->GetUpVector() * Frame.Wheel(Wheel.Wheel).SuspensionOffsetCm;
		const double Gap = FVector::DotProduct(Center - Support.Point, Support.Normal) - Wheel.RadiusCm;
		const bool bEmit = Frame.TireSkidReasons && Settings.Wheels.Contains(Wheel.Wheel) && Support.bHit
			&& FMath::Abs(Gap) <= Settings.MaximumGroundGapCm;
		if (!bEmit) { StopTireSkid(Slot, false); continue; }
		Frame.TireSkidWheelMask |= 1 << Slot;
		const FVector Point = Support.Point + Support.Normal * Settings.SurfaceOffsetCm
			+ Body->GetComponentQuat().RotateVector(Settings.ContactOffsetCm);
		const FRotator Rotation = FRotationMatrix::MakeFromXZ(
			FVector::VectorPlaneProject(Body->GetForwardVector(), Support.Normal), Support.Normal).Rotator();
		if (TireSkidComponents[Slot] && FVector::DistSquared(Point, LastTireSkidPoints[Slot]) > FMath::Square(Settings.MaximumSegmentDistanceCm))
		{ StopTireSkid(Slot, false); }
		if (!IsValid(TireSkidComponents[Slot]))
		{
			// No particles on headless servers/tests. The read-only decision still remains inspectable.
			if (GetWorld()->FXSystem.IsValid() && FApp::CanEverRender())
			{
				auto* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Settings.System, Point,
					Rotation, FVector::OneVector, true, false, ENCPoolMethod::None, false);
				TireSkidComponents[Slot] = Component;
				if (Component)
				{
					NextRibbonID = NextRibbonID == MAX_int32 ? 1 : NextRibbonID + 1;
					Component->SetVariableInt(Settings.RibbonIDParameter, NextRibbonID);
					Component->SetVariableFloat(Settings.SlipRatioParameter, Settings.SlipRatio);
					if (!Settings.SurfaceNormalParameter.IsNone()) { Component->SetVariableVec3(Settings.SurfaceNormalParameter, Support.Normal); }
					Component->Activate(true);
				}
			}
		}
		else
		{
			TireSkidComponents[Slot]->SetWorldLocationAndRotation(Point, Rotation);
			if (!Settings.SurfaceNormalParameter.IsNone())
			{ TireSkidComponents[Slot]->SetVariableVec3(Settings.SurfaceNormalParameter, Support.Normal); }
		}
		LastTireSkidPoints[Slot] = Point;
	}
	if (CVarTireSkidDebug.GetValueOnGameThread() && (PreviousMask != Frame.TireSkidWheelMask || PreviousReasons != Frame.TireSkidReasons))
	{
		UE_LOG(LogTemp, Display, TEXT("HAL TireSkid: Pawn=%s Role=%d Inputs=%u Reasons=%u Wheels=%u Forward=%.1f Lateral=%.1f"),
			*GetNameSafe(GetOwner()), int32(GetOwner()->GetLocalRole()), ReadTireSkidInputs(), Frame.TireSkidReasons,
			Frame.TireSkidWheelMask, Frame.ForwardSpeedCmPerSecond, FVector::DotProduct(Velocity, Body->GetRightVector()));
	}
}
