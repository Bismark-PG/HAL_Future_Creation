#include "HALSideShiftMotion.h"
#include "Serialization/Archive.h"

bool FHALSideShiftMotion::IsValidTuning() const
{
	const auto R=[](float V,float Min,float Max) { return FMath::IsFinite(V)&&V>=Min&&V<=Max; };
	return R(Distance,10,2000)&&R(DurationSeconds,.1f,5)&&R(MaximumAcceleration,100,100000)
		&&R(DriftLateralSpan,10,2000)&&R(DriftArcBend,0,2)
		&&R(DriftSeconds,.05f,5)&&R(DriftTurnDegrees,1,150)&&R(DriftInputThreshold,.05f,1)
		&&R(DriftYawRate,30,1440)&&R(DriftResponse,1,100)&&R(MaximumYawAcceleration,1,1000);
}
bool FHALSideShiftMotion::IsValid(bool bStamped) const
{
	if(!IsValidTuning()) { return false; }
	if(!Activation.IsValid()) { return *this==FHALSideShiftMotion{}; }
	return !Direction.ContainsNaN()&&FMath::IsNearlyEqual(Direction.SizeSquared(),1.,.001)&&FMath::Abs(Direction.Z)<.001
		&&!InitialForward.ContainsNaN()&&FMath::IsNearlyEqual(InitialForward.SizeSquared(),1.,.001)&&FMath::Abs(InitialForward.Z)<.001
		&&FMath::Abs(FVector::DotProduct(InitialForward,Direction))<.001&&!Origin.ContainsNaN()&&!InitialVelocity.ContainsNaN()&&InitialVelocity.Size()<50000
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
FVector FHALSideShiftMotion::ArcOffset(float Progress,float Span) const
{
	const float Angle=FMath::Clamp(Progress,0.f,1.f)*HALF_PI;
	return InitialForward*(Span*DriftArcBend*FMath::Sin(Angle))+Direction*(Span*(1-FMath::Cos(Angle)));
}
float FHALSideShiftMotion::ArcHeadingProgress(float Progress) const
{
	const float P=FMath::Clamp(Progress,0.f,1.f);
	if(DriftArcBend<=KINDA_SMALL_NUMBER) { return P; } // Flat side-step has no curved tangent.
	if(P<=0||P>=1) { return P; }
	return FMath::Atan2(FMath::Sin(P*HALF_PI),DriftArcBend*FMath::Cos(P*HALF_PI))/HALF_PI;
}
float FHALSideShiftMotion::HeadingAcceleration(float ErrorRadians,float CurrentYawRate,float FixedDelta,float ReferenceRate,float ReferenceAcceleration) const
{
	if(FixedDelta<=0) { return 0; }
	// Implicit critically damped PD: stable at fixed-step rates, finite torque and
	// an angular-speed cap. It tracks an angle instead of accumulating steering speed.
	const float W=DriftResponse, K=W*W, D=2*W;
	const float A=ReferenceAcceleration+(K*ErrorRadians-(D+K*FixedDelta)*(CurrentYawRate-ReferenceRate))/(1+D*FixedDelta+K*FixedDelta*FixedDelta);
	const float Limit=FMath::DegreesToRadians(DriftYawRate);
	const float TargetRate=FMath::Clamp(CurrentYawRate+A*FixedDelta,-Limit,Limit);
	return FMath::Clamp((TargetRate-CurrentYawRate)/FixedDelta,-MaximumYawAcceleration,MaximumYawAcceleration);
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
	Ar<<Distance<<DurationSeconds<<MaximumAcceleration<<DriftLateralSpan<<DriftArcBend<<DriftSeconds<<DriftTurnDegrees<<DriftInputThreshold<<DriftYawRate<<DriftResponse<<MaximumYawAcceleration;
	Ar<<Activation<<bActive<<bDriftAtStart<<bDriftReleased<<Direction<<InitialForward<<Origin<<InitialVelocity<<InitialLateralSpeed<<StartFrame<<EndFrame<<ShiftFrames<<DriftFrames;
}

void FHALSideShiftTurnState::TrackGameplayImpulse(const FHALSideShiftMotion& M,int32 Frame,const FVector& Impulse)
{
	if(Activation!=M.Activation) { *this={}; Activation=M.Activation; ArcBaseVelocity=FVector(M.InitialVelocity.X,M.InitialVelocity.Y,0); }
	if(M.IsSliding(Frame)||M.IsDrifting(Frame)) { ArcBaseVelocity+=FVector(Impulse.X,Impulse.Y,0); }
}
bool FHALSideShiftTurnState::Update(const FHALSideShiftMotion& M,int32 Frame,float Yaw,float Rate,float Steering,bool Held,float Dt,float& A,const FVector& Position,const FVector& GameplayImpulse)
{
	A=0;
	TrackGameplayImpulse(M,Frame,GameplayImpulse);
	if(!Activation.IsValid()||Dt<=0) { return false; }
	const bool Intent=M.bDriftAtStart?!M.bDriftReleased:Held;
	const bool HasDirection=M.bDriftAtStart||FMath::Abs(Steering)>=M.DriftInputThreshold;
	if(!bConsumed&&M.IsDrifting(Frame)&&M.IsSliding(Frame)&&Frame<M.StartFrame+M.ShiftFrames&&Intent&&HasDirection)
	{
		bConsumed=bHasTarget=true; OriginYaw=M.InitialForward.Rotation().Yaw;
		const float Side=FMath::Sign(FVector::CrossProduct(M.InitialForward,M.Direction).Z);
		// Heading and travel share a forward-entry, same-side turn; reversing only
		// chassis yaw against the old side-entry arc makes wheel paths fold back.
		TargetYaw=FMath::UnwindDegrees(OriginYaw+Side*M.DriftTurnDegrees);
		ArcStartFrame=Frame; ArcFrames=FMath::Max(1,M.StartFrame+M.ShiftFrames-Frame); ArcOrigin=Position;
		const FVector PriorTravel=Position-M.Origin-M.InitialVelocity*((Frame-M.StartFrame)*Dt);
		ArcSpan=FMath::Clamp(M.DriftLateralSpan-float(FVector::DotProduct(PriorTravel,M.Direction)),0.f,M.DriftLateralSpan);
	}
	if(!bHasTarget) { return false; }
	if(!Intent&&!bReleased) { ReleaseProgress=ArcProgress(Frame); bReleased=true; }
	if(bReleased)
	{
		// Cancel on release and finish finite braking; never re-arm this activation.
		if(FMath::Abs(Rate)<.001f) { bHasTarget=false; return false; }
		A=FMath::Clamp(-Rate/Dt,-M.MaximumYawAcceleration,M.MaximumYawAcceleration); return true;
	}
	const float Sweep=FMath::FindDeltaAngleDegrees(OriginYaw,TargetYaw);
	const float P=M.ArcHeadingProgress(ArcProgress(Frame));
	const float Reference=FMath::UnwindDegrees(OriginYaw+Sweep*P);
	const float PreviousRate=FMath::DegreesToRadians(Sweep*(P-M.ArcHeadingProgress(ArcProgress(Frame-1)))/Dt);
	const float NextRate=FMath::DegreesToRadians(Sweep*(M.ArcHeadingProgress(ArcProgress(Frame+1))-P)/Dt);
	A=M.HeadingAcceleration(FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(Yaw,Reference)),Rate,Dt,PreviousRate,(NextRate-PreviousRate)/Dt);
	return true; // Holding the same key holds this heading; it cannot buy another turn.
}
bool FHALSideShiftTurnState::IsValid() const
{
	return FMath::IsFinite(OriginYaw)&&FMath::IsFinite(TargetYaw)&&FMath::Abs(OriginYaw)<=180&&FMath::Abs(TargetYaw)<=180
		&&FMath::IsFinite(ReleaseProgress)&&ReleaseProgress>=0&&ReleaseProgress<=1
		&&FMath::IsFinite(ArcSpan)&&ArcSpan>=0&&ArcSpan<=2000&&!ArcOrigin.ContainsNaN()&&!ArcBaseVelocity.ContainsNaN()&&FMath::Abs(ArcBaseVelocity.Z)<.001
		&&(ArcFrames==0?ArcStartFrame==INDEX_NONE:(ArcFrames>0&&ArcFrames<=300&&ArcStartFrame>=0))
		&&(Activation.IsValid()?((!bHasTarget&&!bReleased)||bConsumed):(!bConsumed&&!bHasTarget&&!bReleased&&OriginYaw==0&&TargetYaw==0));
}
bool FHALSideShiftTurnState::Equals(const FHALSideShiftTurnState& B) const
{
	return Activation==B.Activation&&bConsumed==B.bConsumed&&bHasTarget==B.bHasTarget&&bReleased==B.bReleased
		&&FMath::IsNearlyEqual(ReleaseProgress,B.ReleaseProgress,.0001f)
		&&ArcStartFrame==B.ArcStartFrame&&ArcFrames==B.ArcFrames&&FMath::IsNearlyEqual(ArcSpan,B.ArcSpan,.05f)&&ArcOrigin.Equals(B.ArcOrigin,.05)&&ArcBaseVelocity.Equals(B.ArcBaseVelocity,.05)
		&&FMath::Abs(FMath::FindDeltaAngleDegrees(OriginYaw,B.OriginYaw))<.05f&&FMath::Abs(FMath::FindDeltaAngleDegrees(TargetYaw,B.TargetYaw))<.05f;
}
void FHALSideShiftTurnState::Serialize(FArchive& Ar)
{
	Ar<<Activation<<OriginYaw<<TargetYaw<<bConsumed<<bHasTarget<<bReleased;
	Ar<<ArcStartFrame<<ArcFrames<<ArcSpan<<ArcOrigin<<ArcBaseVelocity<<ReleaseProgress;
	if(!IsValid()) { Ar.SetError(); }
}
float FHALSideShiftTurnState::ArcProgress(int32 Frame) const
{
	if(ArcFrames<=0) { return 0; }
	const float U=FMath::Clamp(float(Frame-ArcStartFrame)/ArcFrames,0.f,1.f); return U*U*(3-2*U);
}
bool FHALSideShiftTurnState::IsArcDriving(const FHALSideShiftMotion& M,int32 Frame) const
{ return Activation==M.Activation&&ArcFrames>0&&Frame>=ArcStartFrame&&Frame<=ArcStartFrame+ArcFrames&&Frame<M.EndFrame; }
FVector FHALSideShiftTurnState::ArcStepVelocity(const FHALSideShiftMotion& M,int32 Frame,float Dt) const
{ return bReleased||Dt<=0?FVector::ZeroVector:(M.ArcOffset(ArcProgress(Frame+1),ArcSpan)-M.ArcOffset(ArcProgress(Frame),ArcSpan))/Dt; }
FVector FHALSideShiftTurnState::ArcBackgroundVelocity(const FHALSideShiftMotion& M,int32 Frame) const
{
	const float P=bReleased?M.ArcHeadingProgress(ReleaseProgress)
		:.5f*(M.ArcHeadingProgress(ArcProgress(Frame))+M.ArcHeadingProgress(ArcProgress(Frame+1)));
	const float Side=FMath::Sign(FVector::CrossProduct(M.InitialForward,M.Direction).Z);
	const FVector Initial(M.InitialVelocity.X,M.InitialVelocity.Y,0);
	return Initial.RotateAngleAxis(Side*90*P,FVector::UpVector)+(ArcBaseVelocity-Initial);
}
