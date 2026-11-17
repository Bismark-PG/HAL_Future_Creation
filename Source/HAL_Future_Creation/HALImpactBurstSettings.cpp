#include "HALImpactBurstSettings.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"

namespace
{
	bool BurstRange(float Minimum, float Maximum, const TCHAR* Label, FString& Error)
	{
		if (!FMath::IsFinite(Minimum) || !FMath::IsFinite(Maximum) || Minimum <= 0 || Maximum <= Minimum)
		{ Error = FString::Printf(TEXT("%s must be a finite ascending range."), Label); return false; }
		return true;
	}
	bool BurstParameter(FName Name, const TCHAR* Label, FString& Error)
	{
		if (Name.IsNone()) { Error = FString::Printf(TEXT("%s parameter name must be set."), Label); return false; }
		return true;
	}
}

bool FHALImpactBurstConfig::Validate(FString& Error) const
{
	Error.Reset();
	if (!bEnabled) { return true; }
	if (!BurstRange(MinimumPushSpeed, MaximumPushSpeed, TEXT("Push speed"), Error)
		|| !BurstRange(MinimumSizeCm, MaximumSizeCm, TEXT("Size"), Error)
		|| !BurstRange(MinimumSpikes, MaximumSpikes, TEXT("Spikes"), Error)) { return false; }
	if (!FMath::IsFinite(LifetimeSeconds) || LifetimeSeconds <= 0 || !FMath::IsFinite(StrengthLifetimeSeconds) || StrengthLifetimeSeconds < 0)
	{ Error = TEXT("Lifetime must be positive and the strength bonus must not be negative."); return false; }
	if (MaximumConcurrent <= 0) { Error = TEXT("Maximum concurrent bursts must be at least one."); return false; }
	if (!FMath::IsFinite(MaximumEventAgeSeconds) || MaximumEventAgeSeconds <= 0)
	{ Error = TEXT("Maximum event age must be positive."); return false; }
	if (!FMath::IsFinite(MaximumPointDistanceCm) || MaximumPointDistanceCm <= 0 || !FMath::IsFinite(FallbackOffsetCm) || FallbackOffsetCm < 0
		|| !FMath::IsFinite(SurfaceOffsetCm) || SurfaceOffsetCm < 0 || !FMath::IsFinite(VerticalOffsetCm) || VerticalOffsetCm < 0)
	{ Error = TEXT("Placement distances must be finite and non-negative."); return false; }
	return BurstParameter(AgeParameter, TEXT("Age"), Error) && BurstParameter(StrengthParameter, TEXT("Strength"), Error)
		&& BurstParameter(SeedParameter, TEXT("Seed"), Error) && BurstParameter(SpikesParameter, TEXT("Spikes"), Error) && BurstParameter(TierParameter, TEXT("Tier"), Error)
		&& BurstParameter(FillColorParameter, TEXT("Fill color"), Error) && BurstParameter(EdgeColorParameter, TEXT("Edge color"), Error)
		&& BurstParameter(ColorModeParameter, TEXT("Color mode"), Error) && BurstParameter(SourceColorParameter, TEXT("Source color"), Error)
		&& BurstParameter(TargetColorParameter, TEXT("Target color"), Error);
}

#if WITH_EDITOR
bool UHALImpactBurstSettings::CollectCookPackages(TArray<FName>& Packages, FString& Error) const
{
	Error.Reset();
	if (!Burst.Validate(Error)) { return false; }
	if (!Burst.bEnabled || Burst.Material.IsNull()) { return true; }
	const FString Package = Burst.Material.ToSoftObjectPath().GetLongPackageName();
	if (!FPackageName::IsValidLongPackageName(Package) || Package.StartsWith(TEXT("/Engine/Transient")))
	{ Error = TEXT("Impact burst material must point to a saved asset package."); return false; }
	if (!Burst.Material.LoadSynchronous())
	{ Error = FString::Printf(TEXT("Impact burst material %s: missing package or wrong asset class."), *Burst.Material.ToString()); return false; }
	Packages.AddUnique(FName(*Package));
	return true;
}
#endif
