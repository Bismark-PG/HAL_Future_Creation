#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../HALArrestChainTypes.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALArrestChainGeometryTest,"HAL.FutureCreation.Pursuit.Chains.GeometryAndInterlocking",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALArrestChainGeometryTest::RunTest(const FString&)
{
	FHALArrestChainFX C; TestTrue(TEXT("Default chain configuration valid"),C.IsValid()); TArray<FTransform> Pose;
	const FVector MeshSize(23.4f,12.4f,3.4f); const FBox Bounds(FVector(-145,-85,-35),FVector(145,85,65));
	if(!TestTrue(TEXT("Two complete chain poses generated"),HALArrestChains::BuildPose(Bounds,C,MeshSize,0,Pose))) { return false; }
	TestEqual(TEXT("Two closed rings with bounded instance count"),Pose.Num(),C.LinksPerLoop*2);
	const FVector A=Pose[0].GetLocation(),B=Pose[C.LinksPerLoop].GetLocation();
	TestTrue(TEXT("Crossing above the roof has a real vertical separation"),FMath::IsNearlyEqual(float(B.Z-A.Z),C.CrossingSeparation,.01f)&&FVector::Dist2D(A,B)<.01f);
	TestTrue(TEXT("Both crossings stay above the reference roof"),A.Z>Bounds.Max.Z&&B.Z>Bounds.Max.Z);
	const FVector N=FVector::CrossProduct(FVector::UpVector,FVector(FMath::Cos(FMath::DegreesToRadians(C.CrossAngle)),FMath::Sin(FMath::DegreesToRadians(C.CrossAngle)),0));
	TestTrue(TEXT("First flat link follows wrapping plane"),FMath::Abs(FVector::DotProduct(Pose[0].GetRotation().GetAxisY(),N))>.99);
	TestTrue(TEXT("Next link turns ninety degrees for interlocking"),FMath::Abs(FVector::DotProduct(Pose[1].GetRotation().GetAxisY(),N))<.01);
	for(const auto& T:Pose) { if(!T.IsValid()||T.GetScale3D().GetMin()<=0) { AddError(TEXT("Nonfinite or collapsed chain transform")); return false; } }
	for(int32 Loop=0;Loop<2;++Loop)
	{
		float Min=MAX_flt,Max=0;
		for(int32 I=0;I<C.LinksPerLoop;++I)
		{ const float D=float(FVector::Distance(Pose[Loop*C.LinksPerLoop+I].GetLocation(),Pose[Loop*C.LinksPerLoop+(I+1)%C.LinksPerLoop].GetLocation())); Min=FMath::Min(Min,D); Max=FMath::Max(Max,D); }
		TestTrue(TEXT("Arc length placement avoids gaps and bunching at corners"),Min>0&&Max/Min<1.2f);
	}
	C.LinksPerLoop=41; TestFalse(TEXT("Odd closed ring would break alternating links"),C.IsValid());
	C.LinksPerLoop=96; TestTrue(TEXT("Upper instance budget supported"),HALArrestChains::BuildPose(Bounds,C,MeshSize,.28f,Pose)); TestEqual(TEXT("Hard geometry budget"),Pose.Num(),192);
	TestFalse(TEXT("Degenerate bounds rejected without old-marker fallback"),HALArrestChains::BuildPose(FBox(ForceInit),C,MeshSize,0,Pose));
	C.Clearance=std::numeric_limits<float>::quiet_NaN(); TestFalse(TEXT("Nonfinite configuration rejected"),C.IsValid()); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALArrestChainEnvelopeTest,"HAL.FutureCreation.Pursuit.Chains.ServerClockEntryReleaseAndLateJoin",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALArrestChainEnvelopeTest::RunTest(const FString&)
{
	FHALArrestChainFX C;
	const auto Start=HALArrestChains::Envelope(C,true,10,10,-1),Mid=HALArrestChains::Envelope(C,true,10+C.TightenSeconds*.5,10,-1),Tight=HALArrestChains::Envelope(C,true,10+C.TightenSeconds,10,-1);
	TestTrue(TEXT("Starts loose, tightens and remains active"),Start.Looseness>Mid.Looseness&&Mid.Looseness>Tight.Looseness&&!Tight.bFinished);
	TestEqual(TEXT("Entry fades in without a pop"),Start.Visibility,0.f); TestEqual(TEXT("Tight phase visible"),Tight.Visibility,1.f);
	const auto Rebound=HALArrestChains::Envelope(C,true,10+C.TightenSeconds+C.ReboundSeconds*.2,10,-1);
	TestTrue(TEXT("Short damped rebound after tightening"),Rebound.Looseness>0&&Rebound.Looseness<C.ReboundAmount);
	const auto Late=HALArrestChains::Envelope(C,true,12,10,-1); TestTrue(TEXT("Late join sees settled chain rather than replaying entry"),Late.Visibility==1&&FMath::Abs(Late.Looseness)<.001);
	const auto Release=HALArrestChains::Envelope(C,false,13+C.ReleaseSeconds*.5,10,13),Done=HALArrestChains::Envelope(C,false,14,10,13);
	TestTrue(TEXT("Release loosens while fading after gameplay arrest already ended"),Release.Looseness>0&&Release.Visibility>0&&Release.Visibility<1&&!Release.bFinished);
	TestTrue(TEXT("Finished release is fully gone"),Done.bFinished&&Done.Visibility==0);
	TestTrue(TEXT("Late join after release does not replay it"),HALArrestChains::Envelope(C,false,30,10,13).bFinished);
	TestTrue(TEXT("Explicit lifecycle clear skips release"),HALArrestChains::Envelope(C,false,14,-1,-1).bFinished); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALArrestChainFlowTest,"HAL.FutureCreation.Pursuit.Chains.FlowClockDirectionAndIndependentWire",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALArrestChainFlowTest::RunTest(const FString&)
{
	FHALArrestChainFX C; const double Start=10,EndTighten=Start+C.TightenSeconds;
	TestEqual(TEXT("Flow waits until tightening is complete"),HALArrestChains::FlowDistance(C,EndTighten,Start,-1),0.);
	TestTrue(TEXT("Smooth ramp integrates to half full-speed distance"),FMath::IsNearlyEqual(HALArrestChains::FlowDistance(C,EndTighten+C.FlowRampSeconds,Start,-1),double(C.FlowSpeed*C.FlowRampSeconds*.5),.0001));
	const double A=HALArrestChains::FlowDistance(C,EndTighten+1,Start,-1),B=HALArrestChains::FlowDistance(C,EndTighten+2,Start,-1);
	TestTrue(TEXT("Speed uses centimeters per second"),FMath::IsNearlyEqual(B-A,double(C.FlowSpeed),.0001));
	const double Late=HALArrestChains::FlowDistance(C,EndTighten+2,Start,-1);
	TestEqual(TEXT("Late join reconstructs the same phase without accumulated Tick state"),Late,B);
	const double Release=EndTighten+2;
	TestTrue(TEXT("Release begins without a phase jump"),FMath::IsNearlyEqual(HALArrestChains::FlowDistance(C,Release,Start,Release),B,.0001));
	const double Done=HALArrestChains::FlowDistance(C,Release+C.ReleaseSeconds,Start,Release);
	TestTrue(TEXT("Release decelerates to half-speed integrated distance"),FMath::IsNearlyEqual(Done-B,double(C.FlowSpeed*C.ReleaseSeconds*.5),.0001));
	TestEqual(TEXT("Expired release cannot keep circulating"),HALArrestChains::FlowDistance(C,Release+5,Start,Release),Done);
	const FVector Size(23.4,12.4,3.4); const FBox Bounds(FVector(-145,-85,-35),FVector(145,85,65)); TArray<FTransform> Base,Flow,Thick,Reverse,Other;
	HALArrestChains::BuildPose(Bounds,C,Size,0,Base); HALArrestChains::BuildPose(Bounds,C,Size,0,Flow,5);
	TestTrue(TEXT("First loop follows positive path direction"),FVector::DotProduct(Flow[0].GetLocation()-Base[0].GetLocation(),Base[0].GetRotation().GetAxisX())>0);
	TestTrue(TEXT("Opposite second loop follows negative path direction"),FVector::DotProduct(Flow[C.LinksPerLoop].GetLocation()-Base[C.LinksPerLoop].GetLocation(),Base[C.LinksPerLoop].GetRotation().GetAxisX())<0);
	C.bOppositeLoopFlow=false; HALArrestChains::BuildPose(Bounds,C,Size,0,Other,5);
	TestTrue(TEXT("Same-direction option is independent of crossing planes"),FVector::DotProduct(Other[C.LinksPerLoop].GetLocation()-Base[C.LinksPerLoop].GetLocation(),Base[C.LinksPerLoop].GetRotation().GetAxisX())>0);
	C.WireRadius=5; HALArrestChains::BuildPose(Bounds,C,Size,0,Thick,5);
	for(int32 I=0;I<Thick.Num();++I) { TestTrue(TEXT("Wire radius never scales the wrapping path or link centerline"),Thick[I].Equals(Other[I],.0001)); }
	C.bOppositeLoopFlow=true; HALArrestChains::BuildPose(Bounds,C,Size,0,Reverse,-5);
	TestTrue(TEXT("Negative circulation wraps through the closed seam"),FVector::DotProduct(Reverse[0].GetLocation()-Base[0].GetLocation(),Base[0].GetRotation().GetAxisX())<0);
	for(const double Distance:{-1.e9,1.e9})
	{ TestTrue(TEXT("Large absolute times wrap safely within instance budget"),HALArrestChains::BuildPose(Bounds,C,Size,0,Flow,Distance)&&Flow.Num()==128); for(const auto& T:Flow) { if(!T.IsValid()) { AddError(TEXT("Invalid wrapped transform")); return false; } } }
	C.FlowSpeed=0; TestEqual(TEXT("Zero speed disables circulation"),HALArrestChains::FlowDistance(C,100,Start,-1),0.);
	C.FlowSpeed=-45; TestTrue(TEXT("Signed speed reverses circulation"),HALArrestChains::FlowDistance(C,EndTighten+1,Start,-1)<0);
	C.FlowRampSeconds=0; TestTrue(TEXT("Zero ramp is finite and immediate after tighten"),FMath::IsNearlyEqual(HALArrestChains::FlowDistance(C,EndTighten+1,Start,-1),-45.,.0001));
	TestEqual(TEXT("A short arrest released before tightening never starts flow"),HALArrestChains::FlowDistance(C,Start+.2,Start,Start+.1),0.);
	C.WireRadius=0; TestFalse(TEXT("Collapsed wire radius rejected"),C.IsValid()); return true;
}
#endif
