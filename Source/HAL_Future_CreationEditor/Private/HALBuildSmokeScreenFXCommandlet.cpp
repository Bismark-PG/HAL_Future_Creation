#include "HALBuildSmokeScreenFXCommandlet.h"
#include "HALSmokeScreenAbility.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterFactoryNew.h"
#include "NiagaraSystemFactoryNew.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraEditorUtilities.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionParticleColor.h"
#include "Materials/MaterialExpressionParticleRelativeTime.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Engine/StaticMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace
{
	const FString SmokeRoot(TEXT("/Game/Assets/FX/SmokeScreen/"));
	template<class T> T* SmokeAsset(const FString& Path)
	{ return NewObject<T>(CreatePackage(*Path),*FPackageName::GetShortName(Path),RF_Public|RF_Standalone); }
	template<class T> T* SmokeNode(UMaterial& M)
	{ auto* N=NewObject<T>(&M); M.GetExpressionCollection().AddExpression(N); return N; }
	void SmokeInput(UMaterialExpressionCustom& H,FName Name,UMaterialExpression* E,int32 Output=0)
	{ auto& I=H.Inputs.AddDefaulted_GetRef(); I.InputName=Name; I.Input.Connect(Output,E); }
	UMaterialExpressionScalarParameter* SmokeScalar(UMaterial& M,FName Name,float V)
	{ auto* N=SmokeNode<UMaterialExpressionScalarParameter>(M); N->ParameterName=Name; N->DefaultValue=V; return N; }
	UMaterialExpressionVectorParameter* SmokeColor(UMaterial& M,FName Name,FLinearColor V)
	{ auto* N=SmokeNode<UMaterialExpressionVectorParameter>(M); N->ParameterName=Name; N->DefaultValue=V; return N; }
	void SmokeConnect(UMaterial& M,UMaterialExpressionCustom& H)
	{
		auto* RGB=SmokeNode<UMaterialExpressionComponentMask>(M); RGB->R=RGB->G=RGB->B=true; RGB->Input.Connect(0,&H);
		auto* Alpha=SmokeNode<UMaterialExpressionComponentMask>(M); Alpha->R=Alpha->G=Alpha->B=false; Alpha->A=true; Alpha->Input.Connect(0,&H);
		M.GetEditorOnlyData()->EmissiveColor.Connect(0,RGB); M.GetEditorOnlyData()->Opacity.Connect(0,Alpha);
		M.PostEditChange();
	}
	UMaterial* SmokeOverlay(bool bScreen)
	{
		auto* M=SmokeAsset<UMaterial>(SmokeRoot+(bScreen?TEXT("M_SmokeScreen_LocalVeil"):TEXT("M_SmokeScreen_CloudCoverage")));
		M->MaterialDomain=MD_UI; M->BlendMode=BLEND_Translucent;
		auto* H=SmokeNode<UMaterialExpressionCustom>(*M); H->OutputType=CMOT_Float4;
		SmokeInput(*H,TEXT("UV"),SmokeNode<UMaterialExpressionTextureCoordinate>(*M));
		SmokeInput(*H,TEXT("Opacity"),SmokeNode<UMaterialExpressionVertexColor>(*M),4);
		SmokeInput(*H,TEXT("Clock"),SmokeScalar(*M,TEXT("Clock"),0));
		SmokeInput(*H,TEXT("FlowSpeed"),SmokeScalar(*M,TEXT("FlowSpeed"),.7f));
		SmokeInput(*H,TEXT("Strength"),SmokeScalar(*M,TEXT("Strength"),.8f));
		SmokeInput(*H,TEXT("DarkColor"),SmokeColor(*M,TEXT("DarkColor"),FLinearColor(.012f,.016f,.02f)));
		SmokeInput(*H,TEXT("LightColor"),SmokeColor(*M,TEXT("LightColor"),FLinearColor(.16f,.18f,.2f)));
		H->Code=TEXT("float2 p=UV*2-1; float t=Clock*FlowSpeed; ")
			TEXT("float2 q=p+float2(sin(p.y*4+t*.7),cos(p.x*4-t*.5))*.12; ")
			TEXT("float n=.5+.24*sin(q.x*9+t)*sin(q.y*8-t*.7)+.12*sin(dot(q,float2(16,11))-t*.5); ")
			TEXT("float z=sqrt(saturate(1-dot(p,p))); float light=saturate(dot(normalize(float3(p,z+.15)),normalize(float3(-.3,-.45,.84)))*.5+.5); ")
			TEXT("float shade=.18+.36*smoothstep(.3,.52,n)+.32*smoothstep(.65,.85,light); ")
			TEXT("float3 color=lerp(DarkColor.rgb,LightColor.rgb,shade); ");
		H->Code+=bScreen?TEXT("return float4(color*.55,Strength*(.82+.18*saturate(n))*Opacity);")
			:TEXT("float edge=1-smoothstep(.8,1,length(p)); return float4(color,edge*Opacity);");
		SmokeConnect(*M,*H); return M;
	}
	UMaterial* SmokeJetMaterial()
	{
		auto* M=SmokeAsset<UMaterial>(SmokeRoot+TEXT("M_SmokeScreen_ToonPuff"));
		M->BlendMode=BLEND_Translucent; M->TwoSided=true; M->SetShadingModel(MSM_Unlit); M->SetMaterialUsage(MATUSAGE_NiagaraMeshParticles);
		auto* H=SmokeNode<UMaterialExpressionCustom>(*M); H->OutputType=CMOT_Float4;
		SmokeInput(*H,TEXT("N"),SmokeNode<UMaterialExpressionVertexNormalWS>(*M));
		SmokeInput(*H,TEXT("Tint"),SmokeNode<UMaterialExpressionParticleColor>(*M),5);
		SmokeInput(*H,TEXT("Age"),SmokeNode<UMaterialExpressionParticleRelativeTime>(*M));
		H->Code=TEXT("float s=saturate(dot(normalize(N),normalize(float3(-.3,-.4,.85)))*.5+.5); ")
			TEXT("float3 c=Tint.rgb*(.12+.35*smoothstep(.3,.5,s)+.5*smoothstep(.72,.85,s)); ")
			TEXT("return float4(c,smoothstep(0,.08,Age)*(1-smoothstep(.45,1,Age))*.88*Tint.a);");
		SmokeConnect(*M,*H);
		auto* W=SmokeNode<UMaterialExpressionCustom>(*M); W->OutputType=CMOT_Float3;
		SmokeInput(*W,TEXT("N"),SmokeNode<UMaterialExpressionVertexNormalWS>(*M));
		SmokeInput(*W,TEXT("Age"),SmokeNode<UMaterialExpressionParticleRelativeTime>(*M));
		W->Code=TEXT("return N*(55*smoothstep(0,.85,Age));"); M->GetEditorOnlyData()->WorldPositionOffset.Connect(0,W); M->PostEditChange(); return M;
	}
	template<class T> FNiagaraVariable SmokeUser(UNiagaraSystem& S,FNiagaraTypeDefinition Type,const TCHAR* Name,T Value)
	{ FNiagaraVariable V(Type,Name); S.GetExposedParameters().AddParameter(V); S.GetExposedParameters().SetParameterValue(Value,V); return V; }
	void SmokeLink(UNiagaraNodeFunctionCall& N,const TCHAR* Name,FNiagaraTypeDefinition Type,const FNiagaraVariable& V)
	{
		const auto H=FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle::CreateModuleParameterHandle(Name),&N);
		auto& Pin=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(N,H,Type,FGuid(),FGuid());
		FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(Pin,V,{});
	}
	UNiagaraNodeOutput* SmokeOutput(UNiagaraGraph& G,ENiagaraScriptUsage U)
	{ for(const auto& N:G.Nodes) if(auto* O=Cast<UNiagaraNodeOutput>(N.Get());O&&O->GetUsage()==U) { return O; } return nullptr; }
	UNiagaraNodeFunctionCall* SmokeModule(UNiagaraNodeOutput& O,const TCHAR* Path)
	{ return FNiagaraStackGraphUtilities::AddScriptModuleToStack(LoadObject<UNiagaraScript>(nullptr,Path),O); }
	void SmokeRandom(UNiagaraNodeFunctionCall& N,const TCHAR* Name,FNiagaraTypeDefinition Type,const TCHAR* Path,const FNiagaraVariable& Min,const FNiagaraVariable& Max)
	{
		const auto H=FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle::CreateModuleParameterHandle(Name),&N);
		auto& Pin=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(N,H,Type,FGuid(),FGuid()); UNiagaraNodeFunctionCall* Random=nullptr;
		FNiagaraStackGraphUtilities::SetDynamicInputForFunctionInput(Pin,LoadObject<UNiagaraScript>(nullptr,Path),Random);
		check(Random); SmokeLink(*Random,TEXT("Minimum"),Type,Min); SmokeLink(*Random,TEXT("Maximum"),Type,Max);
	}
	UNiagaraSystem* SmokeJet(UMaterial& Material)
	{
		auto* S=SmokeAsset<UNiagaraSystem>(SmokeRoot+TEXT("NS_SmokeScreen_UpwardJet")); UNiagaraSystemFactoryNew::InitializeSystem(S,true);
		auto* E=NewObject<UNiagaraEmitter>(S,TEXT("UpwardSmoke")); UNiagaraEmitterFactoryNew::InitializeEmitter(E,false);
		auto* Data=E->GetLatestEmitterData(); Data->bLocalSpace=false; Data->bDeterminism=true; Data->SimTarget=ENiagaraSimTarget::CPUSim;
		auto& G=*CastChecked<UNiagaraScriptSource>(Data->GraphSource)->NodeGraph;
		auto* EU=SmokeOutput(G,ENiagaraScriptUsage::EmitterUpdateScript); auto* PS=SmokeOutput(G,ENiagaraScriptUsage::ParticleSpawnScript); auto* PU=SmokeOutput(G,ENiagaraScriptUsage::ParticleUpdateScript);
		auto* State=SmokeModule(*EU,TEXT("/Niagara/Modules/Emitter/EmitterState.EmitterState"));
		auto* Mode=LoadObject<UEnum>(nullptr,TEXT("/Niagara/Enums/ENiagaraEmitterLifeCycleMode.ENiagaraEmitterLifeCycleMode"));
		for(auto* Pin:State->Pins) if(Pin->PinName==TEXT("Life Cycle Mode")) for(int32 I=0;I<Mode->NumEnums();++I)
			if(Mode->GetDisplayNameTextByIndex(I).ToString()==TEXT("Self")) { Pin->DefaultValue=Mode->GetNameStringByIndex(I); }
		SmokeLink(*State,TEXT("Loop Duration"),FNiagaraTypeDefinition::GetFloatDef(),SmokeUser(*S,FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.LoopDuration"),60.f));
		SmokeLink(*SmokeModule(*EU,TEXT("/Niagara/Modules/Emitter/SpawnRate.SpawnRate")),TEXT("SpawnRate"),FNiagaraTypeDefinition::GetFloatDef(),SmokeUser(*S,FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.SpawnRate"),30.f));
		const auto MinV=SmokeUser(*S,FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.VelocityMin"),FVector3f(-80,-45,220));
		const auto MaxV=SmokeUser(*S,FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.VelocityMax"),FVector3f(-20,45,320));
		const auto Color=SmokeUser(*S,FNiagaraTypeDefinition::GetColorDef(),TEXT("User.SmokeColor"),FLinearColor(.16f,.18f,.2f));
		auto* Values=FNiagaraStackGraphUtilities::AddParameterModuleToStack({
			FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(),TEXT("Particles.Lifetime")),FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(),TEXT("Particles.Color")),
			FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(),TEXT("Particles.Scale")),FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(),TEXT("Particles.Velocity"))},*PS,INDEX_NONE,
			{TEXT("1.1"),TEXT("(R=.16,G=.18,B=.2,A=1)"),TEXT("(X=.3,Y=.3,Z=.3)"),TEXT("(X=0,Y=0,Z=250)")});
		SmokeLink(*Values,TEXT("Particles.Color"),FNiagaraTypeDefinition::GetColorDef(),Color);
		SmokeRandom(*Values,TEXT("Particles.Velocity"),FNiagaraTypeDefinition::GetVec3Def(),TEXT("/Niagara/DynamicInputs/UniformRange/V2/RandomRangeVector.RandomRangeVector"),MinV,MaxV);
		SmokeModule(*PU,TEXT("/Niagara/Modules/Update/Lifetime/ParticleState.ParticleState"));
		SmokeModule(*PU,TEXT("/Niagara/Modules/Solvers/SolveForcesAndVelocity.SolveForcesAndVelocity"));
		auto* Renderer=NewObject<UNiagaraMeshRendererProperties>(E); Renderer->Meshes.Reset(); Renderer->Meshes.AddDefaulted_GetRef().Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		Renderer->bOverrideMaterials=true; Renderer->OverrideMaterials.AddDefaulted_GetRef().ExplicitMat=&Material;
		E->AddRenderer(Renderer,Data->Version.VersionGuid); FNiagaraEditorUtilities::AddEmitterToSystem(*S,*E,E->GetExposedVersion().VersionGuid);
		S->PostEditChange(); S->RequestCompile(true); S->WaitForCompilationComplete(); return S;
	}
	bool SmokeSave(UObject& O)
	{
		FAssetRegistryModule::AssetCreated(&O); auto* P=O.GetPackage(); P->MarkPackageDirty(); FSavePackageArgs A; A.TopLevelFlags=RF_Public|RF_Standalone; A.SaveFlags=SAVE_NoError;
		return UPackage::SavePackage(P,&O,*FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension()),A);
	}
}
int32 UHALBuildSmokeScreenFXCommandlet::Main(const FString& Params)
{
	const FString ConfigPath(TEXT("/Game/Data/Abilities/DA_Ability_Car08_SmokeScreen"));
	for(const auto& Path:{SmokeRoot+TEXT("M_SmokeScreen_CloudCoverage"),SmokeRoot+TEXT("M_SmokeScreen_LocalVeil"),SmokeRoot+TEXT("M_SmokeScreen_ToonPuff"),SmokeRoot+TEXT("NS_SmokeScreen_UpwardJet"),ConfigPath})
	{ if(FPackageName::DoesPackageExist(Path)) { UE_LOG(LogTemp,Error,TEXT("Refusing to overwrite %s"),*Path); return 1; } }
	auto* Cloud=SmokeOverlay(false); auto* Screen=SmokeOverlay(true); auto* Puff=SmokeJetMaterial(); auto* Jet=SmokeJet(*Puff);
	auto* Config=SmokeAsset<UHALSmokeScreenConfig>(ConfigPath); Config->FX.CloudMaterial=Cloud; Config->FX.ScreenMaterial=Screen; Config->FX.JetSystem=Jet;
	FString Error; if(!Config->Validate(Error)||!Jet->IsReadyToRun()) { UE_LOG(LogTemp,Error,TEXT("Smoke authoring validation: %s Ready=%d"),*Error,Jet->IsReadyToRun()); return 2; }
	const TArray<UObject*> Objects{Cloud,Screen,Puff,Jet,Config};
	for(UObject* Object:Objects) { if(!SmokeSave(*Object)) { return 3; } }
	UE_LOG(LogTemp,Display,TEXT("Car08 smoke: created five new resources; no BP, map or existing DA modified.")); return 0;
}
