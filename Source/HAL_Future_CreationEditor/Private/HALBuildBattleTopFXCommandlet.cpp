#include "HALBuildBattleTopFXCommandlet.h"
#include "NiagaraEmitter.h"
#include "NiagaraSystem.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraEmitterFactoryNew.h"
#include "NiagaraSystemFactoryNew.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraSpriteRendererProperties.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionParticleColor.h"
#include "Materials/MaterialExpressionParticleRelativeTime.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "HALBattleTopAbility.h"
#include "Misc/Parse.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr const TCHAR* Root = TEXT("/Game/Assets/FX/BattleTop");
	template <class T> T* Asset(const TCHAR* Name)
	{
		const FString Path = FString(Root) + TEXT("/") + Name;
		return NewObject<T>(CreatePackage(*Path), Name, RF_Public | RF_Standalone);
	}
	template <class T> T* Node(UMaterial* M)
	{
		auto* N = NewObject<T>(M);
		M->GetExpressionCollection().AddExpression(N);
		return N;
	}
	UMaterial* Material(const TCHAR* Name, bool bWind, bool bSpray = false)
	{
		auto* M = Asset<UMaterial>(Name);
		M->BlendMode = BLEND_Translucent;
		M->SetShadingModel(MSM_Unlit);
		M->TwoSided = true;
		M->SetMaterialUsage(MATUSAGE_NiagaraSprites);
		auto* UV = Node<UMaterialExpressionTextureCoordinate>(M);
		auto* H = Node<UMaterialExpressionCustom>(M);
		H->OutputType = CMOT_Float4;
		const auto Input = [&](const TCHAR* N, UMaterialExpression* E, int32 OutputIndex = 0) {
			FCustomInput I;
			I.InputName = N;
			I.Input.Expression = E;
			I.Input.OutputIndex = OutputIndex;
			H->Inputs.Add(I);
		};
		Input(TEXT("UV"), UV);
		const auto Scalar = [&](const TCHAR* N, float V) {
			auto* P = Node<UMaterialExpressionScalarParameter>(M);
			P->ParameterName = N;
			P->DefaultValue = V;
			Input(N, P);
		};
		const auto Color = [&](const TCHAR* N, FLinearColor V) {
			auto* P = Node<UMaterialExpressionVectorParameter>(M);
			P->ParameterName = N;
			P->DefaultValue = V;
			Input(N, P);
		};
		if (bWind)
		{
			Scalar(TEXT("SpinAngle"), 0);
			Scalar(TEXT("Envelope"), 1);
			Scalar(TEXT("Opacity"), .55f);
			Color(TEXT("MainColor"), FLinearColor(1, .25f, .025f));
			Color(TEXT("AccentColor"), FLinearColor(1, .055f, .025f));
			Color(TEXT("HighlightColor"), FLinearColor(1, .72f, .22f));
			H->Code = TEXT("float2 p=UV*2-1; float r=length(p); float a=atan2(p.y,p.x)/6.2831853-SpinAngle; float w=0; "
						   "float fine=0; "
						   "for(int i=0;i<3;i++){ float rr=0.65+i*0.13+0.018*sin(a*31.4159+i*2); float "
						   "arc=pow(saturate(0.5+0.5*sin(a*18.8496+i*2.1)),3); w+=exp(-pow((r-rr)/0.055,2))*arc*0.48; "
						   "fine+=exp(-pow((r-rr-0.02)/0.012,2))*pow(arc,2)*0.65; } float3 "
						   "c=lerp(MainColor.rgb,AccentColor.rgb,saturate(0.5+0.5*sin(a*12.566))); "
						   "c=lerp(c,HighlightColor.rgb,saturate(fine)); return "
						   "float4(c*1.8,saturate((w+fine)*Envelope*Opacity));");
		}
		else
		{
			Input(TEXT("Tint"), Node<UMaterialExpressionParticleColor>(M), 5);
			Input(TEXT("Age"), Node<UMaterialExpressionParticleRelativeTime>(M));
			H->Code = TEXT("float2 p=abs(UV*2-1); float m=pow(saturate(1-p.x),2)*saturate(1-p.y); return "
						   "float4(Tint.rgb*3,m*Tint.a*pow(saturate(1-Age),2));");
			if (bSpray)
			{
				H->Code =
					TEXT("float2 p=abs(UV*2-1); float edge=saturate(1-p.x); float tail=saturate(1-p.y); "
						 "float3 hot=lerp(Tint.rgb,float3(1.6,1.35,0.75),pow(edge,6)); "
						 "float3 c=lerp(hot,float3(1,0.12,0.015),saturate(Age)*0.7); "
						 "return "
						 "float4(c*4*(1-0.5*saturate(Age)),pow(edge,1.2)*sqrt(tail)*Tint.a*pow(saturate(1-Age),1.1));");
			}
		}
		auto* RGB = Node<UMaterialExpressionComponentMask>(M);
		RGB->R = RGB->G = RGB->B = true;
		RGB->A = false;
		RGB->Input.Expression = H;
		auto* A = Node<UMaterialExpressionComponentMask>(M);
		A->R = A->G = A->B = false;
		A->A = true;
		A->Input.Expression = H;
		M->GetEditorOnlyData()->EmissiveColor.Expression = RGB;
		M->GetEditorOnlyData()->Opacity.Expression = A;
		M->PostEditChange();
		return M;
	}
	UNiagaraNodeFunctionCall* Module(UNiagaraNodeOutput* Output, const TCHAR* Path)
	{
		return FNiagaraStackGraphUtilities::AddScriptModuleToStack(LoadObject<UNiagaraScript>(nullptr, Path), *Output);
	}
	void Link(UNiagaraNodeFunctionCall& Node, const FString& Input, FNiagaraTypeDefinition Type,
			  const FNiagaraVariable& User)
	{
		const auto Handle = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(
			FNiagaraParameterHandle::CreateModuleParameterHandle(FName(*Input)), &Node);
		auto& Pin =
			FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(Node, Handle, Type, FGuid(), FGuid());
		FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(Pin, User, {});
	}
	void RandomRange(UNiagaraNodeFunctionCall& Node, const TCHAR* Input, FNiagaraTypeDefinition Type,
					 const TCHAR* ScriptPath, const FNiagaraVariable& Min, const FNiagaraVariable& Max)
	{
		const auto Handle = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(
			FNiagaraParameterHandle::CreateModuleParameterHandle(Input), &Node);
		auto& Pin =
			FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(Node, Handle, Type, FGuid(), FGuid());
		UNiagaraNodeFunctionCall* Random = nullptr;
		FNiagaraStackGraphUtilities::SetDynamicInputForFunctionInput(
			Pin, LoadObject<UNiagaraScript>(nullptr, ScriptPath), Random);
		check(Random);
		Link(*Random, TEXT("Minimum"), Type, Min);
		Link(*Random, TEXT("Maximum"), Type, Max);
	}
	template <class T> FNiagaraVariable User(UNiagaraSystem* S, FNiagaraTypeDefinition Type, const TCHAR* Name, T Value)
	{
		FNiagaraVariable V(Type, Name);
		V.SetValue(Value);
		S->GetExposedParameters().AddParameter(V);
		S->GetExposedParameters().SetParameterData(V.GetData(), V);
		return V;
	}
	UNiagaraSystem* System(const TCHAR* Name, UMaterial* M, bool bWind, bool bSpray = false)
	{
		auto* S = Asset<UNiagaraSystem>(Name);
		UNiagaraSystemFactoryNew::InitializeSystem(S, true);
		const auto Count = User(S, FNiagaraTypeDefinition::GetIntDef(), TEXT("User.BurstCount"),
								bWind	 ? 1
								: bSpray ? 32
										 : 7);
		const auto LoopDuration = User(S, FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.LoopDuration"), 60.f);
		const auto WindSize = User(S, FNiagaraTypeDefinition::GetVec2Def(), TEXT("User.WindSize"), FVector2f(200, 200));
		const auto Tint =
			User(S, FNiagaraTypeDefinition::GetColorDef(), TEXT("User.SparkColor"), FLinearColor(1, .72f, .22f));
		const auto PreviewMotion = HALBattleTopFX::ContactSprayMotion(FHALBattleTopFXConfig(),FVector::UpVector,FVector::RightVector);
		const auto Min = User(S,FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.VelocityMin"),
			bSpray ? FVector3f(PreviewMotion.VelocityMin) : FVector3f(-80,-80,550));
		const auto Max = User(S,FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.VelocityMax"),
			bSpray ? FVector3f(PreviewMotion.VelocityMax) : FVector3f(80,80,1000));
		auto* E = NewObject<UNiagaraEmitter>(S, TEXT("WarmFlow"));
		UNiagaraEmitterFactoryNew::InitializeEmitter(E, false);
		auto* Data = E->GetLatestEmitterData();
		Data->bLocalSpace = true;
		if (bSpray)
		{
			Data->bDeterminism = true;
		}
		Data->SimTarget = ENiagaraSimTarget::CPUSim;
		auto* Graph = CastChecked<UNiagaraScriptSource>(Data->GraphSource)->NodeGraph.Get();
		const auto Output = [&](ENiagaraScriptUsage Usage) {
			for (const auto& GraphNode : Graph->Nodes)
			{
				if (auto* O = Cast<UNiagaraNodeOutput>(GraphNode.Get()); O && O->GetUsage() == Usage)
				{
					return O;
				}
			}
			return static_cast<UNiagaraNodeOutput*>(nullptr);
		};
		auto* EU = Output(ENiagaraScriptUsage::EmitterUpdateScript);
		auto* PS = Output(ENiagaraScriptUsage::ParticleSpawnScript);
		auto* PU = Output(ENiagaraScriptUsage::ParticleUpdateScript);
		auto* State = Module(EU, TEXT("/Niagara/Modules/Emitter/EmitterState.EmitterState"));
		auto* Mode = LoadObject<UEnum>(
			nullptr, TEXT("/Niagara/Enums/ENiagaraEmitterLifeCycleMode.ENiagaraEmitterLifeCycleMode"));
		for (auto* Pin : State->Pins)
		{
			if (Pin->PinName != TEXT("Life Cycle Mode"))
			{
				continue;
			}
			for (int32 I = 0; I < Mode->NumEnums(); ++I)
			{
				if (Mode->GetDisplayNameTextByIndex(I).ToString() == TEXT("Self"))
				{
					Pin->DefaultValue = Mode->GetNameStringByIndex(I);
				}
			}
		}
		Link(*State, TEXT("Loop Duration"), FNiagaraTypeDefinition::GetFloatDef(), LoopDuration);
		auto* Burst = Module(EU, TEXT("/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous"));
		Link(*Burst, TEXT("Spawn Count"), FNiagaraTypeDefinition::GetIntDef(), Count);
		Module(PS, TEXT("/Niagara/Modules/Spawn/Location/SystemLocation.SystemLocation"));
		TArray<FNiagaraVariable> Vars = {
			FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("Particles.SpriteSize")),
			FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Particles.Lifetime")),
			FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), TEXT("Particles.Color")),
			FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Particles.SpriteFacing")),
			FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Particles.SpriteAlignment"))};
		TArray<FString> Defaults = {bWind ? TEXT("(X=200,Y=200)") : TEXT("(X=3,Y=18)"),
									bWind ? TEXT("60") : TEXT("0.22"), TEXT("(R=1,G=0.72,B=0.22,A=1)"),
									TEXT("(X=0,Y=0,Z=1)"), TEXT("(X=1,Y=0,Z=0)")};
		auto* Set = FNiagaraStackGraphUtilities::AddParameterModuleToStack(Vars, *PS, INDEX_NONE, Defaults);
		Link(*Set, TEXT("Particles.Color"), FNiagaraTypeDefinition::GetColorDef(), Tint);
		if (bWind)
		{
			Link(*Set, TEXT("Particles.SpriteSize"), FNiagaraTypeDefinition::GetVec2Def(), WindSize);
		}
		if (bSpray)
		{
			const auto SizeMin = User(S, FNiagaraTypeDefinition::GetVec2Def(), TEXT("User.SizeMin"), FVector2f(12, 40));
			const auto SizeMax = User(S, FNiagaraTypeDefinition::GetVec2Def(), TEXT("User.SizeMax"), FVector2f(22, 85));
			const auto LifeMin = User(S, FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.LifetimeMin"), .3f);
			const auto LifeMax = User(S, FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.LifetimeMax"), .55f);
			RandomRange(*Set, TEXT("Particles.SpriteSize"), FNiagaraTypeDefinition::GetVec2Def(),
						TEXT("/Niagara/DynamicInputs/UniformRange/V2/RandomRangeVector2D.RandomRangeVector2D"), SizeMin,
						SizeMax);
			RandomRange(*Set, TEXT("Particles.Lifetime"), FNiagaraTypeDefinition::GetFloatDef(),
						TEXT("/Niagara/DynamicInputs/UniformRange/V2/RandomRangeFloat.RandomRangeFloat"), LifeMin,
						LifeMax);
			FNiagaraStackGraphUtilities::AddParameterModuleToStack(
				{FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Particles.Mass"))}, *PS, INDEX_NONE,
				{TEXT("1")});
		}
		if (!bWind)
		{
			auto* Velocity = Module(PS, TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocity.AddVelocity"));
			const auto Handle = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(
				FNiagaraParameterHandle::CreateModuleParameterHandle(TEXT("Velocity")), Velocity);
			auto& Pin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(
				*Velocity, Handle, FNiagaraTypeDefinition::GetVec3Def(), FGuid(), FGuid());
			UNiagaraNodeFunctionCall* Random = nullptr;
			FNiagaraStackGraphUtilities::SetDynamicInputForFunctionInput(
				Pin,
				LoadObject<UNiagaraScript>(
					nullptr, TEXT("/Niagara/DynamicInputs/UniformRange/V2/RandomRangeVector.RandomRangeVector")),
				Random);
			Link(*Random, TEXT("Minimum"), FNiagaraTypeDefinition::GetVec3Def(), Min);
			Link(*Random, TEXT("Maximum"), FNiagaraTypeDefinition::GetVec3Def(), Max);
		}
		Module(PU, TEXT("/Niagara/Modules/Update/Lifetime/ParticleState.ParticleState"));
		if (bSpray)
		{
			const auto Gravity =
				User(S, FNiagaraTypeDefinition::GetVec3Def(), TEXT("User.Gravity"), FVector3f(0, 0, -980));
			const auto Drag = User(S, FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.Drag"), 1.8f);
			// These are simulation-space forces, matching the world-down vector transformed by the burst frame.
			auto* Force = FNiagaraStackGraphUtilities::AddParameterModuleToStack(
				{FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Physics.Force"))}, *PU, INDEX_NONE,
				{TEXT("(X=0,Y=0,Z=-980)")});
			Link(*Force, TEXT("Physics.Force"), FNiagaraTypeDefinition::GetVec3Def(), Gravity);
			auto* DragModule = Module(PU, TEXT("/Niagara/Modules/Update/Forces/Drag.Drag"));
			Link(*DragModule, TEXT("Drag"), FNiagaraTypeDefinition::GetFloatDef(), Drag);
		}
		Module(PU, TEXT("/Niagara/Modules/Solvers/SolveForcesAndVelocity.SolveForcesAndVelocity"));
		auto* R = NewObject<UNiagaraSpriteRendererProperties>(E);
		R->Material = M;
		R->Alignment = bWind ? ENiagaraSpriteAlignment::CustomAlignment : ENiagaraSpriteAlignment::VelocityAligned;
		R->FacingMode = bWind ? ENiagaraSpriteFacingMode::CustomFacingVector : ENiagaraSpriteFacingMode::FaceCamera;
		if (bWind)
		{
			FNiagaraVariable V(FNiagaraTypeDefinition(UMaterialInterface::StaticClass()), TEXT("User.WindMaterial"));
			S->GetExposedParameters().AddParameter(V);
			S->GetExposedParameters().SetUObject(M, V);
			R->MaterialUserParamBinding.Parameter = V;
		}
		E->AddRenderer(R, Data->Version.VersionGuid);
		FNiagaraEditorUtilities::AddEmitterToSystem(*S, *E, E->GetExposedVersion().VersionGuid);
		S->RequestCompile(false);
		S->WaitForCompilationComplete();
		return S;
	}
	int32 ApplyContactSpray(const FString& Params)
	{
		FString Path;
		if (!FParse::Value(*Params, TEXT("SkillAsset="), Path))
		{
			UE_LOG(LogTemp, Error, TEXT("ApplyContactSpray requires an explicit -SkillAsset=/Game/... path"));
			return 4;
		}
		auto* Skill = LoadObject<UHALBattleTopConfig>(nullptr, *Path);
		auto* Spray = LoadObject<UNiagaraSystem>(
			nullptr, TEXT("/Game/Assets/FX/BattleTop/NS_BattleTop_ContactSpray.NS_BattleTop_ContactSpray"));
		if (!Skill || !Spray || !Spray->IsReadyToRun())
		{
			UE_LOG(LogTemp, Error, TEXT("Skill or compiled contact spray is unavailable"));
			return 5;
		}
		auto* Before = DuplicateObject<UHALBattleTopConfig>(Skill, GetTransientPackage());
		Skill->Modify();
		Skill->FX.ContactSparks = Spray;
		Skill->FX.ContactCount = 32;
		FString Error;
		if (!Skill->Validate(Error))
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid skill: %s"), *Error);
			return 6;
		}
		for (TFieldIterator<FProperty> It(Skill->GetClass()); It; ++It)
		{
			if (It->GetFName() != TEXT("FX") && !It->Identical_InContainer(Before, Skill))
			{
				UE_LOG(LogTemp, Error, TEXT("Unexpected non-FX edit: %s"), *It->GetName());
				return 7;
			}
		}
		for (TFieldIterator<FProperty> It(FHALBattleTopFXConfig::StaticStruct()); It; ++It)
		{
			if (It->GetFName() != TEXT("ContactSparks") && It->GetFName() != TEXT("ContactCount") &&
				!It->Identical_InContainer(&Before->FX, &Skill->FX))
			{
				UE_LOG(LogTemp, Error, TEXT("Unexpected FX edit: %s"), *It->GetName());
				return 7;
			}
		}
		auto* Package = Skill->GetPackage();
		Package->MarkPackageDirty();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		const auto File =
			FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		if (!UPackage::SavePackage(Package, Skill, *File, Args))
		{
			return 8;
		}
		UE_LOG(LogTemp, Display, TEXT("Contact spray applied: %s; only FX.ContactSparks and FX.ContactCount changed"),
			   *Skill->GetPathName());
		return 0;
	}
} // namespace
UHALBuildBattleTopFXCommandlet::UHALBuildBattleTopFXCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}
int32 UHALBuildBattleTopFXCommandlet::Main(const FString& Params)
{
	if (FParse::Param(*Params, TEXT("ApplyContactSpray")))
	{
		return ApplyContactSpray(Params);
	}
	const bool bSpray = FParse::Param(*Params, TEXT("ContactSpray"));
	const TArray<FString> Names =
		bSpray ? TArray<FString>{TEXT("M_BattleTop_ContactSpray"), TEXT("NS_BattleTop_ContactSpray")}
			   : TArray<FString>{TEXT("M_BattleTop_Wind"), TEXT("M_BattleTop_Spark"), TEXT("NS_BattleTop_Wind"),
								 TEXT("NS_BattleTop_Sparks")};
	for (const auto& Name : Names)
	{
		if (FPackageName::DoesPackageExist(FString(Root) + TEXT("/") + Name))
		{
			UE_LOG(LogTemp, Error, TEXT("Refusing to overwrite existing BattleTop FX asset %s"), *Name);
			return 1;
		}
	}
	TArray<UObject*> Assets;
	if (bSpray)
	{
		auto* M = Material(TEXT("M_BattleTop_ContactSpray"), false, true);
		Assets = {M, System(TEXT("NS_BattleTop_ContactSpray"), M, false, true)};
	}
	else
	{
		auto* Wind = Material(TEXT("M_BattleTop_Wind"), true);
		auto* Spark = Material(TEXT("M_BattleTop_Spark"), false);
		Assets = {Wind, Spark, System(TEXT("NS_BattleTop_Wind"), Wind, true),
				  System(TEXT("NS_BattleTop_Sparks"), Spark, false)};
	}
	for (auto* A : Assets)
	{
		if (auto* S = Cast<UNiagaraSystem>(A); S && !S->IsReadyToRun())
		{
			UE_LOG(LogTemp, Error, TEXT("Niagara did not compile: %s"), *S->GetName());
			return 2;
		}
	}
	for (auto* A : Assets)
	{
		FAssetRegistryModule::AssetCreated(A);
		auto* P = A->GetPackage();
		P->MarkPackageDirty();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		const FString File =
			FPackageName::LongPackageNameToFilename(P->GetName(), FPackageName::GetAssetPackageExtension());
		if (!UPackage::SavePackage(P, A, *File, Args))
		{
			return 3;
		}
		UE_LOG(LogTemp, Display, TEXT("BattleTop FX created %s"), *A->GetPathName());
	}
	return 0;
}
