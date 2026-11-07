#include "HALShockVisualSettings.h"

FHALShockVisualSettings::FHALShockVisualSettings()
{
	if (FRichCurve* Curve = BrightnessCurve.GetRichCurve())
	{
		Curve->AddKey(0.f, 1.6f); Curve->AddKey(0.4f, 1.f); Curve->AddKey(1.f, 0.55f); Curve->AddKey(1.6f, 0.4f);
	}
	if (FRichCurve* Curve = FilamentPowerCurve.GetRichCurve())
	{
		Curve->AddKey(0.f, 34.f); Curve->AddKey(0.4f, 48.f); Curve->AddKey(1.6f, 70.f);
	}
}

bool FHALShockVisualSettings::Validate(FString& Error) const
{
	const auto InRange = [](float Value, float Min, float Max)
	{ return FMath::IsFinite(Value) && Value >= Min && Value <= Max; };
	const auto IsFiniteColor = [](FLinearColor Color)
	{ return FMath::IsFinite(Color.R) && FMath::IsFinite(Color.G) && FMath::IsFinite(Color.B) && FMath::IsFinite(Color.A); };
	if (ArcOffsetCm.ContainsNaN() || LightOffsetCm.ContainsNaN() || !IsFiniteColor(ArcColor) || !IsFiniteColor(LightColor)
		|| !InRange(ArcScale, 0.01f, 20.f) || !InRange(FlashSeconds, 0, 0.5f)
		|| !InRange(WipeSeconds, 0, 1) || !InRange(WipeRadiusCm, 10, 2000)
		|| !InRange(BaseBrightness, 0, 60) || !InRange(FadeFilamentGain, 0, 120)
		|| !InRange(FlickerSteps, 1, 60) || !InRange(ScorchRiseSeconds, 0, 3)
		|| !InRange(ArcRetractFeather, 0.01f, 1) || !InRange(LightPeakIntensity, 0, 200000)
		|| !InRange(LightRadiusCm, 50, 5000) || !InRange(LightFlickerFloor, 0, 1) || !InRange(LightFlashBoost, 0, 10)
		|| ArcSettings.ArcCount < 1 || ArcSettings.ArcCount > 8 || ArcSettings.Subdivisions < 2 || ArcSettings.Subdivisions > 5
		|| !InRange(ArcSettings.Roughness, 0, 1) || !InRange(ArcSettings.LiftFraction, 0, 0.5f)
		|| !InRange(ArcSettings.BranchChance, 0, 1) || !InRange(ArcSettings.BranchLength, 0.05f, 0.8f)
		|| !InRange(ArcSettings.WidthCm, 0.1f, 30))
	{ Error = TEXT("Shared Shock Look parameters must be finite and within the editor ranges."); return false; }
	for (const FRuntimeFloatCurve* Curve : { &BrightnessCurve, &FilamentPowerCurve })
	{
		const auto* RichCurve = Curve->GetRichCurveConst();
		if (!RichCurve || RichCurve->GetNumKeys() == 0)
		{ Error = TEXT("Shared Shock Look needs brightness and filament curve keys."); return false; }
		for (const auto& Key : RichCurve->GetConstRefOfKeys())
		{
			if (!FMath::IsFinite(Key.Time) || Key.Time < 0 || !FMath::IsFinite(Key.Value)
				|| !FMath::IsFinite(Key.ArriveTangent) || !FMath::IsFinite(Key.LeaveTangent)
				|| !FMath::IsFinite(Key.ArriveTangentWeight) || !FMath::IsFinite(Key.LeaveTangentWeight))
			{ Error = TEXT("Shared Shock Look curve keys/tangents must be finite; times are nonnegative seconds."); return false; }
		}
	}
	return true;
}
