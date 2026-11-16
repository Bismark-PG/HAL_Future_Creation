#include "HALSideShiftMotion.h"
#include "Serialization/Archive.h"

bool FHALSideShiftMotion::IsValidTuning() const
{
	const auto R=[](float V,float Min,float Max) { return FMath::IsFinite(V)&&V>=Min&&V<=Max; };
	return R(Distance,10,2000)&&R(DurationSeconds,.1f,5)&&R(MaximumAcceleration,100,30000)
		&&R(DriftSeconds,.05f,5)&&R(DriftYawRate,30,720)&&R(DriftResponse,1,30)&&R(MaximumYawAcceleration,1,100);
}
bool FHALSideShiftMotion::IsValid(bool bStamped) const
{
	if(!IsValidTuning()) { return false; }
	if(!Activation.IsValid()) { return *this==FHALSideShiftMotion{}; }
	return !Direction.ContainsNaN()&&FMath::IsNearlyEqual(Direction.SizeSquared(),1.,.001)&&FMath::Abs(Direction.Z)<.001
		&&FMath::IsFinite(InitialLateralSpeed)&&FMath::Abs(InitialLateralSpeed)<=50000
		&&(!bStamped||(StartFrame>=0&&EndFrame>=StartFrame&&EndFrame-StartFrame<=600
			&&ShiftFrames>0&&ShiftFrames<=300&&DriftFrames>0&&DriftFrames<=300));
}
bool FHALSideShiftMotion::IsSliding(int32 Frame) const
{ return Activation.IsValid()&&StartFrame>=0&&Frame>=StartFrame&&Frame<EndFrame&&Frame<=StartFrame+ShiftFrames; }
bool FHALSideShiftMotion::IsDrifting(int32 Frame) const
{ return Activation.IsValid()&&StartFrame>=0&&Frame>=StartFrame&&Frame<EndFrame&&Frame<StartFrame+DriftFrames; }
float FHALSideShiftMotion::StepSpeed(int32 Frame,float FixedDelta) const
{
	if(!IsSliding(Frame)||FixedDelta<=0||ShiftFrames<=0) { return 0; }
	const double U0=FMath::Clamp(double(Frame-StartFrame)/ShiftFrames,0.,1.);
	const double U1=FMath::Clamp(double(Frame+1-StartFrame)/ShiftFrames,0.,1.);
	const auto Position=[](double U) { return U*U*(3-2*U); };
	return float(Distance*(Position(U1)-Position(U0))/FixedDelta);
}
float FHALSideShiftMotion::YawAcceleration(float Steering,float CurrentYawRate) const
{
	const float Target=FMath::Clamp(Steering,-1.f,1.f)*FMath::DegreesToRadians(DriftYawRate);
	return FMath::Clamp((Target-CurrentYawRate)*DriftResponse,-MaximumYawAcceleration,MaximumYawAcceleration);
}
void FHALSideShiftMotion::Stamp(const FHALSideShiftMotion* Previous,int32 Frame,float FixedDelta)
{
	if(!Activation.IsValid()) { *this={}; return; }
	if(Previous&&Previous->Activation==Activation&&Previous->StartFrame>=0)
	{
		StartFrame=Previous->StartFrame; ShiftFrames=Previous->ShiftFrames; DriftFrames=Previous->DriftFrames;
		EndFrame=bActive?Previous->EndFrame:FMath::Min(Previous->EndFrame,Frame);
		return;
	}
	StartFrame=Frame;
	ShiftFrames=FMath::Clamp(FMath::CeilToInt(DurationSeconds/FixedDelta),1,300);
	DriftFrames=FMath::Clamp(FMath::CeilToInt(DriftSeconds/FixedDelta),1,300);
	EndFrame=bActive?Frame+FMath::Max(ShiftFrames+1,DriftFrames):Frame;
}
void FHALSideShiftMotion::Serialize(FArchive& Ar)
{
	bool Has=Activation.IsValid(); Ar.SerializeBits(&Has,1);
	if(!Has) { if(Ar.IsLoading()) { *this={}; } return; }
	Ar<<Distance<<DurationSeconds<<MaximumAcceleration<<DriftSeconds<<DriftYawRate<<DriftResponse<<MaximumYawAcceleration;
	Ar<<Activation<<bActive<<Direction<<InitialLateralSpeed<<StartFrame<<EndFrame<<ShiftFrames<<DriftFrames;
}
