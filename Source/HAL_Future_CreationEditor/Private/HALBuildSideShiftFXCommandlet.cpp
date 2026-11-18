#include "HALBuildSideShiftFXCommandlet.h"
#include "HALSideShiftAbility.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
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
#include "Materials/MaterialExpressionParticleColor.h"
#include "Materials/MaterialExpressionParticleRelativeTime.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "UObject/UnrealType.h"
#include "UObject/SavePackage.h"

namespace
{
	const FString ShiftRoot(TEXT("/Game/Assets/FX/SideShift/"));
	bool ShiftSave(UObject& Object,bool bNew=true)
	{
		if(bNew) { FAssetRegistryModule::AssetCreated(&Object); } auto* Package=Object.GetPackage(); Package->MarkPackageDirty();
		FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
		return UPackage::SavePackage(Package,&Object,*FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension()),Args);
	}
	template<class T> T* ShiftAsset(const FString& Path)
	{ return NewObject<T>(CreatePackage(*Path),*FPackageName::GetShortName(Path),RF_Public|RF_Standalone); }
	template<class T> T* ShiftNode(UMaterial& M)
	{ auto* N=NewObject<T>(&M); M.GetExpressionCollection().AddExpression(N); return N; }
	void ShiftInput(UMaterialExpressionCustom& H,FName Name,UMaterialExpression* E,int32 Output=0)
	{ auto& I=H.Inputs.AddDefaulted_GetRef(); I.InputName=Name; I.Input.Connect(Output,E); }
	template<class T> FNiagaraVariable ShiftUser(UNiagaraSystem& S,FNiagaraTypeDefinition Type,const TCHAR* Name,T Value)
	{ FNiagaraVariable V(Type,Name); S.GetExposedParameters().AddParameter(V); S.GetExposedParameters().SetParameterValue(Value,V); return V; }
	void ShiftLink(UNiagaraNodeFunctionCall& N,const TCHAR* Name,FNiagaraTypeDefinition Type,const FNiagaraVariable& Variable)
	{
		const auto H=FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle::CreateModuleParameterHandle(Name),&N);
		auto& P=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(N,H,Type,FGuid(),FGuid());
		FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(P,Variable,{});
	}
	void ShiftRandom(UNiagaraNodeFunctionCall& N,const TCHAR* Name,FNiagaraTypeDefinition Type,const TCHAR* Path,const FNiagaraVariable& Min,const FNiagaraVariable& Max)
	{
		const auto H=FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle::CreateModuleParameterHandle(Name),&N);
		auto& Pin=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(N,H,Type,FGuid(),FGuid());
		UNiagaraNodeFunctionCall* R=nullptr;
		FNiagaraStackGraphUtilities::SetDynamicInputForFunctionInput(Pin,LoadObject<UNiagaraScript>(nullptr,Path),R);
		check(R); ShiftLink(*R,TEXT("Minimum"),Type,Min); ShiftLink(*R,TEXT("Maximum"),Type,Max);
	}
	UNiagaraNodeOutput* ShiftOutput(UNiagaraGraph& G,ENiagaraScriptUsage Usage)
	{ for(const auto& N:G.Nodes) if(auto* O=Cast<UNiagaraNodeOutput>(N.Get());O&&O->GetUsage()==Usage) { return O; } return nullptr; }
	UNiagaraNodeFunctionCall* ShiftModule(UNiagaraNodeOutput& Out,const TCHAR* Path)
	{ return FNiagaraStackGraphUtilities::AddScriptModuleToStack(LoadObject<UNiagaraScript>(nullptr,Path),Out); }
	UMaterial* ShiftSmokeMaterial()
	{
		auto* M=ShiftAsset<UMaterial>(ShiftRoot+TEXT("M_SideShift_ToonSmoke"));
		M->BlendMode=BLEND_Translucent; M->SetShadingModel(MSM_Unlit); M->TwoSided=true; M->SetMaterialUsage(MATUSAGE_NiagaraMeshParticles);
		auto* H=ShiftNode<UMaterialExpressionCustom>(*M); H->OutputType=CMOT_Float4;
		ShiftInput(*H,TEXT("N"),ShiftNode<UMaterialExpressionVertexNormalWS>(*M));
		ShiftInput(*H,TEXT("Tint"),ShiftNode<UMaterialExpressionParticleColor>(*M),5);
		ShiftInput(*H,TEXT("Age"),ShiftNode<UMaterialExpressionParticleRelativeTime>(*M));
		H->Code=TEXT("float shade=saturate(dot(normalize(N),normalize(float3(.3,-.4,.86)))*.5+.5); "
			"float3 c=lerp(Tint.rgb*.46,Tint.rgb,smoothstep(.28,.48,shade)); "
			"c=lerp(c,lerp(Tint.rgb,float3(1,1,1),.45),smoothstep(.69,.87,shade)); "
			"float alpha=smoothstep(0,.08,Age)*(1-smoothstep(.4,1,Age)); return float4(c,alpha*Tint.a*.85);");
		auto* C=ShiftNode<UMaterialExpressionComponentMask>(*M); C->R=C->G=C->B=true; C->Input.Connect(0,H);
		auto* A=ShiftNode<UMaterialExpressionComponentMask>(*M); A->R=A->G=A->B=false; A->A=true; A->Input.Connect(0,H);
		M->GetEditorOnlyData()->EmissiveColor.Connect(0,C); M->GetEditorOnlyData()->Opacity.Connect(0,A);
		auto* W=ShiftNode<UMaterialExpressionCustom>(*M); W->OutputType=CMOT_Float3;
		ShiftInput(*W,TEXT("N"),ShiftNode<UMaterialExpressionVertexNormalWS>(*M)); ShiftInput(*W,TEXT("Age"),ShiftNode<UMaterialExpressionParticleRelativeTime>(*M));
		W->Code=TEXT("return N*(8*smoothstep(0,.6,Age));"); M->GetEditorOnlyData()->WorldPositionOffset.Connect(0,W);
		M->PostEditChange(); return M;
	}
	UNiagaraSystem* ShiftSmoke(UMaterial& Material)
	{
		auto* S=ShiftAsset<UNiagaraSystem>(ShiftRoot+TEXT("NS_SideShift_ToonSmoke")); UNiagaraSystemFactoryNew::InitializeSystem(S,true);
		auto* E=NewObject<UNiagaraEmitter>(S,TEXT("SideSmoke")); UNiagaraEmitterFactoryNew::InitializeEmitter(E,false);
		auto* Data=E->GetLatestEmitterData(); Data->bLocalSpace=false; Data->bDeterminism=true; Data->SimTarget=ENiagaraSimTarget::CPUSim;
		auto& G=*CastChecked<UNiagaraScriptSource>(Data->GraphSource)->NodeGraph;
		auto* EU=ShiftOutput(G,ENiagaraScriptUsage::EmitterUpdateScript); auto* PS=ShiftOutput(G,ENiagaraScriptUsage::ParticleSpawnScript); auto* PU=ShiftOutput(G,ENiagaraScriptUsage::ParticleUpdateScript);
		auto* State=ShiftModule(*EU,TEXT("/Niagara/Modules/Emitter/EmitterState.EmitterState"));
		auto* Mode=LoadObject<UEnum>(nullptr,TEXT("/Niagara/Enums/ENiagaraEmitterLifeCycleMode.ENiagaraEmitterLifeCycleMode"));
		for(auto* Pin:State->Pins) if(Pin->PinName==TEXT("Life Cycle Mode")) for(int32 I=0;I<Mode->NumEnums();++I)
			if(Mode->GetDisplayNameTextByIndex(I).ToString()==TEXT("Self")) { Pin->DefaultValue=Mode->GetNameStringByIndex(I); }
		ShiftLink(*State,TEXT("Loop Duration"),FNiagaraTypeDefinition::GetFloatDef(),ShiftUser(*S,FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.LoopDuration"),60.f));
		ShiftLink(*ShiftModule(*EU,TEXT("/Niagara/Modules/Emitter/SpawnRate.SpawnRate")),TEXT("SpawnRate"),FNiagaraTypeDefinition::GetFloatDef(),ShiftUser(*S,FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.SpawnRate"),35.f));
		ShiftModule(*PS,TEXT("/Niagara/Modules/Spawn/Location/SystemLocation.SystemLocation"));
		const auto LifeMin=ShiftUser(*S,FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.LifetimeMin"),.26f);
		const auto LifeMax=ShiftUser(*S,FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.LifetimeMax"),.4f);
		const auto VelocityMin=ShiftUser(*S,FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.VelocityMin"),FVector3f(0,500,80));
		const auto VelocityMax=ShiftUser(*S,FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.VelocityMax"),FVector3f(0,700,160));
		const auto ScaleMin=ShiftUser(*S,FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.ScaleMin"),FVector3f(.22f));
		const auto ScaleMax=ShiftUser(*S,FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.ScaleMax"),FVector3f(.34f));
		const auto Color=ShiftUser(*S,FNiagaraTypeDefinition::GetColorDef(),TEXT("User.SparkColor"),FLinearColor(.7f,.75f,.8f));
		auto* Values=FNiagaraStackGraphUtilities::AddParameterModuleToStack({
			FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(),TEXT("Particles.Lifetime")),
			FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(),TEXT("Particles.Color")),
			FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(),TEXT("Particles.Scale")),
			FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(),TEXT("Particles.Velocity"))},*PS,INDEX_NONE,
			{TEXT(".4"),TEXT("(R=.7,G=.75,B=.8,A=1)"),TEXT("(X=.34,Y=.34,Z=.34)"),TEXT("(X=0,Y=600,Z=120)")});
		ShiftLink(*Values,TEXT("Particles.Color"),FNiagaraTypeDefinition::GetColorDef(),Color);
		ShiftRandom(*Values,TEXT("Particles.Lifetime"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("/Niagara/DynamicInputs/UniformRange/V2/RandomRangeFloat.RandomRangeFloat"),LifeMin,LifeMax);
		ShiftRandom(*Values,TEXT("Particles.Scale"),FNiagaraTypeDefinition::GetVec3Def(),TEXT("/Niagara/DynamicInputs/UniformRange/V2/RandomRangeVector.RandomRangeVector"),ScaleMin,ScaleMax);
		ShiftRandom(*Values,TEXT("Particles.Velocity"),FNiagaraTypeDefinition::GetVec3Def(),TEXT("/Niagara/DynamicInputs/UniformRange/V2/RandomRangeVector.RandomRangeVector"),VelocityMin,VelocityMax);
		ShiftModule(*PU,TEXT("/Niagara/Modules/Update/Lifetime/ParticleState.ParticleState"));
		ShiftModule(*PU,TEXT("/Niagara/Modules/Solvers/SolveForcesAndVelocity.SolveForcesAndVelocity"));
		auto* Renderer=NewObject<UNiagaraMeshRendererProperties>(E); Renderer->Meshes.Reset(); Renderer->Meshes.AddDefaulted_GetRef().Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		Renderer->bOverrideMaterials=true; Renderer->OverrideMaterials.AddDefaulted_GetRef().ExplicitMat=&Material;
		E->AddRenderer(Renderer,Data->Version.VersionGuid); FNiagaraEditorUtilities::AddEmitterToSystem(*S,*E,E->GetExposedVersion().VersionGuid);
		S->PostEditChange(); S->RequestCompile(true); S->WaitForCompilationComplete(); return S;
	}
}
int32 UHALBuildSideShiftFXCommandlet::Main(const FString& Params)
{
	const FString ConfigPath(TEXT("/Game/Data/Abilities/DA_Ability_Car07_SideShift"));
	if(FParse::Param(*Params,TEXT("DriftArc")))
	{
		auto* Skill=LoadObject<UHALSideShiftConfig>(nullptr,*ConfigPath); if(!Skill) { return 20; }
		auto* Before=DuplicateObject<UHALSideShiftConfig>(Skill,GetTransientPackage());
		Skill->Motion.DriftLateralSpan=450; Skill->Motion.DriftArcBend=1; Skill->Motion.MaximumAcceleration=100000;
		for(TFieldIterator<FProperty> It(Skill->GetClass());It;++It)
		{ if(It->GetFName()!=TEXT("Motion")&&!It->Identical_InContainer(Before,Skill)) { return 21; } }
		const TSet<FName> Allowed={TEXT("DriftLateralSpan"),TEXT("DriftArcBend"),TEXT("MaximumAcceleration")};
		for(TFieldIterator<FProperty> It(FHALSideShiftMotion::StaticStruct());It;++It)
		{ if(!Allowed.Contains(It->GetFName())&&!It->Identical_InContainer(&Before->Motion,&Skill->Motion)) { return 22; } }
		FString Error; if(!Skill->Validate(Error)||!ShiftSave(*Skill,false)) { return 23; }
		UE_LOG(LogTemp,Display,TEXT("DriftArc saved Span=%.1f Bend=%.2f Accel=%.1f Shift=%.3f; all other settings preserved"),Skill->Motion.DriftLateralSpan,Skill->Motion.DriftArcBend,Skill->Motion.MaximumAcceleration,Skill->Motion.DurationSeconds); return 0;
	}
	if(FParse::Param(*Params,TEXT("QuarterTurn90")))
	{
		auto* Skill=LoadObject<UHALSideShiftConfig>(nullptr,*ConfigPath); if(!Skill) { return 10; }
		auto* Before=DuplicateObject<UHALSideShiftConfig>(Skill,GetTransientPackage());
		Skill->Motion.DriftTurnDegrees=90; Skill->Motion.DriftInputThreshold=.2f; Skill->Motion.DriftYawRate=600;
		for(TFieldIterator<FProperty> It(Skill->GetClass());It;++It)
		{ if(It->GetFName()!=TEXT("Motion")&&!It->Identical_InContainer(Before,Skill)) { return 11; } }
		const TSet<FName> Allowed={TEXT("DriftTurnDegrees"),TEXT("DriftInputThreshold"),TEXT("DriftYawRate")};
		for(TFieldIterator<FProperty> It(FHALSideShiftMotion::StaticStruct());It;++It)
		{ if(!Allowed.Contains(It->GetFName())&&!It->Identical_InContainer(&Before->Motion,&Skill->Motion)) { return 12; } }
		FString Error; if(!Skill->Validate(Error)||!ShiftSave(*Skill,false)) { return 13; }
		UE_LOG(LogTemp,Display,TEXT("QuarterTurn90 saved Angle=%.1f MaxYawRate=%.1f Shift=%.3f Drift=%.3f; all other settings preserved"),
			Skill->Motion.DriftTurnDegrees,Skill->Motion.DriftYawRate,Skill->Motion.DurationSeconds,Skill->Motion.DriftSeconds); return 0;
	}
	if(FParse::Param(*Params,TEXT("Quicken30")))
	{
		// Explicit one-skill retune. Absolute targets make reruns idempotent, including
		// fields previously omitted from the package because they matched native defaults.
		auto* Skill=LoadObject<UHALSideShiftConfig>(nullptr,*ConfigPath); if(!Skill) { return 5; }
		auto* Before=DuplicateObject<UHALSideShiftConfig>(Skill,GetTransientPackage());
		Skill->Motion.DurationSeconds=.24f; Skill->Motion.DriftSeconds=.3f;
		Skill->Motion.MaximumAcceleration=100000; Skill->Motion.DriftYawRate=600;
		Skill->Motion.DriftResponse=33.333333f; Skill->Motion.MaximumYawAcceleration=444.44445f;
		Skill->FX.SparkRate=666.6667f; Skill->FX.SparkSpeed=8000; Skill->FX.SparkLifetime=.048f;
		Skill->FX.SmokeRate=116.6667f; Skill->FX.SmokeSpeed=2000; Skill->FX.SmokeLifetime=.12f;
		Skill->FX.FadeSeconds=.018f; Skill->FX.TireFXPlayRate=3.333333f;
		const auto Unchanged=[](UStruct* Type,const void* A,const void* B,const TSet<FName>& Allowed)
		{
			for(TFieldIterator<FProperty> It(Type);It;++It)
			{ if(!Allowed.Contains(It->GetFName())&&!It->Identical_InContainer(A,B)) { UE_LOG(LogTemp,Error,TEXT("Quicken30 changed unauthorized field %s"),*It->GetName()); return false; } }
			return true;
		};
		if(!Unchanged(Skill->GetClass(),Before,Skill,{TEXT("Motion"),TEXT("FX")})
			||!Unchanged(FHALSideShiftMotion::StaticStruct(),&Before->Motion,&Skill->Motion,
				{TEXT("DurationSeconds"),TEXT("DriftSeconds"),TEXT("MaximumAcceleration"),TEXT("DriftYawRate"),TEXT("DriftResponse"),TEXT("MaximumYawAcceleration")})
			||!Unchanged(FHALSideShiftFX::StaticStruct(),&Before->FX,&Skill->FX,
				{TEXT("SparkRate"),TEXT("SparkSpeed"),TEXT("SparkLifetime"),TEXT("SmokeRate"),TEXT("SmokeSpeed"),TEXT("SmokeLifetime"),TEXT("FadeSeconds"),TEXT("TireFXPlayRate")})) { return 6; }
		FString Error; if(!Skill->Validate(Error)) { UE_LOG(LogTemp,Error,TEXT("Quicken30 invalid: %s"),*Error); return 7; }
		if(!ShiftSave(*Skill,false)) { return 8; }
		UE_LOG(LogTemp,Display,TEXT("Quicken30 saved: Shift=%.3fs Drift=%.3fs Distance=%.1f Accel=%.2f Sparks=%.3fs Smoke=%.3fs TireRate=%.3f; all other reflected settings unchanged"),
			Skill->Motion.DurationSeconds,Skill->Motion.DriftSeconds,Skill->Motion.Distance,Skill->Motion.MaximumAcceleration,Skill->FX.SparkLifetime,Skill->FX.SmokeLifetime,Skill->FX.TireFXPlayRate);
		return 0;
	}
	for(const FString& Path:{ShiftRoot+TEXT("M_SideShift_ToonSmoke"),ShiftRoot+TEXT("NS_SideShift_ToonSmoke"),ShiftRoot+TEXT("NS_SideShift_ToonSparks"),ConfigPath})
	{ if(FPackageName::DoesPackageExist(Path)) { UE_LOG(LogTemp,Error,TEXT("Refusing to overwrite existing SideShift asset %s"),*Path); return 1; } }
	auto* P=LoadObject<UHALVehiclePresentationConfig>(nullptr,TEXT("/Game/Data/Car_Presentation/DA_Car_07_Presentation"));
	auto* Source=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Assets/FX/BullCharge/NS_BullCharge_RearSparks"));
	if(!P||!P->Mesh||!Source) { return 2; }
	auto* Material=ShiftSmokeMaterial(); auto* Smoke=ShiftSmoke(*Material);
	auto* Sparks=DuplicateObject<UNiagaraSystem>(Source,CreatePackage(*(ShiftRoot+TEXT("NS_SideShift_ToonSparks"))),TEXT("NS_SideShift_ToonSparks"));
	Sparks->SetFlags(RF_Public|RF_Standalone); Sparks->PostEditChange(); Sparks->RequestCompile(true); Sparks->WaitForCompilationComplete();
	auto* Skill=ShiftAsset<UHALSideShiftConfig>(ConfigPath); Skill->FX.bEnabled=true; Skill->FX.Sparks=Sparks; Skill->FX.Smoke=Smoke;
	Skill->FX.TireMarks=P->TireSkid; Skill->FX.TireMarks.bEnabled=true;
	Skill->FX.TireMarks.System=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Assets/FX/NS_TireSkid"));
	Skill->FX.TireMarks.Wheels={EHALVehicleWheel::FL,EHALVehicleWheel::FR,EHALVehicleWheel::BL,EHALVehicleWheel::BR};
	const FBox Bounds=P->Mesh->GetBounds().GetBox().TransformBy(P->RelativeTransform);
	const FVector Center=Bounds.GetCenter(), Extent=Bounds.GetExtent();
	const auto Mount=[&](double Side)
	{
		FHALVehicleFXMount M;
		const FVector Point(Center.X-Extent.X*.08,Center.Y+Extent.Y*.94*Side,Center.Z-Extent.Z*.15);
		M.Offset=FTransform(FQuat::Identity,Point).GetRelativeTransform(P->RelativeTransform); return M;
	};
	Skill->FX.LeftMounts={Mount(-1)}; Skill->FX.RightMounts={Mount(1)};
	FString Error; if(!Skill->Validate(Error)||!Smoke->IsReadyToRun()||!Sparks->IsReadyToRun())
	{ UE_LOG(LogTemp,Error,TEXT("SideShift art validation failed %s Smoke=%d Sparks=%d"),*Error,Smoke->IsReadyToRun(),Sparks->IsReadyToRun()); return 3; }
	FString Bones=TEXT("Car07 read-only inventory; mounts use initial bounds offsets, not claimed as fitted sockets.\n")+Bounds.ToString()+TEXT("\n");
	for(const auto& Bone:P->Mesh->GetRefSkeleton().GetRefBoneInfo()) { Bones+=Bone.Name.ToString()+TEXT("\n"); }
	FFileHelper::SaveStringToFile(Bones,*(FPaths::ProjectSavedDir()+TEXT("Car07SideShift/20261116/Car07_BonesAndBounds.txt")));
	if(!ShiftSave(*Material)||!ShiftSave(*Smoke)||!ShiftSave(*Sparks)||!ShiftSave(*Skill)) { return 4; }
	UE_LOG(LogTemp,Display,TEXT("Created native Car07 skill DA, independent cartoon smoke/sparks. Existing BP/Definition/presentation/effects untouched. Left=%s Right=%s"),
		*Skill->FX.LeftMounts[0].Offset.ToHumanReadableString(),*Skill->FX.RightMounts[0].Offset.ToHumanReadableString());
	return 0;
}
