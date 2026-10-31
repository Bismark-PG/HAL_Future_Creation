#include "HALBallPresentationTypes.h"

bool FHALBallPresentationConfig::Validate(FString& Error) const
{
	const auto In=[](float V,float Min,float Max) { return FMath::IsFinite(V) && V>=Min && V<=Max; };
	if (!In(SendRateHz,5,60) || !In(RelativeBlendSeconds,0,.25f) || !In(HandoffSeconds,.01f,.5f)
		|| !In(StaleSeconds,.1f,2) || !In(MaximumExtrapolationSeconds,0,.2f)
		|| MaximumExtrapolationSeconds>StaleSeconds || !In(MaximumRelativeDistance,100,10000))
	{ Error=TEXT("Ball Presentation requires finite bounded rates/times/distances and extrapolation <= stale timeout."); return false; }
	return true;
}

bool FHALBallRelativePose::IsValid(float MaximumDistance) const
{
	return Holder && StateSequence && PhysicsFrame>=0 && FMath::IsFinite(ServerPublishSeconds) && ServerPublishSeconds>=0
		&& !Position.ContainsNaN() && Position.SizeSquared()<=FMath::Square(MaximumDistance)
		&& !Rotation.ContainsNaN() && Rotation.IsNormalized() && !LinearVelocity.ContainsNaN() && !AngularVelocity.ContainsNaN()
		&& LinearVelocity.SizeSquared()<=FMath::Square(100000.) && AngularVelocity.SizeSquared()<=FMath::Square(1000.);
}

bool FHALBallRelativePose::IsNewerThan(const FHALBallRelativePose& Previous) const
{
	return StateSequence==Previous.StateSequence ? PhysicsFrame>Previous.PhysicsFrame
		: int32(StateSequence-Previous.StateSequence)>0;
}

FTransform HALBallPresentation::Compose(const FTransform& HolderPose,const FTransform& RelativePose)
{
	return RelativePose * FTransform(HolderPose.GetRotation(),HolderPose.GetLocation());
}

FTransform HALBallPresentation::Extrapolate(const FHALBallRelativePose& Sample,float Age,float Limit)
{
	const float Time=FMath::IsFinite(Age) ? FMath::Clamp(Age,0.f,Limit) : 0;
	const double Speed=Sample.AngularVelocity.Size();
	const FQuat Advance=Speed>SMALL_NUMBER ? FQuat(Sample.AngularVelocity/Speed,Speed*Time) : FQuat::Identity;
	return FTransform((Advance*Sample.Rotation).GetNormalized(),Sample.Position+Sample.LinearVelocity*Time);
}

FTransform HALBallPresentation::Blend(const FTransform& A,const FTransform& B,float Alpha)
{
	const float T=FMath::Clamp(Alpha,0.f,1.f);
	return FTransform(FQuat::Slerp(A.GetRotation(),B.GetRotation(),T).GetNormalized(),FMath::Lerp(A.GetLocation(),B.GetLocation(),T));
}
