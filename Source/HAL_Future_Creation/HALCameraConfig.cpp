#include "HALCameraConfig.h"
#include "Curves/CurveFloat.h"
#include "Misc/DataValidation.h"

namespace
{
	bool CameraRange(float V, float Min, float Max) { return FMath::IsFinite(V) && V >= Min && V <= Max; }
	bool CameraPair(const FVector2D& V, float Min, float Max) { return CameraRange(V.X,Min,Max) && CameraRange(V.Y,Min,Max); }
}
bool FHALCameraTargetRule::Validate() const
{
	return CameraRange(Weight,0,10) && CameraRange(FullWeightDistance,0,100000)
		&& CameraRange(ZeroWeightDistance,FullWeightDistance+1,100000) && CameraRange(ZoomInfluence,0,1);
}
float FHALCameraTargetRule::Evaluate(float Distance) const
{
	if (!FMath::IsFinite(Distance) || Distance >= ZeroWeightDistance) { return 0; }
	if (Distance <= FullWeightDistance) { return Weight; }
	const float T = FMath::Clamp((Distance-FullWeightDistance)/(ZeroWeightDistance-FullWeightDistance),0.f,1.f);
	const float Fraction = DistanceCurve ? DistanceCurve->GetFloatValue(T) : 1-T*T*(3-2*T);
	return FMath::IsFinite(Fraction) ? Weight*FMath::Clamp(Fraction,0.f,1.f) : 0;
}
UHALCameraConfig::UHALCameraConfig()
{
	MainBall.Weight = 3; MainBall.FullWeightDistance = 1800; MainBall.ZeroWeightDistance = 6000;
}
bool UHALCameraConfig::Validate(FString& Error) const
{
	auto Fail = [&Error](const TCHAR* Text) { Error = Text; return false; };
	if (!bReadyForUse) { return Fail(TEXT("Camera profile is not ReadyForUse; copy/compare the saved legacy camera first.")); }
	if (!BaseCamera.Validate(Error)) { return false; }
	const FQuat Boom = BaseCamera.BoomTransform.GetRotation(), View = Boom*BaseCamera.CameraTransform.GetRotation();
	if (!BaseCamera.bAbsoluteRotation || BaseCamera.bAbsoluteLocation || BaseCamera.bUsePawnControlRotation || BaseCamera.bDoCollisionTest
		|| !BaseCamera.BoomTransform.GetScale3D().Equals(FVector::OneVector)
		|| !BaseCamera.CameraTransform.GetScale3D().Equals(FVector::OneVector)
		|| Boom.GetForwardVector().Z > -.2 || View.GetForwardVector().Z > -.2
		|| FVector::DotProduct(Boom.GetForwardVector(),View.GetForwardVector()) < .8)
	{ return Fail(TEXT("Camera 0.1 requires a fixed downward camera, unit scales, no control rotation or spring-arm collision.")); }
	if (!CameraRange(BaseCamera.FieldOfView,5,150) || !CameraRange(MinimumDistance,100,20000)
		|| !CameraRange(MaximumDistance,MinimumDistance,20000) || !CameraRange(BaseCamera.TargetArmLength,MinimumDistance,MaximumDistance)
		|| !CameraRange(OwnWeight,.1f,10) || !MainBall.Validate() || !OtherPlayers.Validate()
		|| !CameraPair(TargetFrameSize,.1f,.95f) || !CameraPair(SoftFrameSize,.1f,.9f) || !CameraPair(HardFrameSize,.1f,.95f)
		|| SoftFrameSize.X > HardFrameSize.X || SoftFrameSize.Y > HardFrameSize.Y
		|| !CameraRange(OwnScreenPadding,0,.1f) || SoftFrameSize.GetMin() <= 2*OwnScreenPadding
		|| !CameraRange(WeightResponseSeconds,.01f,2) || !CameraRange(TargetHeightLimit,0,10000)
		|| !CameraRange(FocusResponseSeconds,0,3) || !CameraRange(ZoomOutResponseSeconds,0,3) || !CameraRange(ZoomInResponseSeconds,0,3)
		|| !CameraRange(DistanceDeadZone,0,300) || !CameraRange(HeightResponseSeconds,0,3) || !CameraRange(MaximumHeightLag,0,2000)
		|| !CameraRange(MouseSensitivity,.00001f,.02f) || !CameraRange(MouseNoiseThreshold,0,10)
		|| !CameraRange(StickDeadZone,0,.9f) || !CameraRange(StickExponent,.1f,4) || !CameraPair(StickSpeed,.01f,2)
		|| !CameraPair(MaximumManualOffset,0,.45f) || !CameraRange(ReturnDelaySeconds,0,5) || !CameraRange(ReturnResponseSeconds,0,3)
		|| !CameraRange(MaximumSampleDelta,.05f,1) || !CameraRange(TeleportResetDistance,100,10000))
	{ return Fail(TEXT("Camera profile contains invalid finite ranges, distance ordering or screen-frame nesting.")); }
	return true;
}
void UHALCameraConfig::CopyLegacyCamera()
{
#if WITH_EDITOR
	if (!LegacySource) { return; }
	Modify(); BaseCamera = LegacySource->Camera;
	MinimumDistance = FMath::Min(MinimumDistance,BaseCamera.TargetArmLength);
	MaximumDistance = FMath::Max(MaximumDistance,BaseCamera.TargetArmLength);
	bReadyForUse = true; MarkPackageDirty();
#endif
}
#if WITH_EDITOR
EDataValidationResult UHALCameraConfig::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context); FString Error;
	if (!Validate(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
	return EDataValidationResult::Valid;
}
#endif
