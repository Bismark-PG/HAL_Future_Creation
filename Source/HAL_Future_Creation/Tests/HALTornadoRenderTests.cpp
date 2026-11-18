#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "../HALTornado.h"
#include "../HALTornadoAbility.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraDataSetAccessor.h"
#include "AssetCompilingManager.h"
#include "MaterialShaderPrecompileMode.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALTornadoRenderTest,"HAL.RenderSmoke.Tornado.GatherSustainDissipateAndToonColors",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALTornadoRenderTest::RunTest(const FString&)
{
	if (!FApp::CanEverRender()) { AddError(TEXT("Requires actual RHI")); return false; }
	const auto* Skill=LoadObject<UHALTornadoConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_Car05_Tornado.DA_Ability_Car05_Tornado"));
	if (!TestNotNull(TEXT("Saved Car05 skill DA"),Skill) || !TestNotNull(TEXT("Saved Niagara tornado"),Skill->FX.System.Get())) { return false; }
	for (const auto* Name:{TEXT("User.WindMaterial"),TEXT("User.PressureMaterial"),TEXT("User.OuterMaterial")})
	{
		const FNiagaraVariable Parameter(FNiagaraTypeDefinition(UMaterialInterface::StaticClass()),Name);
		auto* M=Cast<UMaterial>(Skill->FX.System->GetExposedParameters().GetUObject(Parameter));
		if (!TestNotNull(TEXT("Saved toon material"),M)) { return false; }
		M->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous);
	}
	FAssetCompilingManager::Get().FinishAllCompilation(); Skill->FX.System->RequestCompile(false); Skill->FX.System->WaitForCompilationComplete();
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(true).SetTransactional(false);
	auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	ON_SCOPE_EXIT { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); };
	auto* T=W->SpawnActor<AHALTornado>(); T->State.Activation=FGuid::NewGuid(); T->State.Rules=Skill->Rules; T->State.FX=Skill->FX;
	auto* Capture=W->SpawnActor<ASceneCapture2D>(); auto* Camera=Capture->GetCaptureComponent2D();
	Camera->bCaptureEveryFrame=false; Camera->CaptureSource=SCS_FinalColorLDR; Camera->ProjectionType=ECameraProjectionMode::Orthographic;
	Camera->OrthoWidth=850; Camera->ShowFlags.SetTemporalAA(false);
	Camera->PostProcessSettings.bOverride_AutoExposureMethod=true; Camera->PostProcessSettings.AutoExposureMethod=AEM_Manual;
	Camera->SetWorldLocation(FVector(580,-760,600)); Camera->SetWorldRotation((FVector(0,0,130)-Camera->GetComponentLocation()).Rotation());
	auto* Target=NewObject<UTextureRenderTarget2D>(); Target->InitCustomFormat(768,768,PF_B8G8R8A8,false); Camera->TextureTarget=Target;
	FString Output; if(!FParse::Value(FCommandLine::Get(),TEXT("HALTornadoRenderOutput="),Output)) { Output=FPaths::ProjectSavedDir()/TEXT("Car05Tornado/20261114/Render"); }
	IFileManager::Get().MakeDirectory(*Output,true);
	bool BodyOnly=false;
	const auto Sample=[&](float Age,const TCHAR* Name)
	{
		T->State.StartServerTime=W->GetTimeSeconds()-Age; T->UpdatePresentation(1.f/60);
		if(BodyOnly && T->Materials.Num()>1 && T->Materials[1]) { T->Materials[1]->SetScalarParameterValue(TEXT("Envelope"),0); }
		if (T->Particles) { T->Particles->AdvanceSimulation(3,1.f/60); }
		Camera->CaptureScene(); TArray<FColor> P; Target->GameThread_GetRenderTargetResource()->ReadPixels(P);
		TestTrue(TEXT("Phase proof saved"),FFileHelper::CreateBitmap(*(Output/Name),768,768,P.GetData())); return P;
	};
	const auto Visible=[](const TArray<FColor>& P) { int32 N=0; for(auto C:P) { N+=FMath::Max3(C.R,C.G,C.B)>20; } return N; };
	const auto Difference=[](const TArray<FColor>& A,const TArray<FColor>& B) { int32 N=0; for(int32 I=0;I<A.Num();++I) { N+=FMath::Abs(int32(A[I].G)-B[I].G)>8; } return N; };
	const auto Height=[](const TArray<FColor>& P)
	{
		int32 Low=768,High=-1;
		for(int32 I=0;I<P.Num();++I) if(FMath::Max3(P[I].R,P[I].G,P[I].B)>20) { Low=FMath::Min(Low,I/768); High=FMath::Max(High,I/768); }
		return High>=Low?High-Low+1:0;
	};
	const auto Empty=Sample(0,TEXT("Tornado_000.bmp"));
	if (!TestNotNull(TEXT("Native component created"),T->Particles.Get()) || T->Materials.Num()!=3) { return false; }
	TestEqual(TEXT("Body invisible at creation"),T->Materials[0]->K2_GetScalarParameterValue(TEXT("Envelope")),0.f);
	TestEqual(TEXT("Gather starts from zero opacity"),T->Materials[1]->K2_GetScalarParameterValue(TEXT("Envelope")),0.f);
	const auto Warm=Sample(Skill->Rules.GatherSeconds*.35f,TEXT("Tornado_Gather.bmp"));
	TestTrue(TEXT("Gathering wind is visible before damaging phase"),Visible(Warm)>300);
	BodyOnly=true;
	const auto EarlyBody=Sample(Skill->Rules.GatherSeconds*.15f,TEXT("Tornado_BodyFormation_015.bmp"));
	const auto MidBody=Sample(Skill->Rules.GatherSeconds*.5f,TEXT("Tornado_BodyFormation_050.bmp"));
	const auto LateBody=Sample(Skill->Rules.GatherSeconds*.9f,TEXT("Tornado_BodyFormation_090.bmp"));
	BodyOnly=false;
	TestTrue(TEXT("Body forms progressively even with all ground gathering hidden"),Visible(MidBody)>Visible(EarlyBody)+100 && Visible(LateBody)>Visible(MidBody)+300);
	TestTrue(TEXT("Formation grows a visibly taller column, not just the floor cue"),Height(LateBody)>Height(EarlyBody)+40);
	const auto Full=Sample(Skill->Rules.GatherSeconds+1,TEXT("Tornado_Active.bmp"));
	const auto BaselineFX=T->State.FX;
	T->State.FX.GlowStrength=0;
	const auto NoGlow=Sample(Skill->Rules.GatherSeconds+1,TEXT("Tornado_NoGlow.bmp"));
	int32 BrighterPixels=0;
	for(int32 I=0;I<Full.Num();++I)
	{
		BrighterPixels+=FMath::Max3(Full[I].R,Full[I].G,Full[I].B)>FMath::Max3(NoGlow[I].R,NoGlow[I].G,NoGlow[I].B)+4;
	}
	TestTrue(TEXT("Palette rim emission makes real wind pixels brighter"),BrighterPixels>100);
	T->State.FX=BaselineFX;
	if(auto Controller=T->Particles->GetSystemInstanceController()) if(auto* Instance=Controller->GetSystemInstance_Unsafe())
	{
		for(const auto& E:Instance->GetEmitters())
		{
			const auto Position=FNiagaraDataSetAccessor<FNiagaraPosition>::CreateReader(E->GetParticleData(),TEXT("Position"));
			if(!TestTrue(TEXT("Persistent mesh particle data"),E->GetNumParticles()==1 && Position.IsValid())) { return false; }
			AddInfo(FString::Printf(TEXT("Tornado local particle origin: %s"),*FVector(Position[0]).ToString()));
			TestTrue(TEXT("Body and gather share the world skill's local origin"),FVector(Position[0]).IsNearlyZero(.01));
		}
	}
	TestTrue(TEXT("Sustained tornado grows beyond formation"),Visible(Full)>Visible(Warm)+300);
	const auto Later=Sample(Skill->Rules.GatherSeconds+1.15f,TEXT("Tornado_ActiveMoved.bmp"));
	TestTrue(TEXT("Actual sustained geometry and flow move"),Difference(Full,Later)>300);
	const auto OriginalFX=T->State.FX;
	T->State.FX.OuterOpacity=0;
	const auto NoOuter=Sample(Skill->Rules.GatherSeconds+1,TEXT("Tornado_NoOuter.bmp"));
	const int32 OuterChanged=Difference(Full,NoOuter);
	TestTrue(TEXT("Sparse outer gusts make a separately controllable visible layer"),OuterChanged>100);
	T->State.FX=OriginalFX; T->State.FX.GrayMix=0;
	const auto NoGray=Sample(Skill->Rules.GatherSeconds+1,TEXT("Tornado_NoGray.bmp"));
	const int32 GrayChanged=Difference(Full,NoGray);
	TestTrue(TEXT("Gray-cyan cycling visibly contributes while preserving the base palette"),GrayChanged>300);
	T->State.FX=OriginalFX; T->State.FX.GrayMix=0; T->State.FX.RiseSpeed=0; T->State.FX.FlowSpeed=0; T->State.FX.Turbulence=0;
	const auto Sheared=Sample(Skill->Rules.GatherSeconds+1,TEXT("Tornado_Differential.bmp"));
	T->State.FX.DifferentialRotation=0;
	const auto Rigid=Sample(Skill->Rules.GatherSeconds+1,TEXT("Tornado_CommonRate.bmp"));
	const int32 ShearChanged=Difference(Sheared,Rigid);
	TestTrue(TEXT("Per-stream differential motion changes geometry beyond a common rigid spin"),ShearChanged>300);
	T->State.FX=OriginalFX; T->State.FX.SpinRate=0; T->State.FX.GrayMix=0;
	const auto FlowA=Sample(Skill->Rules.GatherSeconds+1,TEXT("Tornado_NoSpin_FlowA.bmp"));
	const auto FlowB=Sample(Skill->Rules.GatherSeconds+1.3f,TEXT("Tornado_NoSpin_FlowB.bmp"));
	const int32 InternalChanged=Difference(FlowA,FlowB);
	TestTrue(TEXT("Internal circulation continues when common orbit is disabled"),InternalChanged>300);
	T->State.FX=OriginalFX;
	const float End=Skill->Rules.GatherSeconds+Skill->Rules.ActiveSeconds;
	Sample(End,TEXT("Tornado_RetireStart.bmp")); float Last=T->Materials[0]->K2_GetScalarParameterValue(TEXT("MotionTime"));
	float PreviousStep=100;
	for (const float Fraction:{.25f,.5f,.75f})
	{
		Sample(End+Skill->Rules.DissipateSeconds*Fraction,*FString::Printf(TEXT("Tornado_Over_%.2f.bmp"),Fraction));
		const float Now=T->Materials[0]->K2_GetScalarParameterValue(TEXT("MotionTime"));
		TestTrue(TEXT("Dissipating spin stays forward and decelerates"),Now>Last && Now-Last<PreviousStep);
		PreviousStep=Now-Last; Last=Now;
		TestTrue(TEXT("Body shrinks and fades through retirement"),T->Materials[0]->K2_GetScalarParameterValue(TEXT("Envelope"))<1 && T->Materials[0]->K2_GetScalarParameterValue(TEXT("Shape"))<1);
	}
	const auto Gone=Sample(Skill->Rules.TotalSeconds(),TEXT("Tornado_Finished.bmp"));
	TestTrue(TEXT("No visible hard-cut remnant after retirement"),Difference(Empty,Gone)<20);
	Sample(Skill->Rules.GatherSeconds+1,TEXT("Tornado_ColorBaseline.bmp"));
	int32 Changed[3]{}; int32 Index=0;
	for (auto* Color:{&T->State.FX.ShadowColor,&T->State.FX.MidColor,&T->State.FX.LightColor})
	{
		const auto Old=*Color; *Color=FLinearColor(1,0,.1f);
		const auto Pixels=Sample(Skill->Rules.GatherSeconds+1,*FString::Printf(TEXT("Tornado_Color_%d.bmp"),Index));
		Changed[Index++]=Difference(Full,Pixels); TestTrue(TEXT("Each toon color changes actual visible wind"),Changed[Index-1]>80); *Color=Old;
	}
	AddInfo(FString::Printf(TEXT("Tornado visible gather=%d active=%d; motion=%d; toon color changes=%d/%d/%d; retired difference=%d"),Visible(Warm),Visible(Full),Difference(Full,Later),Changed[0],Changed[1],Changed[2],Difference(Empty,Gone)));
	AddInfo(FString::Printf(TEXT("Tornado layered proof: outer=%d gray=%d differential=%d internalWithoutSpin=%d"),OuterChanged,GrayChanged,ShearChanged,InternalChanged));
	AddInfo(FString::Printf(TEXT("Tornado formation/glow proof: body pixels=%d/%d/%d heights=%d/%d/%d brighter=%d"),Visible(EarlyBody),Visible(MidBody),Visible(LateBody),Height(EarlyBody),Height(MidBody),Height(LateBody),BrighterPixels));
	T->Destroy(); return true;
}
#endif
