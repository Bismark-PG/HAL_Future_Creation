#include "HALHookMotion.h"

bool FHALHookMotion::IsValid(bool Stamped) const
{
	if (!Activation.IsValid()) { return *this == FHALHookMotion{}; }
	return !Anchor.ContainsNaN() && !ThrowVelocity.ContainsNaN() && ThrowVelocity.Size() <= 15000
		&& FMath::IsFinite(Stiffness) && Stiffness >= 0 && Stiffness <= 300
		&& FMath::IsFinite(Damping) && Damping >= 0 && Damping <= 80
		&& FMath::IsFinite(MaximumAcceleration) && MaximumAcceleration >= 100 && MaximumAcceleration <= 50000
		&& FMath::IsFinite(DurationSeconds) && DurationSeconds >= .05f && DurationSeconds <= 30
		&& !(bSwing&&bThrow) && (!bSwing || (bCapture&&!Pivot.ContainsNaN()&&(Anchor-Pivot).Size2D()>=1&&(Anchor-Pivot).Size2D()<=10000
			&& FMath::IsFinite(SwingRadians)&&FMath::Abs(SwingRadians)>.00001f&&FMath::Abs(SwingRadians)<=1.4f
			&& FMath::IsFinite(AccelerationPower)&&AccelerationPower>=1.2f&&AccelerationPower<=4
			&& FMath::IsFinite(FixedDelta)&&FixedDelta>0&&FixedDelta<=1))
		&& (!Stamped || (StartFrame >= 0 && EndFrame > StartFrame));
}
void FHALHookMotion::Stamp(const FHALHookMotion* Previous, int32 Frame, float Delta)
{
	if (!Activation.IsValid()) { return; }
	const bool Same = Previous && Previous->Activation == Activation && Previous->bThrow == bThrow && Previous->bSwing == bSwing;
	// Immediate network actions can arrive after this frame's movement callback.
	// Author the one-shot for the next absolute frame so both callback orderings,
	// and later replay, execute it exactly once instead of missing the start.
	StartFrame = Same ? Previous->StartFrame : Frame+((bThrow||bSwing)?1:0);
	if(bSwing) { FixedDelta=Same?Previous->FixedDelta:Delta; }
	EndFrame = StartFrame + FMath::Max(1, FMath::CeilToInt(DurationSeconds / FMath::Max(.001f, Delta)));
}
void FHALHookMotion::Serialize(FArchive& Ar)
{
	// Snapshots recur inside physics history. An unused skill must cost only one bit,
	// and each active mode sends only the values it actually consumes.
	bool Active=Activation.IsValid(); Ar.SerializeBits(&Active,1);
	if(!Active) { if(Ar.IsLoading()) { *this={}; } return; }
	if(Ar.IsLoading()) { *this={}; }
	Ar << Activation << DurationSeconds << StartFrame << EndFrame;
	Ar.SerializeBits(&bCapture,1); Ar.SerializeBits(&bThrow,1); Ar.SerializeBits(&bSwing,1);
	if(bThrow) { Ar << ThrowVelocity; Ar.SerializeBits(&bPreserveHorizontal,1); }
	else
	{
		Ar << Stiffness << Damping << MaximumAcceleration;
		if(bCapture) { Ar << Anchor; }
		if(bSwing) { Ar << Pivot << SwingRadians << AccelerationPower << FixedDelta; }
	}
	if(!Activation.IsValid()) { Ar.SetError(); }
}
bool FHALHookMotion::IsHolding(int32 Frame) const
{ return Activation.IsValid() && !bThrow && !bSwing && Frame >= StartFrame && Frame < EndFrame; }
FVector FHALHookMotion::Acceleration(const FVector& P, const FVector& V, int32 Frame) const
{
	if (!IsHolding(Frame)) { return FVector::ZeroVector; }
	FVector A = -FVector(V.X,V.Y,0) * Damping;
	if (bCapture) { A += FVector(Anchor.X-P.X,Anchor.Y-P.Y,0) * Stiffness; }
	return A.GetClampedToMaxSize(MaximumAcceleration);
}
FVector FHALHookMotion::ReleaseDelta(const FVector& V, int32 Frame) const
{
	if (!Activation.IsValid() || !bThrow || Frame != StartFrame) { return FVector::ZeroVector; }
	return FVector(bPreserveHorizontal?0:ThrowVelocity.X-V.X,bPreserveHorizontal?0:ThrowVelocity.Y-V.Y,FMath::Max(0.,ThrowVelocity.Z-V.Z));
}
bool FHALHookMotion::IsSwinging(int32 Frame) const
{ return Activation.IsValid()&&bSwing&&Frame>=StartFrame&&Frame<EndFrame; }
FVector FHALHookMotion::SwingPosition(int32 Frame) const
{
	const double U=FMath::Clamp(double(int64(Frame)-StartFrame)/FMath::Max(1,EndFrame-StartFrame),0.,1.);
	const double A=SwingRadians*FMath::Pow(U,double(AccelerationPower)),C=FMath::Cos(A),S=FMath::Sin(A);
	const FVector R=Anchor-Pivot;
	return FVector(Pivot.X+C*R.X-S*R.Y,Pivot.Y+S*R.X+C*R.Y,Anchor.Z);
}
FVector FHALHookMotion::SwingDelta(const FVector& P,const FVector& V,int32 Frame,float Delta) const
{
	if(!IsSwinging(Frame)||Delta<=0) { return FVector::ZeroVector; }
	const FVector Next=SwingPosition(Frame+1);
	const FVector Desired((Next.X-P.X)/Delta,(Next.Y-P.Y)/Delta,0);
	return (Desired-FVector(V.X,V.Y,0)).GetClampedToMaxSize(MaximumAcceleration*Delta);
}
