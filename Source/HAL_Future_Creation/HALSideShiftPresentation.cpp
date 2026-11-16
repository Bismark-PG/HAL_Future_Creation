#include "HALSideShiftComponent.h"
#include "HALVehiclePresentationComponent.h"
#include "HALVehicleStatusFXSettings.h"
#include "HALAbilitySystemComponent.h"
#include "Components/MeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraFunctionLibrary.h"
#include "Engine/World.h"
#include "Misc/App.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarSideShiftFXLog(TEXT("hal.SideShiftFXLog"),0,TEXT("Side Shift cosmetic exhaust mounts and lifetime (0=off, 1=on)."));
	bool SideShiftWorldSpace(const UNiagaraSystem* S)
	{
		if(!S) { return false; }
		for(const auto& H:S->GetEmitterHandles()) if(const auto* E=H.GetEmitterData();E&&E->bLocalSpace) { return false; }
		return true;
	}
}
bool FHALSideShiftFX::IsValid() const
{
	if(!bEnabled) { return true; }
	const auto R=[](double V,double A,double B) { return FMath::IsFinite(V)&&V>=A&&V<=B; };
	FString Error;
	if(!TireMarks.Validate(Error)||!SideShiftWorldSpace(Sparks)||!SideShiftWorldSpace(Smoke)
		||LeftMounts.IsEmpty()||RightMounts.IsEmpty()||LeftMounts.Num()>4||RightMounts.Num()>4) { return false; }
	for(const auto& M:LeftMounts) if(!M.IsValid()) { return false; }
	for(const auto& M:RightMounts) if(!M.IsValid()) { return false; }
	const auto Color=[&](FLinearColor C) { return R(C.R,0,10)&&R(C.G,0,10)&&R(C.B,0,10)&&R(C.A,0,1); };
	return R(SparkRate,0,600)&&R(SparkSpeed,100,6000)&&R(SparkLifetime,.05,1)
		&&R(SparkSize.X,1,100)&&R(SparkSize.Y,1,100)&&R(SpreadDegrees,1,60)
		&&R(SmokeRate,0,150)&&R(SmokeSpeed,10,2000)&&R(SmokeLifetime,.1,2)&&R(SmokeSize,1,200)
		&&Color(SparkColor)&&Color(SmokeColor)&&R(Glow,0,3)&&R(FadeSeconds,.01f,.3f);
}
bool UHALSideShiftComponent::IsSkidActive() const
{
	const double Age=ServerTime()-Visual.StartServerTime;
	return Visual.bDriving&&Visual.FX.bEnabled&&Visual.FX.TireMarks.bEnabled&&Visual.Activation.IsValid()
		&&Age>=0&&Age<Visual.ShiftSeconds&&!UHALAbilitySystemComponent::IsActorStunned(GetOwner());
}
const FHALTireSkidConfig* UHALSideShiftComponent::GetActiveTireMarks() const
{ return IsSkidActive()?&Visual.FX.TireMarks:nullptr; }
void UHALSideShiftComponent::OnRep_Visual()
{
	if(DrawActivation!=Visual.Activation) { ClearParticles(false); DrawActivation=Visual.Activation; WarnedMounts.Reset(); }
	if(!Visual.bDriving) { ClearParticles(false); }
	SetComponentTickEnabled((Visual.bDriving&&Visual.FX.bEnabled)||!FadingJets.IsEmpty());
}
void UHALSideShiftComponent::ClearParticles(bool bImmediate)
{
	for(const auto& Item:Jets)
	{
		auto* C=Item.Get();
		if(!IsValid(C)) { continue; }
		if(bImmediate) { C->DestroyComponent(); }
		else { C->Deactivate(); FadingJets.Add({C,ServerTime()+2.5}); }
	}
	Jets.Reset();
	for(int32 I=FadingJets.Num()-1;I>=0;--I)
	{
		if(bImmediate||!FadingJets[I].Component.IsValid()||ServerTime()>=FadingJets[I].Deadline||I>=32)
		{ if(auto* C=FadingJets[I].Component.Get()) { C->DestroyComponent(); } FadingJets.RemoveAtSwap(I); }
	}
}
void UHALSideShiftComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Function)
{
	Super::TickComponent(Dt,Type,Function);
	const double Age=ServerTime()-Visual.StartServerTime;
	if(!Visual.bDriving||Age<0||Age>=Visual.ShiftSeconds||UHALAbilitySystemComponent::IsActorStunned(GetOwner())) { ClearParticles(false); }
	else { UpdateParticles(); }
	for(int32 I=FadingJets.Num()-1;I>=0;--I)
	{
		if(!FadingJets[I].Component.IsValid()||ServerTime()>=FadingJets[I].Deadline)
		{ if(auto* C=FadingJets[I].Component.Get()) { C->DestroyComponent(); } FadingJets.RemoveAtSwap(I); }
	}
	if(Jets.IsEmpty()&&FadingJets.IsEmpty()&&(!Visual.bDriving||Age>=Visual.ShiftSeconds)) { SetComponentTickEnabled(false); }
}
void UHALSideShiftComponent::UpdateParticles()
{
	if(!FApp::CanEverRender()||GetNetMode()==NM_DedicatedServer||!GetWorld()->FXSystem.IsValid()) { return; }
	const auto& FX=Visual.FX;
	const double Age=ServerTime()-Visual.StartServerTime;
	const float Envelope=float(FMath::Min(FMath::Clamp(Age/FX.FadeSeconds,0.,1.),FMath::Clamp((Visual.ShiftSeconds-Age)/FX.FadeSeconds,0.,1.)));
	auto* P=GetOwner()->FindComponentByClass<UHALVehiclePresentationComponent>(); if(!P) { return; }
	FHALVehicleFXTargetSnapshot Targets; P->CaptureStatusFXTargets(Targets);
	const auto& Mounts=Visual.Side<0?FX.RightMounts:FX.LeftMounts;
	const int32 Budget=FMath::Min(8,GetDefault<UHALVehicleStatusFXSettings>()->MaximumParticleLayersPerVehicle);
	if(Jets.Num()!=Mounts.Num()*2) { ClearParticles(false); Jets.SetNum(Mounts.Num()*2); }
	for(int32 I=0;I<Jets.Num();++I)
	{
		if(I>=Budget) { continue; }
		const bool Smoke=I%2!=0; FHALResolvedFXMount Mount;
		if(!HALVehicleFX::ResolveMount(Targets,Mounts[I/2],Mount))
		{
			const FName Id(*FString::Printf(TEXT("Side%dMount%d"),int32(Visual.Side),I/2));
			if(!WarnedMounts.Contains(Id)) { WarnedMounts.Add(Id); UE_LOG(LogTemp,Warning,TEXT("SideShift FX missing mount Pawn=%s Mount=%s; configure PartId/socket/offset in skill DA"),*GetNameSafe(GetOwner()),*Id.ToString()); }
			continue;
		}
		bool Created=false;
		if(!IsValid(Jets[I]))
		{
			Jets[I]=UNiagaraFunctionLibrary::SpawnSystemAttached(Smoke?FX.Smoke:FX.Sparks,Mount.Mesh.Get(),Mount.Socket,
				Mount.Relative.GetLocation(),Mount.Relative.Rotator(),EAttachLocation::KeepRelativeOffset,false,false,ENCPoolMethod::None,false);
			Created=IsValid(Jets[I]);
			if(Created) { UE_CLOG(CVarSideShiftFXLog.GetValueOnGameThread(),LogTemp,Log,TEXT("SideShiftFX mount Pawn=%s ShiftSide=%d ExhaustSide=%s Layer=%s Socket=%s"),
				*GetNameSafe(GetOwner()),int32(Visual.Side),Visual.Side<0?TEXT("Right"):TEXT("Left"),Smoke?TEXT("Smoke"):TEXT("Sparks"),*Mount.Socket.ToString()); }
		}
		auto* C=Jets[I].Get(); if(!C) { continue; }
		C->SetRelativeTransform(Mount.Relative);
		const float Speed=Smoke?FX.SmokeSpeed:FX.SparkSpeed;
		const FVector Center=-Visual.Direction*Speed+(Smoke?FVector(0,0,Speed*.2):FVector::ZeroVector);
		const float Spread=Speed*FMath::Tan(FMath::DegreesToRadians(FX.SpreadDegrees))*.57735f;
		C->SetVariableFloat(TEXT("User.SpawnRate"),(Smoke?FX.SmokeRate:FX.SparkRate)*Envelope);
		C->SetVariableFloat(TEXT("User.LifetimeMin"),(Smoke?FX.SmokeLifetime:FX.SparkLifetime)*.65f);
		C->SetVariableFloat(TEXT("User.LifetimeMax"),Smoke?FX.SmokeLifetime:FX.SparkLifetime);
		C->SetVariableVec3(TEXT("User.VelocityMin"),Center-FVector(Spread)); C->SetVariableVec3(TEXT("User.VelocityMax"),Center+FVector(Spread));
		C->SetVariableVec2(TEXT("User.SizeMin"),FX.SparkSize*.65); C->SetVariableVec2(TEXT("User.SizeMax"),FX.SparkSize);
		C->SetVariableVec3(TEXT("User.ScaleMin"),FVector(FX.SmokeSize/100*.65)); C->SetVariableVec3(TEXT("User.ScaleMax"),FVector(FX.SmokeSize/100));
		C->SetVariableLinearColor(TEXT("User.SparkColor"),Smoke?FX.SmokeColor:FX.SparkColor);
		C->SetVariableLinearColor(TEXT("User.HighlightColor"),FLinearColor(1,.92f,.7f));
		C->SetVariableVec4(TEXT("User.StyleParameters"),FVector4(FX.Glow,0,0,0));
		if(Created) { C->Activate(true); }
	}
}
