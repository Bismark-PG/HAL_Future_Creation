#include "HALDamageFeedbackConfig.h"
#include "Materials/Material.h"
#include "Misc/DataValidation.h"

namespace
{
	// Anonymous namespaces are shared by .cpp files combined into one UBT unity unit.
	bool DamageFeedbackInRange(float V,float A,float B) { return FMath::IsFinite(V)&&V>=A&&V<=B; }
	bool DamageFeedbackColorValid(FLinearColor C) { return FMath::IsFinite(C.R)&&FMath::IsFinite(C.G)&&FMath::IsFinite(C.B)&&FMath::IsFinite(C.A)&&C.GetMin()>=0&&C.GetMax()<=8; }
	float DamageFeedbackFade(double Age,float Duration,float Hold=0)
	{ return Age<0?0:1-FMath::SmoothStep(Hold,Duration,float(Age)); }
}
bool FHALDamageFeedbackStyle::IsValid() const
{
	return DamageFeedbackColorValid(BodyColor)&&DamageFeedbackColorValid(EdgeColor)&&DamageFeedbackInRange(BodyOpacity,0,1)&&DamageFeedbackInRange(WhiteOpacity,0,1)&&DamageFeedbackInRange(RGBOpacity,0,1)
		&&DamageFeedbackInRange(BodySeconds,.03f,1)&&DamageFeedbackInRange(BodyHoldSeconds,0,.2f)&&BodyHoldSeconds<BodySeconds
		&&DamageFeedbackInRange(WhiteSeconds,.01f,.3f)&&DamageFeedbackInRange(RGBSeconds,.01f,.3f)&&DamageFeedbackInRange(OutlinePixels,0,8)&&DamageFeedbackInRange(OutlineSoftness,.1f,2)
		&&DamageFeedbackInRange(SeparationPixels,0,12)&&!SeparationDirection.ContainsNaN()&&!SeparationDirection.IsNearlyZero()&&SeparationDirection.GetAbsMax()<=100
		&&DamageFeedbackInRange(MinimumAccentInterval,.05f,1)&&MinimumAccentInterval>=FMath::Max(WhiteSeconds,RGBSeconds)
		&&DamageFeedbackInRange(MaximumContinuousSeconds,.1f,2)&&MaximumContinuousSeconds>=FMath::Max3(BodySeconds,WhiteSeconds,RGBSeconds)
		&&DamageFeedbackInRange(RestSeconds,.05f,1)&&DamageFeedbackInRange(MaximumEventAge,.1f,2)&&DamageFeedbackInRange(DepthTolerance,0,10)&&MaximumTargets>=1&&MaximumTargets<=8&&FirstStencil>=1&&FirstStencil+MaximumTargets<=256;
}
bool UHALDamageFeedbackConfig::Validate(FString& Error) const
{
	Error.Reset();
	if(!Style.IsValid()) { Error=TEXT("Invalid damage feedback ranges or timing/stencil constraints."); return false; }
	if(Style.bEnabled&&(!Material||Material->GetMaterial()->MaterialDomain!=MD_PostProcess))
	{ Error=TEXT("Enabled damage feedback requires a saved Post Process material."); return false; }
	return true;
}
#if WITH_EDITOR
EDataValidationResult UHALDamageFeedbackConfig::IsDataValid(FDataValidationContext& Context) const
{ FString Error; if(Validate(Error)) { return EDataValidationResult::Valid; } Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
#endif
bool FHALDamageFeedbackPlayback::Receive(const FHALDamageFeedbackStyle& S,double Time)
{
	if(!S.bEnabled||!S.IsValid()||!FMath::IsFinite(Time)||Time<0||Time<LastBody) { return false; }
	if(BurstStart>=0&&Time>=BurstStart+S.MaximumContinuousSeconds&&Time<BurstStart+S.MaximumContinuousSeconds+S.RestSeconds) { return false; }
	if(BurstStart<0||Time-LastBody>S.BodySeconds||Time>=BurstStart+S.MaximumContinuousSeconds+S.RestSeconds)
	{ BurstStart=Time; }
	LastBody=Time;
	if(LastAccent<0||Time-LastAccent>=S.MinimumAccentInterval) { LastAccent=Time; }
	return true;
}
FVector FHALDamageFeedbackPlayback::Weights(const FHALDamageFeedbackStyle& S,double Now) const
{
	if(BurstStart<0||!FMath::IsFinite(Now)||!S.IsValid()||!S.bEnabled) { return FVector::ZeroVector; }
	const float Limit=FMath::Clamp(float((BurstStart+S.MaximumContinuousSeconds-Now)/FMath::Min(.04f,S.BodySeconds)),0.f,1.f);
	return FVector(DamageFeedbackFade(Now-LastBody,S.BodySeconds,S.BodyHoldSeconds)*Limit,DamageFeedbackFade(Now-LastAccent,S.WhiteSeconds)*Limit,DamageFeedbackFade(Now-LastAccent,S.RGBSeconds)*Limit);
}
double FHALDamageFeedbackPlayback::RetireAt(const FHALDamageFeedbackStyle& S) const
{ return BurstStart<0?0:FMath::Max(BurstStart+S.MaximumContinuousSeconds+S.RestSeconds,LastAccent+S.MinimumAccentInterval); }
