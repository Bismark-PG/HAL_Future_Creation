#include "HALImpactTypes.h"

bool HALImpact::IsValidTier(EHALImpactTier Tier) { return uint8(Tier) <= uint8(EHALImpactTier::Heavy); }

bool HALImpact::IsValidPushPreset(EHALImpactPushPreset Preset) { return uint8(Preset) <= uint8(EHALImpactPushPreset::ExtraLong); }

float FHALImpactPushRules::GetMultiplier(EHALImpactPushPreset Preset) const
{
	switch (Preset)
	{
	case EHALImpactPushPreset::Standard: return 1;
	case EHALImpactPushPreset::Short: return ShortMultiplier;
	case EHALImpactPushPreset::Long: return LongMultiplier;
	case EHALImpactPushPreset::ExtraLong: return ExtraLongMultiplier;
	default: return -1; // Invalid content must never silently become Standard.
	}
}

bool FHALImpactPushRules::Validate(FString& Error) const
{
	if (!FMath::IsFinite(ShortMultiplier) || ShortMultiplier < 0 || ShortMultiplier > 1
		|| !FMath::IsFinite(LongMultiplier) || LongMultiplier < 1 || LongMultiplier > 3
		|| !FMath::IsFinite(ExtraLongMultiplier) || ExtraLongMultiplier < LongMultiplier || ExtraLongMultiplier > 3)
	{
		Error = TEXT("Horizontal push multipliers require Short in 0..1 and 1 <= Long <= ExtraLong <= 3; all finite. Standard is fixed at 1.");
		return false;
	}
	return true;
}

EHALImpactTier HALImpact::Reduce(EHALImpactTier Source, int32 Reduction, bool bImmune)
{
	return !IsValidTier(Source) || bImmune ? EHALImpactTier::None
		: EHALImpactTier(FMath::Max(0, int32(Source) - FMath::Clamp(Reduction, 0, 3)));
}

EVehicleKnockbackTier HALImpact::ToLegacy(EHALImpactTier Tier)
{
	return Tier == EHALImpactTier::Heavy ? EVehicleKnockbackTier::Heavy
		: Tier == EHALImpactTier::Medium ? EVehicleKnockbackTier::Medium : EVehicleKnockbackTier::Light;
}

bool FHALVehicleImpactConfig::Validate(FString& Error) const
{
	if (uint8(Version) > 1 || !HALImpact::IsValidTier(CollisionTierCap) || !HALImpact::IsValidTier(MinimumDropTier)
		|| (bDropOnImpact && MinimumDropTier == EHALImpactTier::None) || TierReduction < 0 || TierReduction > 3)
	{
		Error = TEXT("Impact config needs a known version/tiers, a nonzero enabled drop threshold and integer reduction 0..3.");
		return false;
	}
	return true;
}

bool FHALAuthoredImpactConfig::Validate(FString& Error) const
{
	if (uint8(Version) > 1 || !HALImpact::IsValidTier(Tier) || !HALImpact::IsValidPushPreset(PushPreset)
		|| (Version == EHALImpactVersion::Legacy && PushPreset != EHALImpactPushPreset::Standard))
	{
		Error = TEXT("Authored impact needs a known version/tier/push preset; non-Standard push requires TieredV1.");
		return false;
	}
	return true;
}

bool FHALCollisionImpactRules::Validate(FString& Error) const
{
	if (uint8(Version) > 1 || !FMath::IsFinite(LightThreshold) || LightThreshold <= 0
		|| !FMath::IsFinite(MediumThreshold) || MediumThreshold < LightThreshold
		|| !FMath::IsFinite(HeavyThreshold) || HeavyThreshold < MediumThreshold
		|| !HALImpact::IsValidTier(EnvironmentTierCap) || !HALImpact::IsValidTier(FreeBallTierCap)
		|| !FMath::IsFinite(ContactRearmSeconds) || ContactRearmSeconds < 0.05f
		|| !FMath::IsFinite(MaximumRuleRestitution) || MaximumRuleRestitution < 0 || MaximumRuleRestitution > 0.5f)
	{
		Error = TEXT("Invalid collision impact calibration, caps, contact rearm or restitution limit.");
		return false;
	}
	return true;
}

EHALImpactTier FHALCollisionImpactRules::SelectTier(float Speed, EHALImpactTier Cap) const
{
	if (!FMath::IsFinite(Speed) || Speed < LightThreshold || !HALImpact::IsValidTier(Cap)) { return EHALImpactTier::None; }
	const EHALImpactTier Measured = Speed >= HeavyThreshold ? EHALImpactTier::Heavy
		: Speed >= MediumThreshold ? EHALImpactTier::Medium : EHALImpactTier::Light;
	return EHALImpactTier(FMath::Min(uint8(Measured), uint8(Cap)));
}

float HALImpact::PhysicalSupplement(float Desired, float Natural, float RelativeAfter, float InverseRatio, float Restitution)
{
	if (!FMath::IsFinite(Desired) || !FMath::IsFinite(Natural) || !FMath::IsFinite(RelativeAfter)
		|| !FMath::IsFinite(InverseRatio) || !FMath::IsFinite(Restitution)
		|| Desired < 0 || Natural < 0 || InverseRatio < 0) { return 0; }
	const double MassFactor = 1.0 + InverseRatio;
	const double RelativeDelta = double(Natural) * MassFactor;
	const double ClosingBefore = FMath::Max(0.0, RelativeDelta - double(RelativeAfter));
	const double RelativeBudget = FMath::Max(0.0,
		ClosingBefore * (1.0 + FMath::Clamp(double(Restitution), 0.0, 0.5)) - RelativeDelta);
	return float(FMath::Min(FMath::Max(0.0, double(Desired) - Natural), RelativeBudget / MassFactor));
}
