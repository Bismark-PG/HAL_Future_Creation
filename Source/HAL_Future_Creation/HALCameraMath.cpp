#include "HALCameraMath.h"
#include "HALCameraConfig.h"

namespace
{
	double CameraAlpha(double Seconds, double Dt) { return Seconds > UE_SMALL_NUMBER ? 1-FMath::Exp(-Dt/Seconds) : 1; }
	bool CameraPlanePoint(const FMinimalViewInfo& View, const FVector2D& Screen, double Z, FVector& Point)
	{
		const FQuat R = View.Rotation.Quaternion();
		const double Tan = FMath::Tan(FMath::DegreesToRadians(View.FOV*.5));
		const FVector Ray = R.GetForwardVector() + R.GetRightVector()*((Screen.X-.5)*2*Tan)
			- R.GetUpVector()*((Screen.Y-.5)*2*Tan/View.AspectRatio);
		if (Ray.Z >= -.001) { return false; }
		const double T = (Z-View.Location.Z)/Ray.Z;
		if (!FMath::IsFinite(T) || T <= 0) { return false; }
		Point = View.Location + Ray*T; return !Point.ContainsNaN();
	}
}
FMinimalViewInfo HALCamera::View(const UHALCameraConfig& C, const FVector& Focus, float Distance, float Aspect)
{
	const auto& Base = C.BaseCamera;
	const FQuat Boom = Base.BoomTransform.GetRotation();
	FMinimalViewInfo V;
	V.Location = Focus + Base.BoomTransform.GetTranslation() + Base.TargetOffset
		+ Boom.RotateVector(Base.SocketOffset + Base.CameraTransform.GetTranslation() - FVector(Distance,0,0));
	V.Rotation = (Boom*Base.CameraTransform.GetRotation()).Rotator();
	V.FOV = Base.FieldOfView; V.DesiredFOV = V.FOV; V.AspectRatio = Aspect;
	V.bConstrainAspectRatio = Base.bConstrainAspectRatio;
	V.AspectRatioAxisConstraint = AspectRatio_MaintainXFOV;
	V.PostProcessBlendWeight = Base.PostProcessBlendWeight; V.PostProcessSettings = Base.PostProcess;
	return V;
}
bool HALCamera::Project(const FMinimalViewInfo& V, const FVector& Point, FVector2D& Screen)
{
	const FQuat R = V.Rotation.Quaternion(); const FVector Delta = Point-V.Location;
	const double Depth = FVector::DotProduct(Delta,R.GetForwardVector());
	if (!FMath::IsFinite(Depth) || Depth <= 1 || Point.ContainsNaN()) { return false; }
	const double Tan = FMath::Tan(FMath::DegreesToRadians(V.FOV*.5));
	Screen = FVector2D(.5+.5*FVector::DotProduct(Delta,R.GetRightVector())/(Depth*Tan),
		.5-.5*FVector::DotProduct(Delta,R.GetUpVector())/(Depth*Tan/V.AspectRatio));
	return !Screen.ContainsNaN();
}
FVector HALCamera::ClampOwn(const UHALCameraConfig& C, const FVector& Focus, float Distance,
	float Aspect, const FVector& Own, const FVector2D& Size, bool& bLimited)
{
	bLimited = false; const auto V = View(C,Focus,Distance,Aspect);
	FVector2D Screen(.5,.5); const bool bVisible = Project(V,Own,Screen);
	const FVector2D Half = Size*.5-FVector2D(C.OwnScreenPadding);
	const FVector2D Desired(FMath::Clamp(Screen.X,.5-Half.X,.5+Half.X),FMath::Clamp(Screen.Y,.5-Half.Y,.5+Half.Y));
	if (bVisible && Desired.Equals(Screen,1.e-7)) { return Focus; }
	const FQuat R = V.Rotation.Quaternion(); const double Tan = FMath::Tan(FMath::DegreesToRadians(V.FOV*.5));
	const FVector A = R.GetRightVector()-R.GetForwardVector()*((Desired.X-.5)*2*Tan);
	const FVector B = R.GetUpVector()+R.GetForwardVector()*((Desired.Y-.5)*2*Tan/Aspect);
	const double Determinant = A.X*B.Y-A.Y*B.X;
	if (FMath::Abs(Determinant) < 1.e-8) { bLimited = true; return Own; }
	const FVector Rel = Own-V.Location;
	const double AR = FVector::DotProduct(Rel,A), BR = FVector::DotProduct(Rel,B);
	const FVector Delta((AR*B.Y-A.Y*BR)/Determinant,(A.X*BR-AR*B.X)/Determinant,0);
	bLimited = !Delta.IsNearlyZero(); return Focus+Delta;
}
float HALCamera::RequiredDistance(const UHALCameraConfig& C, const FVector& Focus, const FVector& Point, float Aspect)
{
	const auto Zero = View(C,Focus,0,Aspect); const FQuat R = Zero.Rotation.Quaternion();
	const FVector Arm = C.BaseCamera.BoomTransform.GetRotation().GetForwardVector();
	const FVector Rel = Point-Zero.Location;
	const double Forward = FVector::DotProduct(Rel,R.GetForwardVector()), ArmForward = FVector::DotProduct(Arm,R.GetForwardVector());
	double Required = FMath::Max(double(C.MinimumDistance),(2-Forward)/ArmForward);
	const double Tan = FMath::Tan(FMath::DegreesToRadians(Zero.FOV*.5));
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		const FVector Direction = Axis == 0 ? R.GetRightVector() : R.GetUpVector();
		const double Limit = (Axis == 0 ? C.TargetFrameSize.X*Tan : C.TargetFrameSize.Y*Tan/Aspect);
		for (const double Sign : { -1.0, 1.0 })
		{
			const double Numerator = Sign*FVector::DotProduct(Rel,Direction)-Limit*Forward;
			const double Denominator = Limit*ArmForward-Sign*FVector::DotProduct(Arm,Direction);
			if (Denominator > 1.e-8) { Required = FMath::Max(Required,Numerator/Denominator); }
			else if (Numerator > 0) { Required = C.MaximumDistance; }
		}
	}
	return float(FMath::Clamp(Required,double(C.MinimumDistance),double(C.MaximumDistance)));
}
FHALCameraFrame HALCamera::Advance(const UHALCameraConfig& C, const FVector& Own, const TArray<FHALCameraTarget>& Targets,
	const FVector2D& Viewport, float Dt, const FVector2D& Mouse, const FVector2D& Stick, bool bAccept, bool bDynamic, FHALCameraState& S)
{
	FHALCameraFrame F;
	if (Own.ContainsNaN() || Viewport.ContainsNaN() || Viewport.GetMin() < 1 || !FMath::IsFinite(Dt) || Dt < 0) { S = {}; return F; }
	const float Aspect = C.BaseCamera.bConstrainAspectRatio ? C.BaseCamera.AspectRatio : float(Viewport.X/Viewport.Y);
	const bool bReset = !S.bSeeded || Dt > C.MaximumSampleDelta || FVector::DistSquared(Own,S.PreviousOwn) > FMath::Square(C.TeleportResetDistance);
	if (bReset)
	{
		S = {}; S.bSeeded = true; S.AutoFocus = Own; S.Distance = C.BaseCamera.TargetArmLength;
	}
	S.PreviousOwn = Own;
	const double Step = bReset ? 0 : Dt;
	const FVector OldFocus = S.AutoFocus;
	const float StaticResponse = C.BaseCamera.bEnableCameraLag && C.BaseCamera.CameraLagSpeed > 0 ? 1/C.BaseCamera.CameraLagSpeed : 0;
	S.AutoFocus.Z = FMath::Lerp(S.AutoFocus.Z,Own.Z,CameraAlpha(bDynamic ? C.HeightResponseSeconds : StaticResponse,Step));
	const double HeightLag = FMath::Min(double(C.MaximumHeightLag),S.Distance*FMath::Abs(C.BaseCamera.BoomTransform.GetRotation().GetForwardVector().Z)*.5);
	S.AutoFocus.Z = FMath::Clamp(S.AutoFocus.Z,Own.Z-HeightLag,Own.Z+HeightLag);
	F.AutoGoal = Own; double Total = C.OwnWeight; FVector Sum = Own*C.OwnWeight;
	F.Targets.Add({Own,C.OwnWeight,0,TEXT("Own")});
	if (bDynamic)
	{
		for (const auto& Target : Targets)
		{
			if (Target.Position.ContainsNaN() || !FMath::IsFinite(Target.Weight) || Target.Weight <= 0) { continue; }
			Sum += Target.Position*Target.Weight; Total += Target.Weight; F.Targets.Add(Target);
		}
		F.AutoGoal = Sum/Total;
	}
	F.AutoGoal.Z = S.AutoFocus.Z;
	bool bSoftLimited = false;
	F.AutoGoal = ClampOwn(C,F.AutoGoal,S.Distance,Aspect,Own,C.SoftFrameSize,bSoftLimited);
	F.DesiredDistance = C.BaseCamera.TargetArmLength;
	if (bDynamic)
	{
		for (const auto& Target : Targets)
		{
			if (Target.Weight <= 0 || Target.Position.ContainsNaN() || !FMath::IsFinite(Target.Weight)) { continue; }
			FVector Position = Target.Position; Position.Z = FMath::Clamp(Position.Z,Own.Z-C.TargetHeightLimit,Own.Z+C.TargetHeightLimit);
			const float Influence = FMath::Clamp(Target.Weight/C.OwnWeight,0.f,1.f)*FMath::Clamp(Target.ZoomInfluence,0.f,1.f);
			const float Required = RequiredDistance(C,F.AutoGoal,Position,Aspect);
			F.DesiredDistance = FMath::Max(F.DesiredDistance,FMath::Lerp(C.BaseCamera.TargetArmLength,Required,Influence));
		}
		F.DesiredDistance = FMath::Clamp(F.DesiredDistance,C.MinimumDistance,C.MaximumDistance);
		if (FMath::Abs(F.DesiredDistance-S.Distance) > C.DistanceDeadZone)
		{ S.Distance = FMath::Lerp(S.Distance,F.DesiredDistance,float(CameraAlpha(F.DesiredDistance>S.Distance ? C.ZoomOutResponseSeconds : C.ZoomInResponseSeconds,Step))); }
	}
	else { S.Distance = C.BaseCamera.TargetArmLength; }
	S.Distance = FMath::Clamp(S.Distance,C.MinimumDistance,C.MaximumDistance);
	const double FocusAlpha = CameraAlpha(bDynamic ? C.FocusResponseSeconds : StaticResponse,Step);
	S.AutoFocus.X = FMath::Lerp(OldFocus.X,F.AutoGoal.X,FocusAlpha); S.AutoFocus.Y = FMath::Lerp(OldFocus.Y,F.AutoGoal.Y,FocusAlpha);
	S.AutoFocus = ClampOwn(C,S.AutoFocus,S.Distance,Aspect,Own,C.SoftFrameSize,bSoftLimited);
	if (!bAccept || !bDynamic || bReset) { S.ManualOffset = FVector2D::ZeroVector; S.IdleSeconds = 0; S.InputDevice = 0; }
	else
	{
		const bool bMouse = !Mouse.ContainsNaN() && Mouse.GetAbsMax() > C.MouseNoiseThreshold;
		const float StickLength = !Stick.ContainsNaN() ? FMath::Min(float(Stick.Size()),1.f) : 0;
		if (bMouse)
		{
			S.ManualOffset += FVector2D(Mouse.X,-Mouse.Y*(C.bInvertMouseY ? -1 : 1))*C.MouseSensitivity;
			S.IdleSeconds = 0; S.InputDevice = 1;
		}
		else if (StickLength > C.StickDeadZone)
		{
			const float Strength = FMath::Pow((StickLength-C.StickDeadZone)/(1-C.StickDeadZone),C.StickExponent);
			const FVector2D Direction = Stick.GetSafeNormal();
			S.ManualOffset += FVector2D(Direction.X*C.StickSpeed.X,-Direction.Y*C.StickSpeed.Y*(C.bInvertStickY ? -1 : 1))*Strength*Step;
			S.IdleSeconds = 0; S.InputDevice = 2;
		}
		else
		{
			const double PreviousIdle = S.IdleSeconds;
			S.IdleSeconds += Step; F.bReturning = S.IdleSeconds > C.ReturnDelaySeconds;
			const double ReturnStep = FMath::Max(0.0,double(S.IdleSeconds)-C.ReturnDelaySeconds)
				- FMath::Max(0.0,PreviousIdle-C.ReturnDelaySeconds);
			if (F.bReturning) { S.ManualOffset *= 1-CameraAlpha(C.ReturnResponseSeconds,ReturnStep); }
		}
	}
	const FVector2D Requested = S.ManualOffset;
	S.ManualOffset.X = FMath::Clamp(S.ManualOffset.X,-C.MaximumManualOffset.X,C.MaximumManualOffset.X);
	S.ManualOffset.Y = FMath::Clamp(S.ManualOffset.Y,-C.MaximumManualOffset.Y,C.MaximumManualOffset.Y);
	F.bManualLimited = !Requested.Equals(S.ManualOffset,1.e-7);
	const auto AutoView = View(C,S.AutoFocus,S.Distance,Aspect);
	FVector CenterPoint = S.AutoFocus, OffsetPoint; FVector Offset = FVector::ZeroVector;
	if (CameraPlanePoint(AutoView,FVector2D(.5,.5),S.AutoFocus.Z,CenterPoint))
	{
		for (int32 Attempt = 0; Attempt < 12; ++Attempt)
		{
			if (CameraPlanePoint(AutoView,FVector2D(.5,.5)+S.ManualOffset,S.AutoFocus.Z,OffsetPoint)) { Offset = OffsetPoint-CenterPoint; break; }
			S.ManualOffset *= .5; F.bManualLimited = true;
		}
	}
	F.Focus = ClampOwn(C,S.AutoFocus+Offset,S.Distance,Aspect,Own,C.HardFrameSize,F.bHardLimited);
	if (F.bHardLimited)
	{
		FVector2D Actual;
		if (Project(AutoView,CenterPoint+F.Focus-S.AutoFocus,Actual)) { S.ManualOffset = Actual-FVector2D(.5,.5); }
		else { S.ManualOffset = FVector2D::ZeroVector; }
		F.bManualLimited = true;
	}
	F.View = View(C,F.Focus,S.Distance,Aspect);
	F.bValid = Project(F.View,Own,F.OwnScreen);
	F.AutoFocus = S.AutoFocus; F.Distance = S.Distance; F.ManualOffset = S.ManualOffset; F.IdleSeconds = S.IdleSeconds;
	F.InputDevice = S.InputDevice; F.Viewport = Viewport; F.SoftSize = C.SoftFrameSize; F.HardSize = C.HardFrameSize; F.bDynamic = bDynamic;
	return F;
}
