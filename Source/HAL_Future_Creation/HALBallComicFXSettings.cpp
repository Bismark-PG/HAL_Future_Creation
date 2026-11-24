#include "HALBallComicFXSettings.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "Misc/PackageName.h"

bool FHALBallComicFXConfig::Validate(FString& Error) const
{
	Error.Reset();
	if (!bEnabled) { return true; }
	if (bTrail && !TrailSystem.IsNull())
	{
		if (TrailColorParameter.IsNone() || TrailWidthParameter.IsNone())
		{ Error = TEXT("Ink trail colour and width parameter names must be set while a trail system is assigned."); return false; }
		if (!FMath::IsFinite(TrailWidthDiameterFraction) || TrailWidthDiameterFraction <= 0)
		{ Error = TEXT("Ink trail width fraction must be positive."); return false; }
	}
	if (bAim && !AimMaterial.IsNull())
	{
		if (AimMesh.IsNull()) { Error = TEXT("Aim arrow needs a quad mesh while a material is assigned."); return false; }
		if (!FMath::IsFinite(AimLengthWidthFraction) || AimLengthWidthFraction <= 0 || !FMath::IsFinite(AimWidthWidthFraction) || AimWidthWidthFraction <= 0
			|| !FMath::IsFinite(AimGapWidthFraction) || AimGapWidthFraction < 0 || !FMath::IsFinite(AimGroundLiftCm) || AimGroundLiftCm < 0)
		{ Error = TEXT("Aim arrow size must be positive and its offsets must not be negative."); return false; }
		if (!FMath::IsFinite(AimPopSeconds) || AimPopSeconds < 0 || !FMath::IsFinite(AimPopOvershoot) || AimPopOvershoot < 1)
		{ Error = TEXT("Aim arrow pop time must not be negative and its overshoot must be at least one."); return false; }
		if (AimColorParameter.IsNone()) { Error = TEXT("Aim arrow colour parameter name must be set."); return false; }
	}
	if (bHoldRings && (!HoldBallMaterial.IsNull() || !HoldRoofMaterial.IsNull()))
	{
		if (HoldRoofMesh.IsNull()) { Error = TEXT("Hold rings and holder marker need a quad mesh while a material is assigned."); return false; }
		// At one diameter or less the sprite has no room outside the ball for the rings.
		if (!FMath::IsFinite(HoldBallSizeDiameterFraction) || HoldBallSizeDiameterFraction <= 1 || !FMath::IsFinite(HoldRoofSizeWidthFraction)
			|| HoldRoofSizeWidthFraction <= 0 || !FMath::IsFinite(HoldMarkerLiftWidthFraction) || HoldMarkerLiftWidthFraction < 0
			|| !FMath::IsFinite(HoldMarkerBackLengthFraction) || HoldMarkerBackLengthFraction < 0)
		{ Error = TEXT("Hold ring sizes must be positive, the ball sprite must be wider than the ball, and the roof lift must not be negative."); return false; }
		if (!FMath::IsFinite(HoldPopSeconds) || HoldPopSeconds < 0 || !FMath::IsFinite(HoldPopOvershoot) || HoldPopOvershoot < 1
			|| !FMath::IsFinite(HoldShrinkSeconds) || HoldShrinkSeconds < 0)
		{ Error = TEXT("Hold ring pop and shrink times must not be negative and the overshoot must be at least one."); return false; }
		if (HoldColorParameter.IsNone() || HoldBallRadiusParameter.IsNone()) { Error = TEXT("Hold ring parameter names must be set."); return false; }
	}
	if (bIncomingWarning && (!FMath::IsFinite(IncomingMaximumSeconds) || IncomingMaximumSeconds <= 0 || !FMath::IsFinite(IncomingMissDistanceCm)
		|| IncomingMissDistanceCm <= 0 || !FMath::IsFinite(IncomingMinimumSpeed) || IncomingMinimumSpeed < 0
		|| !FMath::IsFinite(IncomingArrowScreenFraction) || IncomingArrowScreenFraction <= 0 || !FMath::IsFinite(IncomingEdgeInsetFraction)
		|| IncomingEdgeInsetFraction < 0 || !FMath::IsFinite(IncomingConfirmSeconds) || IncomingConfirmSeconds < 0
		|| !FMath::IsFinite(IncomingMinimumShowSeconds) || IncomingMinimumShowSeconds < 0 || IncomingMaximumArrows <= 0
		|| !FMath::IsFinite(IncomingPopSeconds) || IncomingPopSeconds < 0 || !FMath::IsFinite(IncomingBoilFps) || IncomingBoilFps < 0
		|| !FMath::IsFinite(IncomingWobbleDegrees) || IncomingWobbleDegrees < 0))
	{ Error = TEXT("Incoming warning window, distances, sizes and counts must be finite and positive."); return false; }
	return true;
}

#if WITH_EDITOR
bool UHALBallComicFXSettings::CollectCookPackages(TArray<FName>& Packages, FString& Error) const
{
	Error.Reset();
	if (!Comic.Validate(Error)) { return false; }
	if (!Comic.bEnabled) { return true; }
	const auto Root = [&Packages, &Error](const FSoftObjectPath& Path, bool bLoaded, const TCHAR* Label)
	{
		const FString Package = Path.GetLongPackageName();
		if (!FPackageName::IsValidLongPackageName(Package) || !bLoaded)
		{ Error = FString::Printf(TEXT("%s %s: missing package or wrong asset class."), Label, *Path.ToString()); return false; }
		Packages.AddUnique(FName(*Package));
		return true;
	};
	if (Comic.bTrail && !Comic.TrailSystem.IsNull()
		&& !Root(Comic.TrailSystem.ToSoftObjectPath(), Comic.TrailSystem.LoadSynchronous() != nullptr, TEXT("Ball ink trail system"))) { return false; }
	if (Comic.bAim && !Comic.AimMaterial.IsNull()
		&& (!Root(Comic.AimMaterial.ToSoftObjectPath(), Comic.AimMaterial.LoadSynchronous() != nullptr, TEXT("Ball aim arrow material"))
			|| !Root(Comic.AimMesh.ToSoftObjectPath(), Comic.AimMesh.LoadSynchronous() != nullptr, TEXT("Ball aim arrow mesh")))) { return false; }
	if (Comic.bHoldRings && !Comic.HoldBallMaterial.IsNull()
		&& !Root(Comic.HoldBallMaterial.ToSoftObjectPath(), Comic.HoldBallMaterial.LoadSynchronous() != nullptr, TEXT("Ball hold ring material"))) { return false; }
	if (Comic.bHoldRings && !Comic.HoldRoofMaterial.IsNull()
		&& (!Root(Comic.HoldRoofMaterial.ToSoftObjectPath(), Comic.HoldRoofMaterial.LoadSynchronous() != nullptr, TEXT("Holder marker material"))
			|| !Root(Comic.HoldRoofMesh.ToSoftObjectPath(), Comic.HoldRoofMesh.LoadSynchronous() != nullptr, TEXT("Holder marker mesh")))) { return false; }
	return true;
}
#endif
