#include "HALSmokeScreenTypes.h"
#include "HALSmokeScreen.h"
#include "EngineUtils.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/Material.h"

namespace
{
	bool IsSmokeScreenValueInRange(float V,float Min,float Max) { return FMath::IsFinite(V)&&V>=Min&&V<=Max; }
}
bool FHALSmokeScreenRules::IsValid() const
{
	return IsSmokeScreenValueInRange(EmissionSeconds,.1f,15)&&IsSmokeScreenValueInRange(PuffSeconds,.5f,20)&&IsSmokeScreenValueInRange(DissolveSeconds,.1f,5)&&DissolveSeconds<PuffSeconds
		&&IsSmokeScreenValueInRange(InitialRadius,20,500)&&IsSmokeScreenValueInRange(MaximumRadius,InitialRadius,1000)&&IsSmokeScreenValueInRange(ExpansionSeconds,.1f,5)
		&&IsSmokeScreenValueInRange(Height,80,700)&&!OutletOffset.ContainsNaN()&&OutletOffset.GetAbsMax()<=1500
		&&IsSmokeScreenValueInRange(UpdateSeconds,.03f,.15f)&&IsSmokeScreenValueInRange(SampleSpacing,30,300)&&IsSmokeScreenValueInRange(StationaryInterval,.1f,1)
		&&MaximumPuffs>=FMath::CeilToInt(EmissionSeconds/UpdateSeconds)+1&&MaximumPuffs<=128
		&&WallRays>=12&&WallRays<=48&&IsSmokeScreenValueInRange(WallRefreshSeconds,.1f,1);
}
float FHALSmokeScreenRules::Radius(float Age) const
{ return FMath::Lerp(InitialRadius,MaximumRadius,FMath::Clamp(Age/ExpansionSeconds,0.f,1.f)); }
float FHALSmokeScreenRules::Density(float Age) const
{
	if(Age<0||Age>=PuffSeconds) { return 0; }
	return FMath::SmoothStep(0.f,.15f,Age)*(1-FMath::SmoothStep(PuffSeconds-DissolveSeconds,PuffSeconds,Age));
}
bool FHALSmokeScreenFX::IsValid() const
{
	const auto Color=[](FLinearColor C) { return IsSmokeScreenValueInRange(C.R,0,4)&&IsSmokeScreenValueInRange(C.G,0,4)&&IsSmokeScreenValueInRange(C.B,0,4); };
	const auto UI=[](const UMaterialInterface* M) { return !M||M->GetMaterial()->MaterialDomain==MD_UI; };
	return Color(DarkColor)&&Color(LightColor)&&UI(CloudMaterial)&&UI(ScreenMaterial)
		&&IsSmokeScreenValueInRange(CloudOpacity,.1f,1)&&IsSmokeScreenValueInRange(FlowSpeed,.1f,4)&&IsSmokeScreenValueInRange(MaximumObscuration,0,.95f)
		&&IsSmokeScreenValueInRange(FadeInSeconds,.01f,2)&&IsSmokeScreenValueInRange(FadeOutSeconds,.01f,3)&&IsSmokeScreenValueInRange(JetRate,1,100)
		&&IsSmokeScreenValueInRange(UpwardSpeed,20,1000)&&IsSmokeScreenValueInRange(BackwardSpeed,0,500);
}
float FHALSmokePuff::Boundary(float Angle) const
{
	if(WallDistances.IsEmpty()) { return 0; }
	const float A=FMath::Fmod(Angle+2*PI,2*PI)/(2*PI)*WallDistances.Num();
	const int32 I=FMath::Clamp(FMath::FloorToInt(A),0,WallDistances.Num()-1);
	// Conservative between rays; a narrow wall cannot be opened by interpolating the longer ray.
	return FMath::Min(WallDistances[I],WallDistances[(I+1)%WallDistances.Num()]);
}
float FHALSmokePuff::Strength(const FHALSmokeScreenRules& R,float Age,const FVector& Position) const
{
	if(Position.Z<Ground.Z-30||Position.Z>Ground.Z+R.Height) { return 0; }
	const FVector D=Position-FVector(Ground);
	const float Radius=FMath::Min(R.Radius(Age),Boundary(FMath::Atan2(D.Y,D.X)));
	if(Radius<1) { return 0; }
	return R.Density(Age)*(1-FMath::SmoothStep(.8f,1.f,float(D.Size2D()/Radius)));
}
double HALSmoke::ServerTime(const UWorld* W)
{ if(!W) { return 0; } const auto* S=W->GetGameState(); return S?S->GetServerWorldTimeSeconds():W->GetTimeSeconds(); }
FVector HALSmoke::TargetPosition(const AActor& Target)
{ const auto* Body=Cast<UPrimitiveComponent>(Target.GetRootComponent()); return Body?Body->Bounds.Origin:Target.GetActorLocation(); }
bool HALSmoke::IsConcealed(const AActor* Target)
{
	if(!IsValid(Target)||Target->IsActorBeingDestroyed()||!Target->GetWorld()) { return false; }
	for(TActorIterator<AHALSmokeScreen> It(Target->GetWorld());It;++It)
	{ if(!It->IsActorBeingDestroyed()&&It->Influence(*Target,false,ServerTime(Target->GetWorld()))>.15f) { return true; } }
	return false;
}
float HALSmoke::AdvanceObscuration(float Current,float Desired,float Delta,float InSeconds,float OutSeconds,float FullStrength)
{
	if(!FMath::IsFinite(Delta)||Delta<=0) { return Current; }
	return FMath::FInterpConstantTo(Current,Desired,Delta,FMath::Max(.01f,FullStrength)/FMath::Max(.01f,Desired>Current?InSeconds:OutSeconds));
}
