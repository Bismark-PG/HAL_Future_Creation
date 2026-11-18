#include "HALArrestChainTypes.h"
#include "Math/RotationMatrix.h"
#include "Algo/BinarySearch.h"

namespace
{
	bool IsArrestChainValueInRange(float X,float Min,float Max) { return FMath::IsFinite(X)&&X>=Min&&X<=Max; }
	float SignedPower(float X,float Power) { return FMath::Sign(X)*FMath::Pow(FMath::Abs(X),Power); }
}
bool FHALArrestChainFX::IsValid() const
{
	for(const auto& C:{DarkColor,MiddleColor,LightColor})
	{ if(!FMath::IsFinite(C.R)||!FMath::IsFinite(C.G)||!FMath::IsFinite(C.B)||!FMath::IsFinite(C.A)) { return false; } }
	TSet<const UVehicleDefinition*> Seen;
	for(const auto& F:VehicleFits)
	{ if(!F.VehicleDefinition||F.CenterOffset.ContainsNaN()||F.CenterOffset.GetAbsMax()>500||F.BoundsScale.ContainsNaN()||F.BoundsScale.GetMin()<.25||F.BoundsScale.GetMax()>2||Seen.Contains(F.VehicleDefinition.Get())) { return false; } Seen.Add(F.VehicleDefinition.Get()); }
	return !PartId.IsNone()&&!BoundsScale.ContainsNaN()&&BoundsScale.GetMin()>=.25f&&BoundsScale.GetMax()<=2
		&&!CenterOffset.ContainsNaN()&&CenterOffset.GetAbsMax()<=500&&!LightDirection.ContainsNaN()&&!LightDirection.IsNearlyZero()
		&&IsArrestChainValueInRange(LowerHeightFraction,0,.4f)&&IsArrestChainValueInRange(UpperHeightFraction,.6f,1.2f)&&IsArrestChainValueInRange(Clearance,0,40)
		&&LinksPerLoop>=16&&LinksPerLoop<=96&&LinksPerLoop%2==0&&IsArrestChainValueInRange(CrossAngle,10,75)&&IsArrestChainValueInRange(ProfileExponent,2,6)
		&&IsArrestChainValueInRange(LinkWidth,2,30)&&IsArrestChainValueInRange(LinkOverlap,1.05f,1.6f)&&IsArrestChainValueInRange(WireScale,.25f,3)&&IsArrestChainValueInRange(WireRadius,.25f,6)&&IsArrestChainValueInRange(CrossingSeparation,2,20)
		&&IsArrestChainValueInRange(TightenSeconds,.05f,1)&&IsArrestChainValueInRange(InitialLooseness,0,.8f)&&IsArrestChainValueInRange(ReboundAmount,0,.1f)&&IsArrestChainValueInRange(ReboundSeconds,.05f,1)
		&&IsArrestChainValueInRange(ReleaseSeconds,.05f,.6f)&&IsArrestChainValueInRange(ReleaseLooseness,0,.8f)&&IsArrestChainValueInRange(FlowSpeed,-200,200)&&IsArrestChainValueInRange(FlowRampSeconds,0,2)&&IsArrestChainValueInRange(ShadowThreshold,-.8f,.3f)
		&&IsArrestChainValueInRange(HighlightThreshold,.1f,.95f)&&HighlightThreshold>ShadowThreshold&&IsArrestChainValueInRange(ToneSoftness,.02f,.4f)&&IsArrestChainValueInRange(GlintStrength,0,2);
}
FHALArrestChainEnvelope HALArrestChains::Envelope(const FHALArrestChainFX& C,bool Arrested,double Now,double Start,double Release)
{
	FHALArrestChainEnvelope E;
	if(!C.bEnabled||!C.IsValid()||!FMath::IsFinite(Now)) { return E; }
	if(Arrested&&Start>=0&&FMath::IsFinite(Start))
	{
		const float Age=float(FMath::Max(0.,Now-Start)); const float T=FMath::Clamp(Age/C.TightenSeconds,0.f,1.f);
		E.Looseness=C.InitialLooseness*(1-FMath::SmoothStep(0.f,1.f,T));
		if(Age>=C.TightenSeconds)
		{ const float R=(Age-C.TightenSeconds)/C.ReboundSeconds; E.Looseness+=C.ReboundAmount*FMath::Sin(R*UE_TWO_PI)*FMath::Exp(-4*R); }
		E.Visibility=FMath::SmoothStep(0.f,C.TightenSeconds*.25f,Age); E.bFinished=false;
	}
	else if(Release>=0&&FMath::IsFinite(Release))
	{
		const float T=float(FMath::Max(0.,Now-Release)/C.ReleaseSeconds);
		E.Looseness=C.ReleaseLooseness*FMath::SmoothStep(0.f,1.f,T);
		E.Visibility=1-FMath::SmoothStep(.15f,1.f,T); E.bFinished=T>=1;
	}
	return E;
}
double HALArrestChains::FlowDistance(const FHALArrestChainFX& C,double Now,double Start,double Release)
{
	if(!C.IsValid()||!FMath::IsFinite(Now)||!FMath::IsFinite(Start)||Start<0||!FMath::IsFinite(Release)) { return 0; }
	const double ActiveNow=Release>=0?FMath::Min(Now,Release):Now;
	const double Age=FMath::Max(0.,ActiveNow-Start-C.TightenSeconds),Ramp=C.FlowRampSeconds;
	double Integrated=Age,SpeedFraction=ActiveNow>=Start+C.TightenSeconds?1:0;
	if(Ramp>0)
	{
		const double X=FMath::Clamp(Age/Ramp,0.,1.);
		Integrated=Age<Ramp?Ramp*(X*X*X-.5*X*X*X*X):Age-.5*Ramp;
		SpeedFraction=X*X*(3-2*X);
	}
	if(Release>=0&&Now>Release)
	{
		const double X=FMath::Clamp((Now-Release)/C.ReleaseSeconds,0.,1.);
		Integrated+=SpeedFraction*C.ReleaseSeconds*(X-X*X*X+.5*X*X*X*X);
	}
	return C.FlowSpeed*Integrated;
}
bool HALArrestChains::BuildPose(const FBox& Bounds,const FHALArrestChainFX& C,const FVector& MeshSize,float Loose,TArray<FTransform>& Out,double Flow)
{
	Out.Reset();
	if(!C.IsValid()||!Bounds.IsValid||Bounds.Min.ContainsNaN()||Bounds.Max.ContainsNaN()||Bounds.GetExtent().GetMin()<=0
		||MeshSize.ContainsNaN()||MeshSize.GetMin()<=KINDA_SMALL_NUMBER||!FMath::IsFinite(Loose)||!FMath::IsFinite(Flow)) { return false; }
	const FVector OriginalCenter=Bounds.GetCenter(),OriginalExtent=Bounds.GetExtent();
	const float Low=Bounds.Min.Z+float(OriginalExtent.Z*2)*C.LowerHeightFraction;
	const float High=Bounds.Min.Z+float(OriginalExtent.Z*2)*C.UpperHeightFraction;
	const FVector Center(OriginalCenter.X+C.CenterOffset.X,OriginalCenter.Y+C.CenterOffset.Y,(Low+High)*.5f+C.CenterOffset.Z);
	const FVector Extent=FVector(OriginalExtent.X,OriginalExtent.Y,(High-Low)*.5f)*C.BoundsScale+FVector(C.Clearance);
	const FVector XYRoom=Extent-FVector(FMath::Abs(C.CenterOffset.X),FMath::Abs(C.CenterOffset.Y),0);
	if(XYRoom.X<5||XYRoom.Y<5) { return false; }
	constexpr int32 Samples=256;
	Out.Reserve(C.LinksPerLoop*2);
	for(int32 Loop=0;Loop<2;++Loop)
	{
		const float Angle=FMath::DegreesToRadians(C.CrossAngle*(Loop==0?1:-1));
		const FVector Direction(FMath::Cos(Angle),FMath::Sin(Angle),0),Normal=FVector::CrossProduct(FVector::UpVector,Direction);
		const float Run=float(FMath::Min(XYRoom.X/FMath::Abs(Direction.X),XYRoom.Y/FMath::Abs(Direction.Y)));
		TArray<FVector,TInlineAllocator<Samples+1>> Points; TArray<float,TInlineAllocator<Samples+1>> Distances;
		for(int32 I=0;I<=Samples;++I)
		{
			const float A=I*UE_TWO_PI/Samples;
			const float U=Run*SignedPower(FMath::Sin(A),2/C.ProfileExponent),Z=float(Extent.Z)*SignedPower(FMath::Cos(A),2/C.ProfileExponent);
			const float RoofWeight=FMath::Pow(FMath::Max(0.f,Z/float(Extent.Z)),4);
			const FVector P=Direction*U+FVector::UpVector*(Z+(Loop==0?-.5f:.5f)*C.CrossingSeparation*RoofWeight);
			Distances.Add(I?Distances.Last()+float(FVector::Distance(Points.Last(),P)):0); Points.Add(P);
		}
		const float Length=Distances.Last(),Pitch=Length/C.LinksPerLoop;
		const double Phase=FMath::Fmod(Flow*(Loop==1&&C.bOppositeLoopFlow?-1:1),double(Length));
		for(int32 I=0;I<C.LinksPerLoop;++I)
		{
			const float Distance=float(FMath::Fmod(I*double(Pitch)+Phase+Length,double(Length)));
			const int32 Segment=FMath::Clamp(Algo::UpperBound(Distances,Distance)-1,0,Samples-1);
			const float Alpha=(Distance-Distances[Segment])/FMath::Max(KINDA_SMALL_NUMBER,Distances[Segment+1]-Distances[Segment]);
			const FVector P=FMath::Lerp(Points[Segment],Points[Segment+1],Alpha);
			const auto At=[&](int32 Index)->const FVector& { return Points[(Index+Samples)%Samples]; };
			const FVector T0=(At(Segment+1)-At(Segment-1)).GetSafeNormal(),T1=(At(Segment+2)-At(Segment)).GetSafeNormal();
			const FVector Tangent=FMath::Lerp(T0,T1,Alpha).GetSafeNormal();
			const FQuat Frame=FRotationMatrix::MakeFromXY(Tangent,Normal).ToQuat();
			const FQuat Roll(FVector::ForwardVector,(I%2)?UE_HALF_PI:0);
			const FVector Scale(Pitch*C.LinkOverlap/MeshSize.X,C.LinkWidth/MeshSize.Y,C.WireScale);
			Out.Emplace(Frame*Roll,Center+P*(1+FMath::Clamp(Loose,-.1f,.8f)),Scale);
		}
	}
	return true;
}
