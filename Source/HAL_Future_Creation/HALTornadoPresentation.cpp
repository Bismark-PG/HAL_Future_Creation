#include "HALTornado.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Engine/World.h"

void AHALTornado::UpdatePresentation(float)
{
	if (!FApp::CanEverRender() || GetNetMode() == NM_DedicatedServer || !GetWorld()->FXSystem.IsValid()
		|| !State.FX.System || !State.FX.IsValid()) { return; }
	const auto& FX = State.FX; const auto& R = State.Rules;
	if (!Particles)
	{
		Particles = NewObject<UNiagaraComponent>(this); Particles->SetAutoActivate(false); Particles->SetAsset(FX.System);
		Particles->SetupAttachment(GetRootComponent()); Particles->RegisterComponent(); Particles->AddTickPrerequisiteActor(this);
		Particles->SetVariableVec3(TEXT("User.FlowOrigin"),FVector::ZeroVector);
		Particles->SetVariableVec3(TEXT("User.WindScale"),FVector(R.Radius/100,R.Radius/100,R.Height/100));
		Particles->SetVariableVec3(TEXT("User.PressureScale"),FVector(R.Radius/100));
		for (const auto* Name : {TEXT("User.WindMaterial"),TEXT("User.PressureMaterial"),TEXT("User.OuterMaterial")})
		{
			const FNiagaraVariable V(FNiagaraTypeDefinition(UMaterialInterface::StaticClass()),Name);
			auto* Source = Cast<UMaterialInterface>(FX.System->GetExposedParameters().GetUObject(V));
			// The original two-layer system remains usable as a presentation fallback.
			if (!Source && Materials.Num()==2) { break; }
			auto* M = Source ? UMaterialInstanceDynamic::Create(Source,this) : nullptr;
			Materials.Add(M); if (M) { Particles->SetVariableMaterial(Name,M); }
		}
		// Set invisible envelopes before first activation; pending compile/DDC is handled by Niagara.
		for (const auto& M : Materials) { if (M) { M->SetScalarParameterValue(TEXT("Envelope"),0); } }
		Particles->Activate(true);
	}
	const double Age = FMath::Max(0.,ServerTime()-State.StartServerTime);
	const auto Phase = R.Phase(Age); const float P = FMath::Clamp(R.PhaseProgress(Age),0.f,1.f);
	const float Smooth = P*P*(3-2*P);
	const float Fade = Phase == EHALTornadoPhase::Gathering ? Smooth
		: Phase == EHALTornadoPhase::Active ? 1.f : Phase == EHALTornadoPhase::Dissipating ? 1-Smooth : 0.f;
	const float Formation = Phase == EHALTornadoPhase::Gathering ? FMath::Pow(Smooth,FX.FormationExponent) : 1.f;
	// Reveal the actual wind column from the bottom up; the ground cue has its own envelope.
	const float Body = Phase == EHALTornadoPhase::Gathering ? FMath::SmoothStep(0.f,.55f,P) : Fade;
	const float Gather = Phase == EHALTornadoPhase::Gathering ? FMath::Sin(P*UE_PI)*.75f+.2f*Fade : .2f*Fade;
	const float Shape = Phase == EHALTornadoPhase::Gathering ? .65f+.35f*Formation : .08f+.92f*Fade;
	const float GatherShape = Phase == EHALTornadoPhase::Gathering ? FMath::Lerp(FX.GatherRadiusScale,1.f,Smooth) : FMath::Lerp(.65f,1.f,Fade);
	// Integral of (1-p)^2: retirement slows continuously and can never reverse the vortex spin.
	const double ActiveEnd = R.GatherSeconds+R.ActiveSeconds;
	const float MotionTime = Age <= ActiveEnd ? float(Age)
		: float(ActiveEnd + R.DissipateSeconds*(P-P*P+P*P*P/3));
	FVector GroundNormal = FVector::UpVector; FHitResult Hit;
	FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_WorldStatic); Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	if (GetWorld()->LineTraceSingleByObjectType(Hit,GetActorLocation()+FVector(0,0,25),GetActorLocation()-FVector(0,0,80),Objects,
		FCollisionQueryParams(SCENE_QUERY_STAT(TornadoVisualGround),false,this)) && Hit.ImpactNormal.Z >= R.MinimumGroundNormalZ)
	{ GroundNormal = Hit.ImpactNormal; }
	for (int32 I=0; I<Materials.Num(); ++I) if (auto* M = Materials[I].Get())
	{
		M->SetScalarParameterValue(TEXT("Envelope"),I == 1 ? Gather : Body);
		M->SetScalarParameterValue(TEXT("Shape"),I == 1 ? GatherShape : Shape);
		M->SetScalarParameterValue(TEXT("Formation"),Formation);
		M->SetScalarParameterValue(TEXT("FormationFeather"),FX.FormationFeather);
		M->SetScalarParameterValue(TEXT("GlowStrength"),FX.GlowStrength);
		M->SetScalarParameterValue(TEXT("GlowPower"),FX.GlowPower);
		M->SetScalarParameterValue(TEXT("MotionTime"),MotionTime);
		M->SetScalarParameterValue(TEXT("SpinRate"),FX.SpinRate);
		M->SetScalarParameterValue(TEXT("FlowSpeed"),FX.FlowSpeed);
		M->SetScalarParameterValue(TEXT("Turbulence"),FX.Turbulence);
		M->SetScalarParameterValue(TEXT("DifferentialRotation"),FX.DifferentialRotation);
		M->SetScalarParameterValue(TEXT("RiseSpeed"),FX.RiseSpeed);
		M->SetScalarParameterValue(TEXT("CoreOpacity"),FX.CoreOpacity);
		M->SetScalarParameterValue(TEXT("OuterOpacity"),FX.OuterOpacity);
		M->SetScalarParameterValue(TEXT("OuterRadiusScale"),FX.OuterRadiusScale);
		M->SetScalarParameterValue(TEXT("OuterSpeedScale"),FX.OuterSpeedScale);
		M->SetScalarParameterValue(TEXT("GrayMix"),FX.GrayMix);
		M->SetScalarParameterValue(TEXT("ColorCycleSeconds"),FX.ColorCycleSeconds);
		M->SetVectorParameterValue(TEXT("GrayColor"),FX.GrayColor);
		M->SetScalarParameterValue(TEXT("Opacity"),FX.Opacity);
		M->SetScalarParameterValue(TEXT("ToneSoftness"),FX.ToneSoftness);
		M->SetScalarParameterValue(TEXT("ShadowThreshold"),FX.ShadowThreshold);
		M->SetScalarParameterValue(TEXT("LightThreshold"),FX.LightThreshold);
		M->SetScalarParameterValue(TEXT("GroundLift"),FX.GroundLift/FMath::Max(.3f,R.Radius/100));
		M->SetVectorParameterValue(TEXT("ShadowColor"),FX.ShadowColor);
		M->SetVectorParameterValue(TEXT("MidColor"),FX.MidColor);
		M->SetVectorParameterValue(TEXT("LightColor"),FX.LightColor);
		M->SetVectorParameterValue(TEXT("GroundNormal"),FLinearColor(GroundNormal.X,GroundNormal.Y,GroundNormal.Z,1));
	}
}
