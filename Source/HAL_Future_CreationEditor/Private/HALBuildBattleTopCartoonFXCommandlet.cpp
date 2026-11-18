#include "HALBuildBattleTopCartoonFXCommandlet.h"
#include "HALBattleTopAbility.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraSystemFactoryNew.h"
#include "NiagaraEmitterFactoryNew.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraEditorUtilities.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionParticleColor.h"
#include "Materials/MaterialExpressionParticleSize.h"
#include "Materials/MaterialExpressionParticleRelativeTime.h"
#include "Materials/MaterialExpressionDynamicParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr const TCHAR* Root = TEXT("/Game/Assets/FX/BattleTop/");
	template<class T> T* Asset(const TCHAR* Name)
	{
		return NewObject<T>(CreatePackage(*(FString(Root)+Name)), Name, RF_Public|RF_Standalone);
	}
	template<class T> T* Node(UMaterial& M)
	{
		auto* N=NewObject<T>(&M); M.GetExpressionCollection().AddExpression(N); return N;
	}
	void Input(UMaterialExpressionCustom& H, const TCHAR* Name, UMaterialExpression* E, int32 Output=0)
	{
		auto& I=H.Inputs.AddDefaulted_GetRef(); I.InputName=Name; I.Input.Connect(Output,E);
	}
	void Scalar(UMaterial& M, UMaterialExpressionCustom& H, const TCHAR* Name, float Default)
	{
		auto* P=Node<UMaterialExpressionScalarParameter>(M); P->ParameterName=Name; P->DefaultValue=Default;
		Input(H,Name,P);
	}
	void Color(UMaterial& M, UMaterialExpressionCustom& H, const TCHAR* Name, FLinearColor Default)
	{
		auto* P=Node<UMaterialExpressionVectorParameter>(M); P->ParameterName=Name; P->DefaultValue=Default;
		Input(H,Name,P);
	}
	UMaterial* Material(bool bWind)
	{
		auto* M=Asset<UMaterial>(bWind?TEXT("M_BattleTop_ToonWind"):TEXT("M_BattleTop_ToonContact"));
		M->BlendMode=BLEND_Translucent; M->SetShadingModel(MSM_Unlit); M->TwoSided=!bWind;
		M->SetMaterialUsage(bWind?MATUSAGE_NiagaraMeshParticles:MATUSAGE_NiagaraSprites);
		auto* H=Node<UMaterialExpressionCustom>(*M); H->OutputType=CMOT_Float4;
		Input(*H,TEXT("UV"),Node<UMaterialExpressionTextureCoordinate>(*M));
		Input(*H,TEXT("Age"),Node<UMaterialExpressionParticleRelativeTime>(*M));
		if(bWind)
		{
			Input(*H,TEXT("N"),Node<UMaterialExpressionVertexNormalWS>(*M));
			Scalar(*M,*H,TEXT("Envelope"),1); Scalar(*M,*H,TEXT("Opacity"),.55f);
			Scalar(*M,*H,TEXT("GlowStrength"),.8f);
			Color(*M,*H,TEXT("MainColor"),FLinearColor(1,.25f,.025f));
			Color(*M,*H,TEXT("AccentColor"),FLinearColor(1,.055f,.025f));
			Color(*M,*H,TEXT("HighlightColor"),FLinearColor(1,.72f,.22f));
			H->Code=TEXT("float f=saturate(dot(normalize(N),normalize(float3(.3,-.35,.89)))*.5+.5); "
				"float3 c=lerp(lerp(AccentColor.rgb*.65,MainColor.rgb,smoothstep(.42,.48,f)),HighlightColor.rgb,smoothstep(.78,.84,f)); "
				"float life=smoothstep(0,.12,Age)*(1-smoothstep(.68,1,Age)); "
				"return float4(c*max(GlowStrength,0),saturate(Opacity*Envelope*life));");
			// Vertex colors encode the mesh-local radial offset. This transform uses each Niagara mesh instance.
			auto* Offset=Node<UMaterialExpressionCustom>(*M); Offset->OutputType=CMOT_Float3;
			Input(*Offset,TEXT("V"),Node<UMaterialExpressionVertexColor>(*M));
			Scalar(*M,*Offset,TEXT("BandWidthScale"),1);
			Offset->Code=TEXT("#if VERTEXSHADER\nreturn TransformLocalVectorToWorld(Parameters,float3((V.rg*2-1)*12*(clamp(BandWidthScale,.35,2.5)-1),0));\n#else\nreturn float3(0,0,0);\n#endif");
			M->GetEditorOnlyData()->WorldPositionOffset.Connect(0,Offset);
		}
		else
		{
			Input(*H,TEXT("Tint"),Node<UMaterialExpressionParticleColor>(*M),5);
			Input(*H,TEXT("Size"),Node<UMaterialExpressionParticleSize>(*M));
			Input(*H,TEXT("Control"),Node<UMaterialExpressionDynamicParameter>(*M));
			H->Code=TEXT("float2 p=UV*2-1; float life=smoothstep(0,.09,Age)*(1-smoothstep(.68,1,Age)); "
				"p/=max(life,.001); float random=frac(sin(dot(Size,float2(12.9898,78.233)))*43758.5453); "
				"p.x-=.18*sin(p.y*2.2); float2 q=abs(p)-float2(.49,.25); "
				"float d=length(max(q,0))+min(max(q.x,q.y),0)-.28; "
				"if(random>.90)d=length(p)-.4; else if(random>.79)d=(abs(p.x)+abs(p.y))*.707-.47; "
				"float mask=1-smoothstep(-.015,.035,d); float shade=saturate(p.x*.45+p.y*.25+.5); "
				"float3 mid=lerp(Tint.rgb,float3(1,.38,.045),.22); "
				"float3 c=lerp(lerp(mid*.58,mid,smoothstep(.28,.32,shade)),lerp(mid,float3(1,.96,.76),.3),smoothstep(.74,.78,shade)); "
				"return float4(c*max(Control.x,0),mask*Tint.a*.95);");
		}
		auto* RGB=Node<UMaterialExpressionComponentMask>(*M); RGB->R=RGB->G=RGB->B=true; RGB->A=false; RGB->Input.Connect(0,H);
		auto* A=Node<UMaterialExpressionComponentMask>(*M); A->R=A->G=A->B=false; A->A=true; A->Input.Connect(0,H);
		M->GetEditorOnlyData()->EmissiveColor.Connect(0,RGB); M->GetEditorOnlyData()->Opacity.Connect(0,A);
		M->PostEditChange(); return M;
	}
	UStaticMesh* ArcMesh(UMaterial& Material)
	{
		auto* Mesh=Asset<UStaticMesh>(TEXT("SM_BattleTop_WindArc"));
		FMeshDescription D; FStaticMeshAttributes A(D); A.Register();
		auto P=A.GetVertexPositions(); auto Normals=A.GetVertexInstanceNormals(); auto Tangents=A.GetVertexInstanceTangents();
		auto Signs=A.GetVertexInstanceBinormalSigns(); auto Colors=A.GetVertexInstanceColors(); auto UV=A.GetVertexInstanceUVs(); UV.SetNumChannels(1);
		constexpr int32 Along=32, Around=8;
		TArray<FVertexID> IDs; TArray<FVector3f> Ns,Ts; TArray<FVector4f> Cs; TArray<FVector2f> UVs;
		for(int32 I=0;I<=Along;++I)
		{
			const double U=double(I)/Along, Angle=FMath::DegreesToRadians(-55.+110.*U);
			const FVector3f R(FMath::Cos(Angle),FMath::Sin(Angle),0), T(-R.Y,R.X,0);
			const float Taper=.15f+.85f*FMath::Sqrt(FMath::Sin(UE_DOUBLE_PI*U));
			for(int32 J=0;J<Around;++J)
			{
				const double V=2*UE_DOUBLE_PI*J/Around;
				const FVector3f Offset=R*(12*Taper*FMath::Cos(V));
				const FVertexID ID=D.CreateVertex(); P[ID]=R*100+Offset+FVector3f(0,0,4*Taper*FMath::Sin(V)); IDs.Add(ID);
				Ns.Add((R*float(FMath::Cos(V)/12)+FVector3f(0,0,FMath::Sin(V)/4)).GetSafeNormal()); Ts.Add(T);
				Cs.Add(FVector4f(.5f+Offset.X/24,.5f+Offset.Y/24,0,1)); UVs.Add(FVector2f(U,float(J)/Around));
			}
		}
		const auto Group=D.CreatePolygonGroup(); A.GetPolygonGroupMaterialSlotNames()[Group]=TEXT("Airflow");
		const auto Triangle=[&](int32 X,int32 Y,int32 Z){
			TArray<FVertexInstanceID,TInlineAllocator<3>> Is;
			for(int32 V:{X,Y,Z}){auto ID=D.CreateVertexInstance(IDs[V]);Is.Add(ID);Normals[ID]=Ns[V];Tangents[ID]=Ts[V];Signs[ID]=1;Colors[ID]=Cs[V];UV.Set(ID,0,UVs[V]);}
			D.CreateTriangle(Group,Is);
		};
		for(int32 I=0;I<Along;++I)for(int32 J=0;J<Around;++J){int32 X=I*Around+J,Y=I*Around+(J+1)%Around,Z=(I+1)*Around+J,W=(I+1)*Around+(J+1)%Around;Triangle(X,Y,W);Triangle(X,W,Z);}
		Mesh->GetStaticMaterials().Add(FStaticMaterial(&Material,TEXT("Airflow"),TEXT("Airflow")));
		Mesh->SetNumSourceModels(1); auto& B=Mesh->GetSourceModel(0).BuildSettings; B.bRecomputeNormals=false;B.bRecomputeTangents=false;B.bGenerateLightmapUVs=false;
		UStaticMesh::FBuildMeshDescriptionsParams Params;Params.bBuildSimpleCollision=false;Params.bCommitMeshDescription=true;Params.bMarkPackageDirty=false;
		return Mesh->BuildFromMeshDescriptions({&D},Params)?Mesh:nullptr;
	}
	UNiagaraNodeFunctionCall* Module(UNiagaraNodeOutput* Out,const TCHAR* Path)
	{
		return FNiagaraStackGraphUtilities::AddScriptModuleToStack(LoadObject<UNiagaraScript>(nullptr,Path),*Out);
	}
	void Link(UNiagaraNodeFunctionCall& N,const TCHAR* Name,FNiagaraTypeDefinition Type,const FNiagaraVariable& Value)
	{
		const auto H=FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle::CreateModuleParameterHandle(Name),&N);
		auto& Pin=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(N,H,Type,FGuid(),FGuid());
		FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(Pin,Value,{});
	}
	template<class T> FNiagaraVariable User(UNiagaraSystem& S,FNiagaraTypeDefinition Type,const TCHAR* Name,T Value)
	{
		FNiagaraVariable V(Type,Name); S.GetExposedParameters().AddParameter(V); S.GetExposedParameters().SetParameterValue(Value,V); return V;
	}
	void Random(UNiagaraNodeFunctionCall& N,const TCHAR* Name,FNiagaraTypeDefinition Type,const FNiagaraVariable& Min,const FNiagaraVariable& Max)
	{
		const auto H=FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle::CreateModuleParameterHandle(Name),&N);
		auto& Pin=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(N,H,Type,FGuid(),FGuid());
		UNiagaraNodeFunctionCall* R=nullptr;
		FNiagaraStackGraphUtilities::SetDynamicInputForFunctionInput(Pin,LoadObject<UNiagaraScript>(nullptr,TEXT("/Niagara/DynamicInputs/UniformRange/V2/RandomRangeVector.RandomRangeVector")),R);
		Link(*R,TEXT("Minimum"),FNiagaraTypeDefinition::GetVec3Def(),Min);Link(*R,TEXT("Maximum"),FNiagaraTypeDefinition::GetVec3Def(),Max);
	}
	UNiagaraNodeOutput* Output(UNiagaraGraph& G, ENiagaraScriptUsage Usage)
	{
		for(const auto& N:G.Nodes) if(auto* O=Cast<UNiagaraNodeOutput>(N.Get());O&&O->GetUsage()==Usage)return O;
		return nullptr;
	}
	UNiagaraSystem* ContactSystem(UMaterial& Material)
	{
		auto* Source=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Assets/FX/BattleTop/NS_BattleTop_ContactSpray.NS_BattleTop_ContactSpray"));
		if(!Source)return nullptr;
		auto* S=DuplicateObject<UNiagaraSystem>(Source,CreatePackage(*(FString(Root)+TEXT("NS_BattleTop_ToonContact"))),TEXT("NS_BattleTop_ToonContact"));
		S->SetFlags(RF_Public|RF_Standalone);
		const auto Style=User(*S,FNiagaraTypeDefinition::GetVec4Def(),TEXT("User.StyleParameters"),FVector4f(1,0,0,0));
		for(auto& Handle:S->GetEmitterHandles())
		{
			auto* E=Handle.GetInstance().Emitter.Get(); if(!E)continue;
			auto* Data=E->GetLatestEmitterData();
			for(auto* R:Data->GetRenderers())if(auto* Sprite=Cast<UNiagaraSpriteRendererProperties>(R))Sprite->Material=&Material;
			auto* G=CastChecked<UNiagaraScriptSource>(Data->GraphSource)->NodeGraph.Get();
			auto* Set=FNiagaraStackGraphUtilities::AddParameterModuleToStack(
				{FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(),TEXT("Particles.DynamicMaterialParameter"))},
				*Output(*G,ENiagaraScriptUsage::ParticleSpawnScript),INDEX_NONE,{TEXT("(X=1,Y=0,Z=0,W=0)")});
			Link(*Set,TEXT("Particles.DynamicMaterialParameter"),FNiagaraTypeDefinition::GetVec4Def(),Style);
		}
		S->PostEditChange();S->RequestCompile(true);S->WaitForCompilationComplete(); return S;
	}
	bool SetEnumInput(UNiagaraNodeFunctionCall& Node,FName Name,const TCHAR* DisplayName)
	{
		for(auto* Pin:Node.Pins)if(Pin->PinName==Name)
		{
			if(auto* Enum=Cast<UEnum>(Pin->PinType.PinSubCategoryObject.Get()))
				for(int32 I=0;I<Enum->NumEnums();++I)if(Enum->GetDisplayNameTextByIndex(I).ToString()==DisplayName)
				{
					Node.Modify();Pin->DefaultValue=Enum->GetNameStringByIndex(I);
					Node.MarkNodeRequiresSynchronization(TEXT("Constrain BattleTop wind axes"),true);return true;
				}
		}
		UE_LOG(LogTemp,Error,TEXT("Missing Niagara enum input %s / %s"),*Name.ToString(),DisplayName);return false;
	}
	UNiagaraNodeFunctionCall* Dynamic(UNiagaraNodeFunctionCall& Node,FName Name,FNiagaraTypeDefinition Type,const TCHAR* Path)
	{
		const auto Handle=FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle::CreateModuleParameterHandle(Name),&Node);
		auto& Pin=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(Node,Handle,Type,FGuid(),FGuid());
		UNiagaraNodeFunctionCall* Result=nullptr;
		FNiagaraStackGraphUtilities::SetDynamicInputForFunctionInput(Pin,LoadObject<UNiagaraScript>(nullptr,Path),Result);return Result;
	}
	bool ConstrainWindAxes(UNiagaraSystem& S)
	{
		int32 Fixed=0;
		for(auto& H:S.GetEmitterHandles())if(auto* E=H.GetInstance().Emitter.Get())
		{
			auto* G=CastChecked<UNiagaraScriptSource>(E->GetLatestEmitterData()->GraphSource)->NodeGraph.Get();
			bool bHasVerticalRotation=false,bHasIndependentTilt=false;
			for(auto& N:G->Nodes)
			{
				bHasVerticalRotation|=N->NodeComment==TEXT("HAL BattleTop Vertical Airflow v1");
				bHasIndependentTilt|=N->NodeComment==TEXT("HAL BattleTop Independent Tilt v2");
				if(auto* Call=Cast<UNiagaraNodeFunctionCall>(N.Get());Call&&Call->FunctionScript)
				{
					if(Call->FunctionScript->GetPathName().Contains(TEXT("/V3/InitialMeshOrientation.")))
					{
						if(!SetEnumInput(*Call,TEXT("Mesh Orientation Mode"),TEXT("None"))
							||!SetEnumInput(*Call,TEXT("Rotation Coordinate Space"),TEXT("Simulation")))return false;
						Call->SetEnabledState(ENodeEnabledState::Disabled,true);
						Call->MarkNodeRequiresSynchronization(TEXT("Replace coupled rotation vector with independent tilt"),true);
						++Fixed;
					}
					else if(Call->FunctionScript->GetName()==TEXT("MeshRotationRate"))
					{
						Call->SetEnabledState(ENodeEnabledState::Disabled,true);
						Call->MarkNodeRequiresSynchronization(TEXT("Replace particle-local spin with vertical spin"),true);
					}
				}
			}
			if(!bHasIndependentTilt)
			{
				// Birth pose = random heading around Z * configured tilt around X. Unlike an axis-angle
				// vector containing yaw, the full heading cannot cancel the configured plane tilt.
				auto* Initial=FNiagaraStackGraphUtilities::AddParameterModuleToStack(
					{FNiagaraVariable(FNiagaraTypeDefinition::GetQuatDef(),TEXT("Particles.InitialRotation"))},
					*Output(*G,ENiagaraScriptUsage::ParticleSpawnScript),INDEX_NONE,{TEXT("(X=0,Y=0,Z=0,W=1)")});
				Initial->NodeComment=TEXT("HAL BattleTop Independent Tilt v2");
				auto* Combined=Dynamic(*Initial,TEXT("Particles.InitialRotation"),FNiagaraTypeDefinition::GetQuatDef(),TEXT("/Niagara/DynamicInputs/Rotation/MultiplyQuaternion.MultiplyQuaternion"));
				const auto AxisPin=[](UNiagaraNodeFunctionCall& N,const TCHAR* Value)
				{
					const auto AxisHandle=FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle::CreateModuleParameterHandle(TEXT("Axis")),&N);
					auto& P=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(N,AxisHandle,FNiagaraTypeDefinition::GetVec3Def(),FGuid(),FGuid());P.DefaultValue=Value;
				};
				auto* Tilt=Dynamic(*Combined,TEXT("Quaternion B"),FNiagaraTypeDefinition::GetQuatDef(),TEXT("/Niagara/DynamicInputs/TypeConversions/MakeQuatFromAxisAngle.MakeQuatFromAxisAngle"));
				AxisPin(*Tilt,TEXT("(X=1,Y=0,Z=0)"));
				Link(*Tilt,TEXT("AngleInDegrees"),FNiagaraTypeDefinition::GetFloatDef(),FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.TiltDegrees")));
				auto* Heading=Dynamic(*Combined,TEXT("Quaternion A"),FNiagaraTypeDefinition::GetQuatDef(),TEXT("/Niagara/DynamicInputs/TypeConversions/MakeQuatFromAxisAngle.MakeQuatFromAxisAngle"));
				AxisPin(*Heading,TEXT("(X=0,Y=0,Z=1)"));
				auto* RandomYaw=Dynamic(*Heading,TEXT("AngleInDegrees"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("/Niagara/DynamicInputs/UniformRange/V2/RandomRangeFloat.RandomRangeFloat"));
				for(const auto& Name:{FName(TEXT("Minimum")),FName(TEXT("Maximum"))})
				{
					const auto RangeHandle=FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle::CreateModuleParameterHandle(Name),RandomYaw);
					auto& P=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*RandomYaw,RangeHandle,FNiagaraTypeDefinition::GetFloatDef(),FGuid(),FGuid());P.DefaultValue=Name==TEXT("Minimum")?TEXT("0"):TEXT("360");
				}
				auto* Pose=FNiagaraStackGraphUtilities::AddParameterModuleToStack(
					{FNiagaraVariable(FNiagaraTypeDefinition::GetQuatDef(),TEXT("Particles.MeshOrientation"))},
					*Output(*G,ENiagaraScriptUsage::ParticleSpawnScript),INDEX_NONE,{TEXT("(X=0,Y=0,Z=0,W=1)")});
				Pose->NodeComment=TEXT("HAL BattleTop Initial Wind Mesh Pose v2");
				Link(*Pose,TEXT("Particles.MeshOrientation"),FNiagaraTypeDefinition::GetQuatDef(),FNiagaraVariable(FNiagaraTypeDefinition::GetQuatDef(),TEXT("Particles.InitialRotation")));
			}
			if(!bHasVerticalRotation)
			{
				// Q = world-yaw(age * speed) * initial-tilt. Multiplying in this order preserves the
				// cone angle and advances around simulation Z, even when the initial arc is tilted.
				auto* Set=FNiagaraStackGraphUtilities::AddParameterModuleToStack(
					{FNiagaraVariable(FNiagaraTypeDefinition::GetQuatDef(),TEXT("Particles.MeshOrientation"))},
					*Output(*G,ENiagaraScriptUsage::ParticleUpdateScript),INDEX_NONE,{TEXT("(X=0,Y=0,Z=0,W=1)")});
				Set->NodeComment=TEXT("HAL BattleTop Vertical Airflow v1");
				auto* Multiply=Dynamic(*Set,TEXT("Particles.MeshOrientation"),FNiagaraTypeDefinition::GetQuatDef(),TEXT("/Niagara/DynamicInputs/Rotation/MultiplyQuaternion.MultiplyQuaternion"));
				Link(*Multiply,TEXT("Quaternion B"),FNiagaraTypeDefinition::GetQuatDef(),FNiagaraVariable(FNiagaraTypeDefinition::GetQuatDef(),TEXT("Particles.InitialRotation")));
				auto* Yaw=Dynamic(*Multiply,TEXT("Quaternion A"),FNiagaraTypeDefinition::GetQuatDef(),TEXT("/Niagara/DynamicInputs/TypeConversions/MakeQuatFromAxisAngle.MakeQuatFromAxisAngle"));
				const auto AxisHandle=FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle::CreateModuleParameterHandle(TEXT("Axis")),Yaw);
				auto& Axis=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*Yaw,AxisHandle,FNiagaraTypeDefinition::GetVec3Def(),FGuid(),FGuid());
				Axis.DefaultValue=TEXT("(X=0,Y=0,Z=1)");
				auto* Angle=Dynamic(*Yaw,TEXT("AngleInDegrees"),FNiagaraTypeDefinition::GetFloatDef(),TEXT("/Niagara/DynamicInputs/Multiply/Multiply_Float.Multiply_Float"));
				Link(*Angle,TEXT("A"),FNiagaraTypeDefinition::GetFloatDef(),FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(),TEXT("Particles.Age")));
				Link(*Angle,TEXT("B"),FNiagaraTypeDefinition::GetFloatDef(),FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.YawRate")));
			}
		}
		const float Tilt=12.f/(360.f*FMath::Sqrt(2.f));
		User(S,FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.TiltDegrees"),12.f);
		S.GetExposedParameters().SetParameterValue(FVector3f(-Tilt,-Tilt,0),FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.RotationMin")));
		S.GetExposedParameters().SetParameterValue(FVector3f(Tilt,Tilt,1),FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.RotationMax")));
		return Fixed==1;
	}
	UNiagaraSystem* WindSystem(UMaterial& Material,UStaticMesh& Mesh)
	{
		auto* S=Asset<UNiagaraSystem>(TEXT("NS_BattleTop_ToonWind"));
		UNiagaraSystemFactoryNew::InitializeSystem(S,true);
		const auto Rate=User(*S,FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.SpawnRate"),5.f/1.2f);
		const auto Life=User(*S,FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.Lifetime"),1.2f);
		const auto Yaw=User(*S,FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.YawRate"),240.f);
		const auto PositionMin=User(*S,FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.PositionMin"),FVector3f(0,0,-11.25));
		const auto PositionMax=User(*S,FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.PositionMax"),FVector3f(0,0,33.75));
		const auto RotationMin=User(*S,FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.RotationMin"),FVector3f(-12.f/360,-12.f/360,0));
		const auto RotationMax=User(*S,FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.RotationMax"),FVector3f(12.f/360,12.f/360,1));
		const auto ScaleMin=User(*S,FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.ScaleMin"),FVector3f(.9f));
		const auto ScaleMax=User(*S,FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.ScaleMax"),FVector3f(1.08f));
		const auto Velocity=User(*S,FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.Velocity"),FVector3f(0,0,18));
		const FNiagaraVariable Mat(FNiagaraTypeDefinition(UMaterialInterface::StaticClass()),TEXT("User.WindMaterial"));
		S->GetExposedParameters().AddParameter(Mat);S->GetExposedParameters().SetUObject(&Material,Mat);
		auto* E=NewObject<UNiagaraEmitter>(S,TEXT("CartoonAirflow"));UNiagaraEmitterFactoryNew::InitializeEmitter(E,false);
		auto* Data=E->GetLatestEmitterData();Data->bLocalSpace=true;Data->bDeterminism=true;Data->SimTarget=ENiagaraSimTarget::CPUSim;
		auto* G=CastChecked<UNiagaraScriptSource>(Data->GraphSource)->NodeGraph.Get();
		auto* EU=Output(*G,ENiagaraScriptUsage::EmitterUpdateScript);
		auto* PS=Output(*G,ENiagaraScriptUsage::ParticleSpawnScript);
		auto* PU=Output(*G,ENiagaraScriptUsage::ParticleUpdateScript);
		auto* State=Module(EU,TEXT("/Niagara/Modules/Emitter/EmitterState.EmitterState"));
		auto* Mode=LoadObject<UEnum>(nullptr,TEXT("/Niagara/Enums/ENiagaraEmitterLifeCycleMode.ENiagaraEmitterLifeCycleMode"));
		for(auto* Pin:State->Pins)if(Pin->PinName==TEXT("Life Cycle Mode"))
			for(int32 I=0;I<Mode->NumEnums();++I)if(Mode->GetDisplayNameTextByIndex(I).ToString()==TEXT("Self"))Pin->DefaultValue=Mode->GetNameStringByIndex(I);
		Link(*Module(EU,TEXT("/Niagara/Modules/Emitter/SpawnRate.SpawnRate")),TEXT("SpawnRate"),FNiagaraTypeDefinition::GetFloatDef(),Rate);
		Module(PS,TEXT("/Niagara/Modules/Spawn/Location/SystemLocation.SystemLocation"));
		auto* Set=FNiagaraStackGraphUtilities::AddParameterModuleToStack({
			FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(),TEXT("Particles.Position")),
			FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(),TEXT("Particles.Scale")),
			FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(),TEXT("Particles.Lifetime")),
			FNiagaraVariable(FNiagaraTypeDefinition::GetQuatDef(),TEXT("Particles.MeshOrientation"))},*PS,INDEX_NONE,
			{TEXT("(X=0,Y=0,Z=0)"),TEXT("(X=1,Y=1,Z=1)"),TEXT("1.2"),TEXT("(X=0,Y=0,Z=0,W=1)")});
		// Position is written as a simulation-space vector; Niagara converts its position pin at the assignment boundary.
		Random(*Set,TEXT("Particles.Position"),FNiagaraTypeDefinition::GetPositionDef(),PositionMin,PositionMax);
		Random(*Set,TEXT("Particles.Scale"),FNiagaraTypeDefinition::GetVec3Def(),ScaleMin,ScaleMax);
		Link(*Set,TEXT("Particles.Lifetime"),FNiagaraTypeDefinition::GetFloatDef(),Life);
		auto* Orientation=Module(PS,TEXT("/Niagara/Modules/Spawn/Orientation/V3/InitialMeshOrientation.InitialMeshOrientation"));
		for(auto* Pin:Orientation->Pins)if(Pin->PinName==TEXT("Use Rotation Vector"))Pin->DefaultValue=TEXT("true");
		Random(*Orientation,TEXT("Rotation"),FNiagaraTypeDefinition::GetVec3Def(),RotationMin,RotationMax);
		Link(*Module(PS,TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocity.AddVelocity")),TEXT("Velocity"),FNiagaraTypeDefinition::GetVec3Def(),Velocity);
		Module(PU,TEXT("/Niagara/Modules/Update/Lifetime/ParticleState.ParticleState"));
		Link(*Module(PU,TEXT("/Niagara/Modules/Update/Orientation/MeshRotationRate.MeshRotationRate")),TEXT("Yaw"),FNiagaraTypeDefinition::GetFloatDef(),Yaw);
		Module(PU,TEXT("/Niagara/Modules/Solvers/SolveForcesAndVelocity.SolveForcesAndVelocity"));
		auto* R=NewObject<UNiagaraMeshRendererProperties>(E);
		// Niagara initializes slot zero; adding another entry leaves the selected mesh empty.
		R->Meshes.Reset();R->Meshes.AddDefaulted_GetRef().Mesh=&Mesh;
		R->bOverrideMaterials=true;auto& Override=R->OverrideMaterials.AddDefaulted_GetRef();Override.ExplicitMat=&Material;Override.UserParamBinding.Parameter=Mat;
		R->MeshBoundsScale=FVector(1.4);E->AddRenderer(R,Data->Version.VersionGuid);
		FNiagaraEditorUtilities::AddEmitterToSystem(*S,*E,E->GetExposedVersion().VersionGuid);
		if(!ConstrainWindAxes(*S))return nullptr;
		S->SetWarmupTickDelta(1.f/30);S->SetWarmupTime(1.2f);S->ResolveWarmupTickCount();
		S->PostEditChange();S->RequestCompile(true);S->WaitForCompilationComplete();return S;
	}
	bool Save(UObject& Asset, bool bNew=true)
	{
		if(bNew)FAssetRegistryModule::AssetCreated(&Asset);
		auto* P=Asset.GetPackage();P->MarkPackageDirty();
		FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;A.SaveFlags=SAVE_NoError;
		return UPackage::SavePackage(P,&Asset,*FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension()),A);
	}
	int32 Apply(const FString& Params)
	{
		FString Path;if(!FParse::Value(*Params,TEXT("SkillAsset="),Path))return 4;
		auto* Skill=LoadObject<UHALBattleTopConfig>(nullptr,*Path);if(!Skill)return 4;
		auto* Wind=LoadObject<UNiagaraSystem>(nullptr,*(FString(Root)+TEXT("NS_BattleTop_ToonWind.NS_BattleTop_ToonWind")));
		auto* Contact=LoadObject<UNiagaraSystem>(nullptr,*(FString(Root)+TEXT("NS_BattleTop_ToonContact.NS_BattleTop_ToonContact")));
		auto* M=LoadObject<UMaterial>(nullptr,*(FString(Root)+TEXT("M_BattleTop_ToonWind.M_BattleTop_ToonWind")));
		if(!Wind||!Contact||!M||!Wind->IsReadyToRun()||!Contact->IsReadyToRun())return 5;
		auto* Before=DuplicateObject<UHALBattleTopConfig>(Skill,GetTransientPackage());
		Skill->FX.Wind=Wind;Skill->FX.ContactSparks=Contact;Skill->FX.WindMaterial=M;
		FString Error;if(!Skill->Validate(Error)){UE_LOG(LogTemp,Error,TEXT("%s"),*Error);return 6;}
		for(TFieldIterator<FProperty> I(Skill->GetClass());I;++I)if(I->GetFName()!=TEXT("FX")&&!I->Identical_InContainer(Before,Skill))return 7;
		for(TFieldIterator<FProperty> I(FHALBattleTopFXConfig::StaticStruct());I;++I)
			if(I->GetFName()!=TEXT("Wind")&&I->GetFName()!=TEXT("WindMaterial")&&I->GetFName()!=TEXT("ContactSparks")&&!I->Identical_InContainer(&Before->FX,&Skill->FX))return 7;
		const bool bSaved=Save(*Skill,false);UE_LOG(LogTemp,Display,TEXT("Cartoon FX applied: only Wind, WindMaterial, ContactSparks; ContactCount=%d"),Skill->FX.ContactCount);
		return bSaved?0:8;
	}
}

UHALBuildBattleTopCartoonFXCommandlet::UHALBuildBattleTopCartoonFXCommandlet()
{
	IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;
}
int32 UHALBuildBattleTopCartoonFXCommandlet::Main(const FString& Params)
{
	if(FParse::Param(*Params,TEXT("Apply")))return Apply(Params);
	if(FParse::Param(*Params,TEXT("ConstrainWindAxes")))
	{
		auto* S=LoadObject<UNiagaraSystem>(nullptr,*(FString(Root)+TEXT("NS_BattleTop_ToonWind")));
		if(!S||!ConstrainWindAxes(*S))return 10;
		S->PostEditChange();S->RequestCompile(true);S->WaitForCompilationComplete();
		if(!S->IsReadyToRun()||!Save(*S,false))return 10;
		UE_LOG(LogTemp,Display,TEXT("Wind axes constrained: independent random heading and exact User.TiltDegrees; world-Z advance; only ToonWind saved"));return 0;
	}
	if(FParse::Param(*Params,TEXT("RefreshCompiledData")))
	{
		for(const TCHAR* Name:{TEXT("NS_BattleTop_ToonWind"),TEXT("NS_BattleTop_ToonContact")})
		{
			auto* S=LoadObject<UNiagaraSystem>(nullptr,*(FString(Root)+Name));if(!S)return 9;
			S->PostEditChange();S->RequestCompile(true);S->WaitForCompilationComplete();
			if(!S->IsReadyToRun()||!Save(*S,false))return 9;
		}
		return 0;
	}
	for(const TCHAR* Name:{TEXT("M_BattleTop_ToonWind"),TEXT("M_BattleTop_ToonContact"),TEXT("SM_BattleTop_WindArc"),TEXT("NS_BattleTop_ToonWind"),TEXT("NS_BattleTop_ToonContact")})
		if(FPackageName::DoesPackageExist(FString(Root)+Name)){UE_LOG(LogTemp,Error,TEXT("Refusing to overwrite %s"),Name);return 1;}
	UMaterial* Wind=Material(true);UMaterial* Contact=Material(false);UStaticMesh* Mesh=ArcMesh(*Wind);if(!Mesh)return 2;
	UNiagaraSystem* W=WindSystem(*Wind,*Mesh);UNiagaraSystem* C=ContactSystem(*Contact);
	for(auto* S:{W,C})if(S)
	{
		TArray<UNiagaraScript*> Scripts{S->GetSystemSpawnScript(),S->GetSystemUpdateScript()};
		for(const auto& Handle:S->GetEmitterHandles())if(auto* E=Handle.GetInstance().Emitter.Get())E->GetLatestEmitterData()->GetScripts(Scripts);
		UE_LOG(LogTemp,Display,TEXT("%s ready=%d"),*S->GetName(),S->IsReadyToRun());
		for(auto* Script:Scripts)if(Script)UE_LOG(LogTemp,Display,TEXT("%s status=%d errors=%s"),*Script->GetPathName(),int32(Script->GetLastCompileStatus()),*Script->GetVMExecutableData().ErrorMsg);
	}
	if(!W||!C||!W->IsReadyToRun()||!C->IsReadyToRun()){UE_LOG(LogTemp,Error,TEXT("Cartoon Niagara compile failed"));return 2;}
	for(UObject* A:{static_cast<UObject*>(Wind),static_cast<UObject*>(Contact),static_cast<UObject*>(Mesh),static_cast<UObject*>(W),static_cast<UObject*>(C)})
		if(!Save(*A))return 3;
	UE_LOG(LogTemp,Display,TEXT("Created five new BattleTop cartoon assets; original resources preserved"));return 0;
}
