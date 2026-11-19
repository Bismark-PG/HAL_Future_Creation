#include "HALHookThrowTypes.h"
#include "Math/RotationMatrix.h"

namespace
{
	bool HookFiniteRange(double V,double A,double B) { return FMath::IsFinite(V)&&V>=A&&V<=B; }
}
bool FHALHookRules::IsValid() const
{
	return HookFiniteRange(Range,200,4000)&&HookFiniteRange(OutboundSpeed,200,10000)&&HookFiniteRange(ReturnSpeed,500,15000)
		&&HookFiniteRange(HitRadius,5,80)&&!OutletOffset.ContainsNaN()&&OutletOffset.GetAbsMax()<=1500
		&&HookFiniteRange(ChoiceSeconds,.2,3)&&HookFiniteRange(DirectionThreshold,.1,.9)
		&&HookFiniteRange(SwingSeconds,.1,1.5)&&HookFiniteRange(SwingAngle,1,80)&&HookFiniteRange(SwingArcLength,20,1500)
		&&HookFiniteRange(SwingAccelerationPower,1.2,4)&&HookFiniteRange(SwingMaximumAcceleration,1000,50000)
		&&HookFiniteRange(HorizontalSpeed,100,6000)&&HookFiniteRange(UpwardSpeed,0,1500)
		&&HookFiniteRange(CaptureStiffness,0,300)&&HookFiniteRange(Damping,1,80)&&HookFiniteRange(MaximumAcceleration,100,50000)
		&&HookFiniteRange(MaximumCaptureDrift,50,1500)&&HookFiniteRange(AirborneUpwardSpeed,20,1500)&&MaximumSeconds()<=29;
}
float FHALHookRules::MaximumSeconds() const { return Range/OutboundSpeed+ChoiceSeconds+SwingSeconds+(Range+MaximumCaptureDrift+SwingArcLength)/ReturnSpeed+1; }
bool FHALHookFX::IsValid() const
{
	const auto Color=[](const FLinearColor& C) { return HookFiniteRange(C.R,0,4)&&HookFiniteRange(C.G,0,4)&&HookFiniteRange(C.B,0,4)&&FMath::IsFinite(C.A); };
	return HookFiniteRange(LinkPitch,8,80)&&HookFiniteRange(LinkScale,.3,4)&&HookFiniteRange(HookScale,.3,4)
		&&MaximumLinks>=16&&MaximumLinks<=512&&!HookChainOffset.ContainsNaN()&&HookChainOffset.GetAbsMax()<=300&&HookFiniteRange(Sag,0,80)&&HookFiniteRange(Wobble,0,30)
		&&HookFiniteRange(WobbleFrequency,0,20)&&HookFiniteRange(EnterSeconds,.02,.3)
		&&Color(DarkColor)&&Color(MiddleColor)&&Color(LightColor)&&!LightDirection.ContainsNaN()&&!LightDirection.IsNearlyZero()
		&&HookFiniteRange(ShadowThreshold,-.8,.3)&&HookFiniteRange(HighlightThreshold,.1,.95)&&HighlightThreshold>ShadowThreshold
		&&HookFiniteRange(ToneSoftness,.02,.4)&&HookFiniteRange(GlintStrength,0,2)&&HookFiniteRange(GlintPower,2,80)&&HookFiniteRange(ContourStrength,0,1);
}
FVector HALHook::VisualPoint(const FVector& S,const FVector& E,float T,double Age,const FHALHookFX& FX,bool Taut)
{
	const FVector Side=FVector::CrossProduct(FVector::UpVector,(E-S).GetSafeNormal()).GetSafeNormal();
	const float Envelope=FMath::Sin(PI*T)*(Taut?.15f:1.f);
	return FMath::Lerp(S,E,T)+Envelope*(Side*FX.Wobble*FMath::Sin(float(Age)*FX.WobbleFrequency*UE_TWO_PI+T*PI*3)-FVector::UpVector*FX.Sag);
}
void HALHook::BuildChain(const FVector& S,const FVector& E,double Age,const FHALHookFX& FX,bool Taut,TArray<FTransform>& Out)
{
	Out.Reset();
	if(!FX.IsValid()||S.ContainsNaN()||E.ContainsNaN()||!FMath::IsFinite(Age)) { return; }
	const double Length=FVector::Distance(S,E); if(Length<1) { return; }
	const int32 Count=FMath::Clamp(FMath::CeilToInt(Length/FX.LinkPitch),1,FX.MaximumLinks);
	for(int32 I=0;I<Count;++I)
	{
		const float T=(I+.5f)/Count;
		const FVector P=VisualPoint(S,E,T,Age,FX,Taut);
		const FVector Tangent=(VisualPoint(S,E,FMath::Min(1.f,T+.01f),Age,FX,Taut)-VisualPoint(S,E,FMath::Max(0.f,T-.01f),Age,FX,Taut)).GetSafeNormal();
		const FQuat Rotation=FRotationMatrix::MakeFromX(Tangent).ToQuat()*FQuat(FVector::ForwardVector,I%2?UE_HALF_PI:0);
		Out.Emplace(Rotation,P,FVector(FX.LinkScale));
	}
}
