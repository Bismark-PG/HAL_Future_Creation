#include "HALBuildBullChargeFXCommandlet.h"
#include "HALBullChargeAbility.h"
#include "HALVehiclePresentationConfig.h"
#include "Engine/SkeletalMesh.h"
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
#include "Materials/MaterialExpressionParticleRelativeTime.h"
#include "Materials/MaterialExpressionDynamicParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr const TCHAR* Root = TEXT("/Game/Assets/FX/BullCharge/");
	template<class T> T* Asset(const TCHAR* Name)
	{
		return NewObject<T>(CreatePackage(*(FString(Root) + Name)), Name, RF_Public | RF_Standalone);
	}
	template<class T> T* Node(UMaterial& M)
	{
		auto* N = NewObject<T>(&M); M.GetExpressionCollection().AddExpression(N); return N;
	}
	void Input(UMaterialExpressionCustom& H, const TCHAR* Name, UMaterialExpression* E, int32 Output = 0)
	{
		auto& I = H.Inputs.AddDefaulted_GetRef(); I.InputName = Name; I.Input.Connect(Output, E);
	}
	void Scalar(UMaterial& M, UMaterialExpressionCustom& H, const TCHAR* Name, float Value)
	{
		auto* P = Node<UMaterialExpressionScalarParameter>(M); P->ParameterName = Name; P->DefaultValue = Value;
		Input(H, Name, P);
	}
	/** Shared by fresh authoring and the explicit existing-shell migration; WPO is left intact. */
	void SoftPressureSurface(UMaterial& M, UMaterialExpressionCustom& H)
	{
		const FHALBullChargeFXConfig Defaults;
		H.Inputs.RemoveAll([](const FCustomInput& I) { return I.InputName == TEXT("ContourWidth") || I.InputName == TEXT("Ink"); });
		const auto Color = [&](const TCHAR* Name, FLinearColor Value)
		{
			auto* P = Node<UMaterialExpressionVectorParameter>(M); P->ParameterName = Name; P->DefaultValue = Value;
			Input(H, Name, P);
		};
		Color(TEXT("ShadowColor"), Defaults.PressureShadowColor); Color(TEXT("MidColor"), Defaults.PressureMidColor);
		Color(TEXT("LightColor"), Defaults.PressureLightColor);
		Scalar(M, H, TEXT("ShadowThreshold"), Defaults.PressureShadowThreshold);
		Scalar(M, H, TEXT("LightThreshold"), Defaults.PressureLightThreshold);
		Scalar(M, H, TEXT("ToneSoftness"), Defaults.PressureToneSoftness);
		H.Code = TEXT("float t=lerp(Age*60,MotionTime,saturate(UseRuntimeTime)); float phase=t*FlowSpeed; "
			"float shade=saturate(dot(normalize(N),normalize(float3(.25,-.3,.92)))*.5+.5); "
			"float halfWidth=.5*min(max(ToneSoftness,.02),max(.018,(LightThreshold-ShadowThreshold)*.9)); "
			"float shadowToMid=smoothstep(ShadowThreshold-halfWidth,ShadowThreshold+halfWidth,shade); "
			"float midToLight=smoothstep(LightThreshold-halfWidth,LightThreshold+halfWidth,shade); "
			"float3 col=lerp(lerp(ShadowColor.rgb,MidColor.rgb,shadowToMid),LightColor.rgb,midToLight); "
			"float wave=.5+.5*sin((UV.x*2-phase)*6.2831853+.32*cos(UV.y*6.2831853)); "
			"float facing=abs(dot(normalize(N),normalize(View))); "
			"float edge=1-smoothstep(1-clamp(Feather,.05,.6),1,UV.x); "
			"float lower=smoothstep(-.98,-.28,sin(UV.y*6.2831853)); "
			"float silhouetteFade=smoothstep(0,clamp(Feather,.05,.6),facing); "
			"float a=(.76+.12*wave)*edge*lower*silhouetteFade; "
			"return float4(col,saturate(a*PressureOpacity*Envelope));");
	}
	UMaterial* Material(const TCHAR* Name, int32 Kind)
	{
		auto* M = Asset<UMaterial>(Name);
		M->BlendMode = BLEND_Translucent; M->SetShadingModel(MSM_Unlit); M->TwoSided = true;
		M->SetMaterialUsage(Kind == 1 ? MATUSAGE_NiagaraSprites : MATUSAGE_NiagaraMeshParticles);
		auto* H = Node<UMaterialExpressionCustom>(*M); H->OutputType = CMOT_Float4;
		Input(*H, TEXT("UV"), Node<UMaterialExpressionTextureCoordinate>(*M));
		Input(*H, TEXT("Age"), Node<UMaterialExpressionParticleRelativeTime>(*M));
		if (Kind == 0)
		{
			Input(*H, TEXT("N"), Node<UMaterialExpressionVertexNormalWS>(*M));
			Scalar(*M, *H, TEXT("Envelope"), 1); Scalar(*M, *H, TEXT("Opacity"), .48f);
			H->Code = TEXT("float shade=dot(normalize(N),normalize(float3(.2,-.35,.9)))*.5+.5; "
				"float3 c=lerp(lerp(float3(.27,.29,.3),float3(.68,.71,.73),smoothstep(.36,.43,shade)),float3(.94,.97,1),smoothstep(.73,.8,shade)); "
				"float life=smoothstep(0,.15,Age)*(1-smoothstep(.55,1,Age)); "
				"return float4(c,Opacity*Envelope*life); ");
		}
		else
		{
			Input(*H, TEXT("Tint"), Node<UMaterialExpressionParticleColor>(*M), 5);
			Input(*H, TEXT("Control"), Node<UMaterialExpressionDynamicParameter>(*M));
			if (Kind == 1)
			{
				H->Code = TEXT("float2 p=UV*2-1; float r=length(p); float a=atan2(p.y,p.x); "
					"float shape=.46+.10*cos(a*5)+.035*cos(a*9); float mask=1-smoothstep(shape-.035,shape+.035,r); "
					"float life=1-smoothstep(.05,1,Age); float core=1-smoothstep(.12,.32,r); "
					"float3 c=lerp(Tint.rgb*.7,lerp(Tint.rgb,float3(1,1,.94),.7),core); "
					"return float4(c*max(Control.x,0),mask*life*Tint.a); ");
			}
			else
			{
				H->Code = TEXT("float brush=.84+.16*sin(UV.x*31.4159+sin(UV.x*18.8496)); "
					"float band=smoothstep(0,.12,UV.y)*(1-smoothstep(.7,1,UV.y)); "
					"float life=smoothstep(0,.06,Age)*(1-smoothstep(.25,1,Age)); "
					"float3 c=lerp(Tint.rgb*.65,Tint.rgb,smoothstep(.4,.48,UV.y)); "
					"return float4(c*max(Control.x,0),brush*band*life*Tint.a*.8); ");
				auto* Offset = Node<UMaterialExpressionCustom>(*M); Offset->OutputType = CMOT_Float3;
				Input(*Offset, TEXT("V"), Node<UMaterialExpressionVertexColor>(*M));
				Input(*Offset, TEXT("Age"), Node<UMaterialExpressionParticleRelativeTime>(*M));
				Offset->Code = TEXT("#if VERTEXSHADER\nreturn TransformLocalVectorToWorld(Parameters,(V.rgb*2-1)*100*(lerp(.2,1.2,saturate(Age))-1));\n#else\nreturn float3(0,0,0);\n#endif");
				M->GetEditorOnlyData()->WorldPositionOffset.Connect(0, Offset);
			}
		}
		auto* RGB = Node<UMaterialExpressionComponentMask>(*M); RGB->R = RGB->G = RGB->B = true; RGB->A = false; RGB->Input.Connect(0, H);
		auto* Alpha = Node<UMaterialExpressionComponentMask>(*M); Alpha->R = Alpha->G = Alpha->B = false; Alpha->A = true; Alpha->Input.Connect(0, H);
		M->GetEditorOnlyData()->EmissiveColor.Connect(0, RGB); M->GetEditorOnlyData()->Opacity.Connect(0, Alpha);
		M->PostEditChange(); return M;
	}
	UMaterial* PressureWindMaterial(const TCHAR* Name, bool bShell, bool bContour = false)
	{
		auto* M = Asset<UMaterial>(Name);
		M->BlendMode = BLEND_Translucent; M->SetShadingModel(MSM_Unlit); M->TwoSided = true;
		M->SetMaterialUsage(MATUSAGE_NiagaraMeshParticles);
		auto* H = Node<UMaterialExpressionCustom>(*M); H->OutputType = CMOT_Float4;
		Input(*H, TEXT("UV"), Node<UMaterialExpressionTextureCoordinate>(*M));
		Input(*H, TEXT("Age"), Node<UMaterialExpressionParticleRelativeTime>(*M));
		Input(*H, TEXT("N"), Node<UMaterialExpressionVertexNormalWS>(*M));
		Scalar(*M, *H, TEXT("Envelope"), 1); Scalar(*M, *H, TEXT("FlowSpeed"), 1.8f);
		if (bShell)
		{
			Input(*H, TEXT("View"), Node<UMaterialExpressionCameraVectorWS>(*M));
			Scalar(*M, *H, TEXT("PressureOpacity"), .42f); Scalar(*M, *H, TEXT("Feather"), .28f);
			H->Code = TEXT("float phase=Age*60*FlowSpeed; float rim=pow(saturate(1-abs(dot(normalize(N),normalize(View)))),1.35); "
				"float wave=.5+.5*sin((UV.x*2-phase)*6.2831853+.32*cos(UV.y*6.2831853)); "
				"float edge=1-smoothstep(1-clamp(Feather,.05,.6),1,UV.x); "
				"float lower=smoothstep(-.98,-.28,sin(UV.y*6.2831853)); "
				"float a=(.30+.48*rim+.25*smoothstep(.62,.95,wave))*edge*lower; "
				"float c=lerp(.77,.98,smoothstep(.35,.8,wave)); return float4(c,c,c,saturate(a*PressureOpacity*Envelope));");
		}
		else
		{
			Input(*H, TEXT("V"), Node<UMaterialExpressionVertexColor>(*M));
			Scalar(*M, *H, TEXT("Opacity"), .72f);
			H->Code = TEXT("float phase=Age*60*FlowSpeed; float w=.5+.5*sin((UV.x*2-phase+V.r)*6.2831853); "
				"float core=smoothstep(.25,.82,w); float side=1-smoothstep(.35,1,abs(UV.y*2-1)); "
				"float tip=smoothstep(0,.045,UV.x)*(1-smoothstep(.78,1,UV.x)); "
				"float shade=saturate(dot(normalize(N),normalize(float3(.25,-.3,.92)))*.5+.5); "
				"float c=lerp(lerp(.45,.76,smoothstep(.32,.43,shade)),.99,smoothstep(.7,.82,shade)); "
				"return float4(c,c,c,saturate(Opacity*Envelope*side*tip*(.52+.48*core)));");
		}
		if (bContour)
		{
			Scalar(*M, *H, TEXT("MotionTime"), 0); Scalar(*M, *H, TEXT("UseRuntimeTime"), 0);
			Scalar(*M, *H, TEXT("ContourWidth"), .12f);
			auto* Ink = Node<UMaterialExpressionVectorParameter>(*M); Ink->ParameterName = TEXT("ContourColor"); Ink->DefaultValue = FLinearColor(.055f, .075f, .085f);
			Input(*H, TEXT("Ink"), Ink);
			if (bShell)
			{
				SoftPressureSurface(*M, *H);
			}
			else
			{
				H->Code = TEXT("float t=lerp(Age*60,MotionTime,saturate(UseRuntimeTime)); float phase=t*FlowSpeed; "
					"float w=.5+.5*sin((UV.x*2-phase+V.r)*6.2831853); float core=smoothstep(.25,.82,w); float q=abs(UV.y*2-1); "
					"float side=1-smoothstep(.87,1,q); float tip=smoothstep(0,.012,UV.x)*(1-smoothstep(.78,1,UV.x)); "
					"float shade=saturate(dot(normalize(N),normalize(float3(.25,-.3,.92)))*.5+.5); "
					"float c=lerp(lerp(.30,.71,smoothstep(.32,.40,shade)),.99,smoothstep(.72,.80,shade)); "
					"float outline=smoothstep(1-ContourWidth-.03,1-ContourWidth+.03,q)*step(.001,ContourWidth); "
					"float3 col=lerp(float3(c,c,c),Ink.rgb,outline); "
					"float a=max(Opacity*(.65+.35*core),outline*.88)*Envelope*side*tip; return float4(col,saturate(a));");
			}
			auto* Motion = Node<UMaterialExpressionCustom>(*M); Motion->OutputType = CMOT_Float3;
			Input(*Motion, TEXT("UV"), Node<UMaterialExpressionTextureCoordinate>(*M));
			Input(*Motion, TEXT("Age"), Node<UMaterialExpressionParticleRelativeTime>(*M));
			Input(*Motion, TEXT("V"), Node<UMaterialExpressionVertexColor>(*M));
			Scalar(*M, *Motion, TEXT("MotionTime"), 0); Scalar(*M, *Motion, TEXT("UseRuntimeTime"), 0);
			Scalar(*M, *Motion, TEXT("FlowSpeed"), 1.8f); Scalar(*M, *Motion, TEXT("Turbulence"), bShell ? 2.5f : 8.f);
			Motion->Code = bShell
				? TEXT("#if VERTEXSHADER\nfloat t=lerp(Age*60,MotionTime,saturate(UseRuntimeTime))*FlowSpeed; float p=UV.x*10-t*6.2831853+UV.y*6.2831853; float a=Turbulence*smoothstep(0,.2,UV.x); return TransformLocalVectorToWorld(Parameters,float3(0,sin(p)*a,cos(p+UV.x*2)*a*.65));\n#else\nreturn float3(0,0,0);\n#endif")
				: TEXT("#if VERTEXSHADER\nfloat t=lerp(Age*60,MotionTime,saturate(UseRuntimeTime))*FlowSpeed; float p=(UV.x*1.8-t+V.r)*6.2831853; float a=Turbulence*smoothstep(0,.18,UV.x)*(1-.4*UV.x); return TransformLocalVectorToWorld(Parameters,float3(0,sin(p)*a,cos(p*.83+V.r*3)*a*.65));\n#else\nreturn float3(0,0,0);\n#endif");
			M->GetEditorOnlyData()->WorldPositionOffset.Connect(0, Motion);
		}
		auto* RGB = Node<UMaterialExpressionComponentMask>(*M); RGB->R = RGB->G = RGB->B = true; RGB->A = false; RGB->Input.Connect(0, H);
		auto* Alpha = Node<UMaterialExpressionComponentMask>(*M); Alpha->R = Alpha->G = Alpha->B = false; Alpha->A = true; Alpha->Input.Connect(0, H);
		M->GetEditorOnlyData()->EmissiveColor.Connect(0, RGB); M->GetEditorOnlyData()->Opacity.Connect(0, Alpha);
		M->PostEditChange(); return M;
	}
	UStaticMesh* Mesh(const TCHAR* Name, UMaterial& Material, bool bShock, bool bPressure = false, bool bFlow = false, bool bConnected = false)
	{
		auto* M = Asset<UStaticMesh>(Name);
		FMeshDescription D; FStaticMeshAttributes A(D); A.Register();
		auto Position = A.GetVertexPositions(); auto Normal = A.GetVertexInstanceNormals();
		auto Tangent = A.GetVertexInstanceTangents(); auto Sign = A.GetVertexInstanceBinormalSigns();
		auto UV = A.GetVertexInstanceUVs(); UV.SetNumChannels(1); auto Color = A.GetVertexInstanceColors();
		const auto Group = D.CreatePolygonGroup(); A.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Flow");
		float LanePhase = 0;
		const auto Triangle = [&](FVector3f P0, FVector3f P1, FVector3f P2, FVector2f T0, FVector2f T1, FVector2f T2)
		{
			TArray<FVertexInstanceID, TInlineAllocator<3>> IDs;
			const FVector3f N = FVector3f::CrossProduct(P1 - P0, P2 - P0).GetSafeNormal();
			const FVector3f T = (P1 - P0).GetSafeNormal();
			const FVector3f Ps[] = {P0, P1, P2}; const FVector2f Ts[] = {T0, T1, T2};
			for (int32 I = 0; I < 3; ++I)
			{
				const auto V = D.CreateVertex(); Position[V] = Ps[I]; const auto VI = D.CreateVertexInstance(V); IDs.Add(VI);
				Normal[VI] = bPressure ? Ps[I].GetSafeNormal() : N; Tangent[VI] = T; Sign[VI] = 1; UV.Set(VI, 0, Ts[I]);
				Color[VI] = bFlow ? FVector4f(LanePhase, 0, 0, 1) : bShock ? FVector4f(Ps[I] / 200.f + FVector3f(.5f), 1) : FVector4f(1);
			}
			D.CreateTriangle(Group, IDs);
		};
		const int32 Lanes = bShock || bPressure ? 1 : bFlow ? 7 : 3;
		const int32 Segments = bPressure ? 32 : bShock ? 64 : 28, Across = bPressure ? 64 : 1;
		for (int32 Lane = 0; Lane < Lanes; ++Lane)
		{
			LanePhase = float(Lane) / Lanes;
			const auto P = [&](float U, float V)
			{
				if (bPressure)
				{
					const float Theta = .015f + U * 1.60f, Phi = V * 2 * UE_PI;
					return FVector3f(FMath::Cos(Theta), FMath::Sin(Theta) * FMath::Cos(Phi), FMath::Sin(Theta) * FMath::Sin(Phi)) * 100;
				}
				if (bShock)
				{
					const float Angle = U * 2 * UE_PI, R = 78 + V * 22;
					return FVector3f(-.003f * R * R, FMath::Cos(Angle) * R, FMath::Sin(Angle) * R);
				}
				if (bFlow)
				{
					const float Open = bConnected ? FMath::SmoothStep(0.f, .16f, U) : 1.f;
					const float W = 9 * (.15f + .85f * FMath::Sqrt(FMath::Max(0.f, FMath::Sin(UE_PI * U)))) * (bConnected ? FMath::SmoothStep(0.f, .05f, U) : 1.f);
					const float S = Lane % 2 ? 1.f : -1.f, Layer = Lane / 2;
					return Lane == 6 ? FVector3f(-235 * U, (V - .5f) * W * 2, 20 * Open + 80 * FMath::Sin(U * UE_PI * .7f))
						: FVector3f(-235 * U, S * (14 * Open + (50 + Layer * 10) * FMath::Pow(U, .8f)), (8 + Layer * 17) * Open + 25 * FMath::Sin(U * UE_PI) + (V - .5f) * W * 2);
				}
				const float Width = 4 * FMath::Sqrt(FMath::Max(0.f, FMath::Sin(UE_PI * U))) + .2f;
				const float Side = Lane == 0 ? -1.f : 1.f;
				return Lane == 2 ? FVector3f(-210 * U, (V - .5f) * Width * 2, 25 + 65 * FMath::Sin(U * UE_PI * .7f))
					: FVector3f(-210 * U, Side * (20 + 95 * FMath::Pow(U, .8f)), 12 + 25 * FMath::Sin(U * UE_PI) + (V - .5f) * Width * 2);
			};
			for (int32 I = 0; I < Segments; ++I)
			{
				const float U = float(I) / Segments, N = float(I + 1) / Segments;
				for (int32 J = 0; J < Across; ++J)
				{
					const float A0 = float(J) / Across, A1 = float(J + 1) / Across;
					Triangle(P(U, A0), P(N, A0), P(N, A1), FVector2f(U, A0), FVector2f(N, A0), FVector2f(N, A1));
					Triangle(P(U, A0), P(N, A1), P(U, A1), FVector2f(U, A0), FVector2f(N, A1), FVector2f(U, A1));
				}
			}
		}
		M->GetStaticMaterials().Add(FStaticMaterial(&Material)); M->SetNumSourceModels(1);
		auto& B = M->GetSourceModel(0).BuildSettings; B.bRecomputeNormals = B.bRecomputeTangents = B.bGenerateLightmapUVs = false;
		UStaticMesh::FBuildMeshDescriptionsParams Params; Params.bBuildSimpleCollision = false; Params.bCommitMeshDescription = true; Params.bMarkPackageDirty = false;
		return M->BuildFromMeshDescriptions({&D}, Params) ? M : nullptr;
	}
	template<class T> FNiagaraVariable User(UNiagaraSystem& S, FNiagaraTypeDefinition Type, const TCHAR* Name, T Value)
	{
		FNiagaraVariable V(Type, Name); S.GetExposedParameters().AddParameter(V); S.GetExposedParameters().SetParameterValue(Value, V); return V;
	}
	void Link(UNiagaraNodeFunctionCall& N, const TCHAR* Name, FNiagaraTypeDefinition Type, const FNiagaraVariable& Variable)
	{
		const auto H = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle::CreateModuleParameterHandle(Name), &N);
		auto& P = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(N, H, Type, FGuid(), FGuid());
		FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(P, Variable, {});
	}
	void Random(UNiagaraNodeFunctionCall& N, const TCHAR* Name, FNiagaraTypeDefinition Type, const TCHAR* Path,
		const FNiagaraVariable& Min, const FNiagaraVariable& Max)
	{
		const auto H = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle::CreateModuleParameterHandle(Name), &N);
		auto& P = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(N, H, Type, FGuid(), FGuid());
		UNiagaraNodeFunctionCall* R = nullptr;
		FNiagaraStackGraphUtilities::SetDynamicInputForFunctionInput(P, LoadObject<UNiagaraScript>(nullptr, Path), R);
		check(R); Link(*R, TEXT("Minimum"), Type, Min); Link(*R, TEXT("Maximum"), Type, Max);
	}
	UNiagaraNodeOutput* Output(UNiagaraGraph& G, ENiagaraScriptUsage Usage)
	{
		for (const auto& N : G.Nodes) { if (auto* O = Cast<UNiagaraNodeOutput>(N.Get()); O && O->GetUsage() == Usage) { return O; } }
		return nullptr;
	}
	UNiagaraNodeFunctionCall* Module(UNiagaraNodeOutput& Out, const TCHAR* Path)
	{
		return FNiagaraStackGraphUtilities::AddScriptModuleToStack(LoadObject<UNiagaraScript>(nullptr, Path), Out);
	}
	/** A single looping local wind emitter, or a one-shot flash / expanding shock mesh. */
	void AddAccent(UNiagaraSystem& S, const TCHAR* Name, UMaterial& Material, UStaticMesh* Mesh, bool bWind,
		bool bContinuous = false, bool bPressure = false, bool bConnected = false)
	{
		auto* E = NewObject<UNiagaraEmitter>(&S, Name); UNiagaraEmitterFactoryNew::InitializeEmitter(E, false);
		auto* Data = E->GetLatestEmitterData(); Data->bLocalSpace = true; Data->bDeterminism = true; Data->SimTarget = ENiagaraSimTarget::CPUSim;
		auto& G = *CastChecked<UNiagaraScriptSource>(Data->GraphSource)->NodeGraph;
		auto* EU = Output(G, ENiagaraScriptUsage::EmitterUpdateScript);
		auto* PS = Output(G, ENiagaraScriptUsage::ParticleSpawnScript);
		auto* PU = Output(G, ENiagaraScriptUsage::ParticleUpdateScript);
		auto* State = Module(*EU, TEXT("/Niagara/Modules/Emitter/EmitterState.EmitterState"));
		auto* Mode = LoadObject<UEnum>(nullptr, TEXT("/Niagara/Enums/ENiagaraEmitterLifeCycleMode.ENiagaraEmitterLifeCycleMode"));
		for (auto* Pin : State->Pins) if (Pin->PinName == TEXT("Life Cycle Mode"))
			for (int32 I = 0; I < Mode->NumEnums(); ++I) if (Mode->GetDisplayNameTextByIndex(I).ToString() == TEXT("Self")) { Pin->DefaultValue = Mode->GetNameStringByIndex(I); }
		const auto Loop = User(S, FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.LoopDuration"), 60.f);
		Link(*State, TEXT("Loop Duration"), FNiagaraTypeDefinition::GetFloatDef(), Loop);
		if (bWind && !bContinuous)
		{
			const auto Rate = User(S, FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.SpawnRate"), 14.f);
			Link(*Module(*EU, TEXT("/Niagara/Modules/Emitter/SpawnRate.SpawnRate")), TEXT("SpawnRate"), FNiagaraTypeDefinition::GetFloatDef(), Rate);
		}
		else
		{
			auto* Burst = Module(*EU, TEXT("/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous"));
			const auto One = User(S, FNiagaraTypeDefinition::GetIntDef(), TEXT("User.AccentCount"), 1);
			Link(*Burst, TEXT("Spawn Count"), FNiagaraTypeDefinition::GetIntDef(), One);
		}
		Module(*PS, TEXT("/Niagara/Modules/Spawn/Location/SystemLocation.SystemLocation"));
		if (bConnected && !bPressure)
		{
			const auto Origin = User(S, FNiagaraTypeDefinition::GetVec3Def(), TEXT("User.FlowOrigin"), FVector3f(100, 0, 0));
			auto* Position = FNiagaraStackGraphUtilities::AddParameterModuleToStack({FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("Particles.Position"))},
				*PS, INDEX_NONE, {TEXT("(X=100,Y=0,Z=0)")});
			Link(*Position, TEXT("Particles.Position"), FNiagaraTypeDefinition::GetPositionDef(), Origin);
		}
		const TCHAR* LifeName = bContinuous ? TEXT("User.StreamLifetime") : bWind ? TEXT("User.Lifetime") : Mesh ? TEXT("User.ShockLifetime") : TEXT("User.FlashLifetime");
		const auto Life = User(S, FNiagaraTypeDefinition::GetFloatDef(), LifeName, bContinuous ? 60.f : bWind ? .32f : Mesh ? .3f : .12f);
		const auto Tint = User(S, FNiagaraTypeDefinition::GetColorDef(), TEXT("User.HighlightColor"), FLinearColor(1, .92f, .63f));
		const auto Style = User(S, FNiagaraTypeDefinition::GetVec4Def(), TEXT("User.StyleParameters"), FVector4f(1.1f, 0, 0, 0));
		auto* Set = FNiagaraStackGraphUtilities::AddParameterModuleToStack({
			FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Particles.Lifetime")),
			FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), TEXT("Particles.Color")),
			FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Particles.DynamicMaterialParameter"))}, *PS, INDEX_NONE,
			{TEXT(".32"), TEXT("(R=1,G=.92,B=.63,A=1)"), TEXT("(X=1.1,Y=0,Z=0,W=0)")});
		Link(*Set, TEXT("Particles.Lifetime"), FNiagaraTypeDefinition::GetFloatDef(), Life);
		Link(*Set, TEXT("Particles.Color"), FNiagaraTypeDefinition::GetColorDef(), Tint);
		Link(*Set, TEXT("Particles.DynamicMaterialParameter"), FNiagaraTypeDefinition::GetVec4Def(), Style);
		if (Mesh)
		{
			auto* Scale = FNiagaraStackGraphUtilities::AddParameterModuleToStack({
				FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Particles.Scale")),
				FNiagaraVariable(FNiagaraTypeDefinition::GetQuatDef(), TEXT("Particles.MeshOrientation"))}, *PS, INDEX_NONE,
				{TEXT("(X=1,Y=1,Z=1)"), TEXT("(X=0,Y=0,Z=0,W=1)")});
			if (bWind)
			{
				if (bContinuous)
				{
					const auto Value = User(S, FNiagaraTypeDefinition::GetVec3Def(), bPressure ? TEXT("User.PressureScale") : TEXT("User.WindScale"), bPressure ? FVector3f(1, 1.45f, 1.1f) : FVector3f(1));
					Link(*Scale, TEXT("Particles.Scale"), FNiagaraTypeDefinition::GetVec3Def(), Value);
				}
				else
				{
				const auto Min = User(S, FNiagaraTypeDefinition::GetVec3Def(), TEXT("User.ScaleMin"), FVector3f(.85f));
				const auto Max = User(S, FNiagaraTypeDefinition::GetVec3Def(), TEXT("User.ScaleMax"), FVector3f(1.15f));
				Random(*Scale, TEXT("Particles.Scale"), FNiagaraTypeDefinition::GetVec3Def(),
					TEXT("/Niagara/DynamicInputs/UniformRange/V2/RandomRangeVector.RandomRangeVector"), Min, Max);
				const auto Velocity = User(S, FNiagaraTypeDefinition::GetVec3Def(), TEXT("User.Velocity"), FVector3f(-140, 0, 0));
				Link(*Module(*PS, TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocity.AddVelocity")), TEXT("Velocity"), FNiagaraTypeDefinition::GetVec3Def(), Velocity);
				}
			}
			else
			{
				const auto Value = User(S, FNiagaraTypeDefinition::GetVec3Def(), TEXT("User.ShockScale"), FVector3f(1.2f));
				Link(*Scale, TEXT("Particles.Scale"), FNiagaraTypeDefinition::GetVec3Def(), Value);
			}
			auto* R = NewObject<UNiagaraMeshRendererProperties>(E); R->Meshes.Reset(); R->Meshes.AddDefaulted_GetRef().Mesh = Mesh;
			R->bOverrideMaterials = true; auto& Override = R->OverrideMaterials.AddDefaulted_GetRef(); Override.ExplicitMat = &Material;
			if (bWind)
			{
				const FNiagaraVariable Mat(FNiagaraTypeDefinition(UMaterialInterface::StaticClass()), bPressure ? TEXT("User.PressureMaterial") : TEXT("User.WindMaterial"));
				S.GetExposedParameters().AddParameter(Mat); S.GetExposedParameters().SetUObject(&Material, Mat); Override.UserParamBinding.Parameter = Mat;
			}
			R->MeshBoundsScale = FVector(1.5); E->AddRenderer(R, Data->Version.VersionGuid);
		}
		else
		{
			auto* SizeSet = FNiagaraStackGraphUtilities::AddParameterModuleToStack({FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("Particles.SpriteSize"))}, *PS, INDEX_NONE, {TEXT("(X=150,Y=150)")});
			const auto Size = User(S, FNiagaraTypeDefinition::GetVec2Def(), TEXT("User.FlashSize"), FVector2f(150));
			Link(*SizeSet, TEXT("Particles.SpriteSize"), FNiagaraTypeDefinition::GetVec2Def(), Size);
			auto* R = NewObject<UNiagaraSpriteRendererProperties>(E); R->Material = &Material; R->FacingMode = ENiagaraSpriteFacingMode::FaceCamera;
			E->AddRenderer(R, Data->Version.VersionGuid);
		}
		Module(*PU, TEXT("/Niagara/Modules/Update/Lifetime/ParticleState.ParticleState"));
		Module(*PU, TEXT("/Niagara/Modules/Solvers/SolveForcesAndVelocity.SolveForcesAndVelocity"));
		FNiagaraEditorUtilities::AddEmitterToSystem(S, *E, E->GetExposedVersion().VersionGuid);
	}
	UNiagaraSystem* Sparks(const TCHAR* Name, UMaterial& Material, bool bRear)
	{
		auto* Source = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Assets/FX/BattleTop/NS_BattleTop_ToonContact.NS_BattleTop_ToonContact"));
		if (!Source) { return nullptr; }
		auto* S = DuplicateObject<UNiagaraSystem>(Source, CreatePackage(*(FString(Root) + Name)), Name);
		S->SetFlags(RF_Public | RF_Standalone);
		User(*S, FNiagaraTypeDefinition::GetIntDef(), TEXT("User.BurstCount"), 48);
		if (bRear)
		{
			User(*S, FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.SpawnRate"), 65.f);
			User(*S, FNiagaraTypeDefinition::GetVec3Def(), TEXT("User.VelocityMin"), FVector3f(-1250, -150, -150));
			User(*S, FNiagaraTypeDefinition::GetVec3Def(), TEXT("User.VelocityMax"), FVector3f(-950, 150, 150));
		}
		for (auto& Handle : S->GetEmitterHandles())
		{
			auto* E = Handle.GetInstance().Emitter.Get(); auto* Data = E->GetLatestEmitterData(); Data->bLocalSpace = !bRear;
			for (auto* Renderer : Data->GetRenderers()) if (auto* R = Cast<UNiagaraSpriteRendererProperties>(Renderer)) { R->Material = &Material; }
			if (!bRear) { continue; }
			auto& G = *CastChecked<UNiagaraScriptSource>(Data->GraphSource)->NodeGraph;
			for (const auto& N : G.Nodes) if (auto* Call = Cast<UNiagaraNodeFunctionCall>(N.Get()); Call && Call->FunctionScript)
			{
				if (Call->FunctionScript->GetName() == TEXT("SpawnBurst_Instantaneous") || Call->FunctionScript->GetName() == TEXT("AddVelocity"))
				{
					Call->SetEnabledState(ENodeEnabledState::Disabled, true); Call->MarkNodeRequiresSynchronization(TEXT("Car04 world-space continuous exhaust"), true);
				}
			}
			Link(*Module(*Output(G, ENiagaraScriptUsage::EmitterUpdateScript), TEXT("/Niagara/Modules/Emitter/SpawnRate.SpawnRate")),
				TEXT("SpawnRate"), FNiagaraTypeDefinition::GetFloatDef(), FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.SpawnRate")));
			auto* V = FNiagaraStackGraphUtilities::AddParameterModuleToStack({FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Particles.Velocity"))},
				*Output(G, ENiagaraScriptUsage::ParticleSpawnScript), INDEX_NONE, {TEXT("(X=-1100,Y=0,Z=0)")});
			Random(*V, TEXT("Particles.Velocity"), FNiagaraTypeDefinition::GetVec3Def(), TEXT("/Niagara/DynamicInputs/UniformRange/V2/RandomRangeVector.RandomRangeVector"),
				FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("User.VelocityMin")), FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("User.VelocityMax")));
		}
		return S;
	}
	bool Save(UObject& Object, bool bNew = true)
	{
		if (bNew) { FAssetRegistryModule::AssetCreated(&Object); }
		auto* Package = Object.GetPackage(); Package->MarkPackageDirty(); FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, &Object, *FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()), Args);
	}
	int32 Apply(const FString& Params)
	{
		FString SkillPath, PresentationPath;
		if (!FParse::Value(*Params, TEXT("SkillAsset="), SkillPath) || !FParse::Value(*Params, TEXT("PresentationAsset="), PresentationPath)) { return 4; }
		auto* Skill = LoadObject<UHALBullChargeConfig>(nullptr, *SkillPath);
		const auto* P = LoadObject<UHALVehiclePresentationConfig>(nullptr, *PresentationPath);
		if (!Skill || !P || !P->Mesh || Skill->FX.bEnabled) { UE_LOG(LogTemp, Error, TEXT("Explicit skill/presentation required; refusing to overwrite an enabled FX recipe")); return 5; }
		auto* Before = DuplicateObject<UHALBullChargeConfig>(Skill, GetTransientPackage());
		FHALBullChargeFXConfig FX;
		FX.FrontWind = LoadObject<UNiagaraSystem>(nullptr, *(FString(Root) + TEXT("NS_BullCharge_FrontWind.NS_BullCharge_FrontWind")));
		FX.RearSparks = LoadObject<UNiagaraSystem>(nullptr, *(FString(Root) + TEXT("NS_BullCharge_RearSparks.NS_BullCharge_RearSparks")));
		FX.ContactImpact = LoadObject<UNiagaraSystem>(nullptr, *(FString(Root) + TEXT("NS_BullCharge_ToonImpact.NS_BullCharge_ToonImpact")));
		if (!FX.FrontWind || !FX.RearSparks || !FX.ContactImpact) { return 6; }
		const FBox Bounds = P->Mesh->GetBounds().GetBox().TransformBy(P->RelativeTransform);
		const FVector Center = Bounds.GetCenter(), Extent = Bounds.GetExtent();
		const auto Mount = [&](FVector Point)
		{
			FHALVehicleFXMount M; M.Offset = FTransform(FQuat::Identity, Point).GetRelativeTransform(P->RelativeTransform); return M;
		};
		FX.FrontMount = Mount(FVector(Bounds.Max.X - Extent.X * .12, Center.Y, Center.Z));
		FX.RearMounts = {Mount(FVector(Bounds.Min.X + Extent.X * .1, Center.Y - Extent.Y * .42, Center.Z - Extent.Z * .1)),
			Mount(FVector(Bounds.Min.X + Extent.X * .1, Center.Y + Extent.Y * .42, Center.Z - Extent.Z * .1))};
		FX.WindSizeScale = FMath::Clamp(float(Extent.X / 140), .5f, 3.f); FX.bEnabled = true;
		if (!FX.IsValid()) { return 7; }
		Skill->Modify(); Skill->FX = FX;
		for (TFieldIterator<FProperty> It(Skill->GetClass()); It; ++It)
		{
			if (It->GetFName() != TEXT("FX") && !It->Identical_InContainer(Before, Skill)) { return 8; }
		}
		UE_LOG(LogTemp, Display, TEXT("Car04 FX: only FX changed; bounds=%s front=%s rear0=%s rear1=%s"),
			*Bounds.ToString(), *FX.FrontMount.Offset.ToHumanReadableString(), *FX.RearMounts[0].Offset.ToHumanReadableString(), *FX.RearMounts[1].Offset.ToHumanReadableString());
		return Save(*Skill, false) ? 0 : 9;
	}
	int32 CreatePressureWind()
	{
		for (const auto* Name : {TEXT("M_BullCharge_ContinuousWind"), TEXT("M_BullCharge_PressureShell"), TEXT("SM_BullCharge_FlowStrips"),
			TEXT("SM_BullCharge_PressureShell"), TEXT("NS_BullCharge_PressureWind")})
		{
			if (FPackageName::DoesPackageExist(FString(Root) + Name)) { UE_LOG(LogTemp, Error, TEXT("Refusing to overwrite %s"), Name); return 20; }
		}
		auto* Flow = PressureWindMaterial(TEXT("M_BullCharge_ContinuousWind"), false);
		auto* Shell = PressureWindMaterial(TEXT("M_BullCharge_PressureShell"), true);
		auto* Strips = Mesh(TEXT("SM_BullCharge_FlowStrips"), *Flow, false, false, true);
		auto* Dome = Mesh(TEXT("SM_BullCharge_PressureShell"), *Shell, false, true, false);
		if (!Strips || !Dome) { return 21; }
		auto* S = Asset<UNiagaraSystem>(TEXT("NS_BullCharge_PressureWind")); UNiagaraSystemFactoryNew::InitializeSystem(S, true);
		AddAccent(*S, TEXT("ContinuousFlowStrips"), *Flow, Strips, true, true, false);
		AddAccent(*S, TEXT("SoftPressureShell"), *Shell, Dome, true, true, true);
		S->PostEditChange(); S->RequestCompile(true); S->WaitForCompilationComplete();
		if (!S->IsReadyToRun()) { return 22; }
		for (auto* O : TArray<UObject*>{Flow, Shell, Strips, Dome, S}) { if (!Save(*O)) { return 23; } }
		UE_LOG(LogTemp, Display, TEXT("Car04 pressure wind created: two persistent mesh particles, material-driven flow; legacy resources preserved."));
		return 0;
	}
	int32 CreateContourWind()
	{
		for (const auto* Name : {TEXT("M_BullCharge_ContourWind"), TEXT("M_BullCharge_ContourShell"), TEXT("SM_BullCharge_ConnectedFlow"), TEXT("NS_BullCharge_ContourWind")})
		{
			if (FPackageName::DoesPackageExist(FString(Root) + Name)) { UE_LOG(LogTemp, Error, TEXT("Refusing to overwrite %s"), Name); return 30; }
		}
		auto* Flow = PressureWindMaterial(TEXT("M_BullCharge_ContourWind"), false, true);
		auto* Shell = PressureWindMaterial(TEXT("M_BullCharge_ContourShell"), true, true);
		auto* Strips = Mesh(TEXT("SM_BullCharge_ConnectedFlow"), *Flow, false, false, true, true);
		auto* Dome = LoadObject<UStaticMesh>(nullptr, *(FString(Root) + TEXT("SM_BullCharge_PressureShell.SM_BullCharge_PressureShell")));
		if (!Strips || !Dome) { return 31; }
		auto* S = Asset<UNiagaraSystem>(TEXT("NS_BullCharge_ContourWind")); UNiagaraSystemFactoryNew::InitializeSystem(S, true);
		AddAccent(*S, TEXT("ConnectedContourFlow"), *Flow, Strips, true, true, false, true);
		AddAccent(*S, TEXT("ContourPressureShell"), *Shell, Dome, true, true, true, true);
		S->PostEditChange(); S->RequestCompile(true); S->WaitForCompilationComplete();
		if (!S->IsReadyToRun()) { return 32; }
		for (auto* O : TArray<UObject*>{Flow, Shell, Strips, S}) { if (!Save(*O)) { return 33; } }
		UE_LOG(LogTemp, Display, TEXT("Car04 contour wind created: shared apex, anchored WPO turbulence, dark toon contours; previous resources preserved."));
		return 0;
	}
	int32 ApplyContourWind(const FString& Params)
	{
		FString Path; if (!FParse::Value(*Params, TEXT("SkillAsset="), Path)) { return 34; }
		auto* Skill = LoadObject<UHALBullChargeConfig>(nullptr, *Path);
		auto* Wind = LoadObject<UNiagaraSystem>(nullptr, *(FString(Root) + TEXT("NS_BullCharge_ContourWind.NS_BullCharge_ContourWind")));
		if (!Skill || !Wind || Skill->FX.FrontWind == Wind) { return 35; }
		auto* Before = DuplicateObject<UHALBullChargeConfig>(Skill, GetTransientPackage());
		auto& FX = Skill->FX;
		FX.FrontWind = Wind; FX.WindTurbulence = 8; FX.PressureTurbulence = 2.5f;
		FX.WindContourColor = FLinearColor(.055f, .075f, .085f); FX.WindContourWidth = .12f;
		if (FX.WindFlowSpeed <= 0) { FX.WindFlowSpeed = 1.8f; }
		FX.RearLifetime = .18f;
		if (!FX.IsValid()) { return 36; }
		for (TFieldIterator<FProperty> It(Skill->GetClass()); It; ++It)
		{
			if (It->GetFName() != TEXT("FX") && !It->Identical_InContainer(Before, Skill)) { return 37; }
		}
		const TSet<FName> Allowed = {TEXT("FrontWind"), TEXT("WindTurbulence"), TEXT("PressureTurbulence"), TEXT("WindContourColor"), TEXT("WindContourWidth"), TEXT("WindFlowSpeed"), TEXT("RearLifetime")};
		for (TFieldIterator<FProperty> It(FHALBullChargeFXConfig::StaticStruct()); It; ++It)
		{
			if (!Allowed.Contains(It->GetFName()) && !It->Identical_InContainer(&Before->FX, &FX)) { return 38; }
		}
		UE_LOG(LogTemp, Display, TEXT("Car04 contour applied only authorized FX fields. FlowSpeed %.3f -> %.3f; RearLifetime %.3f -> %.3f; RearSpeed preserved %.1f; mounts/contact/style/other tuning preserved."),
			Before->FX.WindFlowSpeed, FX.WindFlowSpeed, Before->FX.RearLifetime, FX.RearLifetime, FX.RearSpeed);
		return Save(*Skill, false) ? 0 : 39;
	}
	int32 RefreshSoftPressureSurface()
	{
		auto* M = LoadObject<UMaterial>(nullptr, *(FString(Root) + TEXT("M_BullCharge_ContourShell")));
		if (!M) { return 46; }
		UMaterialExpressionCustom* Main = nullptr;
		for (const auto& Expression : M->GetExpressionCollection().Expressions)
		{
			if (auto* H = Cast<UMaterialExpressionCustom>(Expression.Get()); H && H->OutputType == CMOT_Float4)
			{ if (Main) { return 47; } Main = H; }
		}
		if (!Main || !Main->Code.Contains(TEXT("PressureOpacity"))) { return 48; }
		if (Main->Code.Contains(TEXT("shadowToMid")))
		{ UE_LOG(LogTemp, Display, TEXT("Soft pressure surface already installed; refusing to reset artist tuning.")); return 0; }
		M->Modify(); SoftPressureSurface(*M, *Main); M->PostEditChange();
		if (!Save(*M, false)) { return 49; }
		UE_LOG(LogTemp, Display, TEXT("Car04 updated only M_BullCharge_ContourShell: whole-surface soft three tones, no contour tint; WPO and existing Niagara/DA/BP untouched."));
		return 0;
	}
	int32 RefreshContourStyle(const FString& Params)
	{
		FString Path; if (!FParse::Value(*Params, TEXT("SkillAsset="), Path)) { return 40; }
		auto* Skill = LoadObject<UHALBullChargeConfig>(nullptr, *Path); if (!Skill) { return 41; }
		for (const auto* Name : {TEXT("M_BullCharge_ContourWind"), TEXT("M_BullCharge_ContourShell")})
		{
			auto* M = LoadObject<UMaterial>(nullptr, *(FString(Root) + Name)); if (!M) { return 42; }
			M->Modify();
			for (const auto& Expression : M->GetExpressionCollection().Expressions)
			{
				auto* E = Expression.Get();
				if (auto* Color = Cast<UMaterialExpressionVectorParameter>(E); Color && Color->ParameterName == TEXT("ContourColor"))
				{ Color->DefaultValue = FLinearColor(.055f, .075f, .085f); }
				if (auto* H = Cast<UMaterialExpressionCustom>(E))
				{
					H->Code.ReplaceInline(TEXT("max((.3+.48*rim+.25*smoothstep(.62,.95,wave)),outline*1.8)*edge*lower"),
						TEXT("max((.3+.48*rim+.25*smoothstep(.62,.95,wave))*edge,outline*2.2*(1-smoothstep(.97,1,UV.x)))*lower"));
				}
			}
			M->PostEditChange(); if (!Save(*M, false)) { return 43; }
		}
		auto* Before = DuplicateObject<UHALBullChargeConfig>(Skill, GetTransientPackage());
		Skill->FX.WindContourColor = FLinearColor(.055f, .075f, .085f);
		for (TFieldIterator<FProperty> It(Skill->GetClass()); It; ++It)
		{ if (It->GetFName() != TEXT("FX") && !It->Identical_InContainer(Before, Skill)) { return 44; } }
		for (TFieldIterator<FProperty> It(FHALBullChargeFXConfig::StaticStruct()); It; ++It)
		{ if (It->GetFName() != TEXT("WindContourColor") && !It->Identical_InContainer(&Before->FX, &Skill->FX)) { return 44; } }
		UE_LOG(LogTemp, Display, TEXT("Car04 contour style refreshed: two new materials and only FX.WindContourColor in the explicit skill DA."));
		return Save(*Skill, false) ? 0 : 45;
	}
	int32 ApplyPressureWind(const FString& Params)
	{
		FString SkillPath, PresentationPath;
		if (!FParse::Value(*Params, TEXT("SkillAsset="), SkillPath) || !FParse::Value(*Params, TEXT("PresentationAsset="), PresentationPath)) { return 24; }
		auto* Skill = LoadObject<UHALBullChargeConfig>(nullptr, *SkillPath);
		const auto* P = LoadObject<UHALVehiclePresentationConfig>(nullptr, *PresentationPath);
		auto* Wind = LoadObject<UNiagaraSystem>(nullptr, *(FString(Root) + TEXT("NS_BullCharge_PressureWind.NS_BullCharge_PressureWind")));
		if (!Skill || !P || !P->Mesh || !Wind || Skill->FX.FrontWind == Wind) { return 25; }
		auto* Before = DuplicateObject<UHALBullChargeConfig>(Skill, GetTransientPackage());
		auto& FX = Skill->FX;
		FX.FrontWind = Wind; FX.WindOpacity = .72f; FX.WindFullSpeed = 1600;
		FX.WindFlowSpeed = 1.8f; FX.WindResponseSeconds = .1f;
		const FVector E = P->Mesh->GetBounds().GetBox().TransformBy(P->RelativeTransform).GetExtent();
		FX.PressureSize = FVector(100, FMath::Clamp(E.Y * 1.22, 60., 400.), FMath::Clamp(E.Z * .8, 40., 400.));
		FX.PressureOpacity = .42f; FX.PressureFeather = .28f;
		FX.RearSpawnRate = 200; FX.RearSpeed = 2600; FX.RearSize = FVector2D(16, 50); FX.RearLifetime = .38f;
		if (!FX.IsValid()) { return 26; }
		for (TFieldIterator<FProperty> It(Skill->GetClass()); It; ++It)
		{
			if (It->GetFName() != TEXT("FX") && !It->Identical_InContainer(Before, Skill)) { return 27; }
		}
		const TSet<FName> Allowed = {TEXT("FrontWind"), TEXT("WindOpacity"), TEXT("WindFullSpeed"), TEXT("WindFlowSpeed"), TEXT("WindResponseSeconds"),
			TEXT("PressureSize"), TEXT("PressureOpacity"), TEXT("PressureFeather"), TEXT("RearSpawnRate"), TEXT("RearSpeed"), TEXT("RearSize"), TEXT("RearLifetime")};
		for (TFieldIterator<FProperty> It(FHALBullChargeFXConfig::StaticStruct()); It; ++It)
		{
			if (!Allowed.Contains(It->GetFName()) && !It->Identical_InContainer(&Before->FX, &FX)) { return 28; }
		}
		UE_LOG(LogTemp, Display, TEXT("Car04 polish applied only allowed FX fields. Mounts/contact/style/gameplay preserved. Shell=%s Rear=%.1f/s Speed=%.1f Size=%s"),
			*FX.PressureSize.ToCompactString(), FX.RearSpawnRate, FX.RearSpeed, *FX.RearSize.ToString());
		return Save(*Skill, false) ? 0 : 29;
	}
}

UHALBuildBullChargeFXCommandlet::UHALBuildBullChargeFXCommandlet()
{
	IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}
int32 UHALBuildBullChargeFXCommandlet::Main(const FString& Params)
{
	if (FParse::Param(*Params, TEXT("RefreshSoftPressureSurface"))) { return RefreshSoftPressureSurface(); }
	if (FParse::Param(*Params, TEXT("CreateContourWind"))) { return CreateContourWind(); }
	if (FParse::Param(*Params, TEXT("ApplyContourWind"))) { return ApplyContourWind(Params); }
	if (FParse::Param(*Params, TEXT("RefreshContourStyle"))) { return RefreshContourStyle(Params); }
	if (FParse::Param(*Params, TEXT("CreatePressureWind"))) { return CreatePressureWind(); }
	if (FParse::Param(*Params, TEXT("ApplyPressureWind"))) { return ApplyPressureWind(Params); }
	if (FParse::Param(*Params, TEXT("ApplySkill"))) { return Apply(Params); }
	if (FParse::Param(*Params, TEXT("Audit")) || FParse::Param(*Params, TEXT("RefreshCompiledData")))
	{
		for (const auto* Name : {TEXT("NS_BullCharge_FrontWind"), TEXT("NS_BullCharge_RearSparks"), TEXT("NS_BullCharge_ToonImpact")})
		{
			auto* S = LoadObject<UNiagaraSystem>(nullptr, *(FString(Root) + Name)); if (!S) { return 12; }
			UE_LOG(LogTemp, Display, TEXT("Car04 cold-load %s Ready=%d Pending=%d SpawnUpdateAttributesEqual=%d"), Name,
				S->IsReadyToRun(), S->HasOutstandingCompilationRequests(),
				S->GetSystemSpawnScript()->GetVMExecutableData().Attributes == S->GetSystemUpdateScript()->GetVMExecutableData().Attributes);
			for (const auto& H : S->GetEmitterHandles())
			{
				const auto* Data = H.GetEmitterData(); TArray<UNiagaraScript*> Scripts; Data->GetScripts(Scripts);
				UE_LOG(LogTemp, Display, TEXT("Car04 emitter %s Ready=%d Local=%d"), *H.GetName().ToString(), Data->IsReadyToRun(), Data->bLocalSpace);
				for (auto* Script : Scripts) if (Script) { UE_LOG(LogTemp, Display, TEXT("Car04 script %s Status=%d Error=%s"), *Script->GetName(), int32(Script->GetLastCompileStatus()), *Script->GetVMExecutableData().ErrorMsg); }
			}
			if (FParse::Param(*Params, TEXT("RefreshCompiledData")))
			{
				S->PostEditChange(); S->RequestCompile(true); S->WaitForCompilationComplete();
				if (!S->IsReadyToRun() || !Save(*S, false)) { return 13; }
				UE_LOG(LogTemp, Display, TEXT("Car04 refreshed compiled data only: %s"), Name);
			}
		}
		return 0;
	}
	for (const auto* Name : {TEXT("M_BullCharge_FrontWind"), TEXT("M_BullCharge_ToonSpark"), TEXT("M_BullCharge_Flash"), TEXT("M_BullCharge_Shock"),
		TEXT("SM_BullCharge_WindStreaks"), TEXT("SM_BullCharge_ShockRing"), TEXT("NS_BullCharge_FrontWind"), TEXT("NS_BullCharge_RearSparks"), TEXT("NS_BullCharge_ToonImpact")})
	{
		if (FPackageName::DoesPackageExist(FString(Root) + Name)) { UE_LOG(LogTemp, Error, TEXT("Refusing to overwrite %s"), Name); return 1; }
	}
	auto* SourceMat = LoadObject<UMaterial>(nullptr, TEXT("/Game/Assets/FX/BattleTop/M_BattleTop_ToonContact.M_BattleTop_ToonContact"));
	if (!SourceMat) { return 2; }
	auto* Spark = DuplicateObject<UMaterial>(SourceMat, CreatePackage(*(FString(Root) + TEXT("M_BullCharge_ToonSpark"))), TEXT("M_BullCharge_ToonSpark"));
	Spark->SetFlags(RF_Public | RF_Standalone);
	auto* WindMat = Material(TEXT("M_BullCharge_FrontWind"), 0);
	auto* FlashMat = Material(TEXT("M_BullCharge_Flash"), 1);
	auto* ShockMat = Material(TEXT("M_BullCharge_Shock"), 2);
	auto* WindMesh = Mesh(TEXT("SM_BullCharge_WindStreaks"), *WindMat, false);
	auto* ShockMesh = Mesh(TEXT("SM_BullCharge_ShockRing"), *ShockMat, true);
	if (!WindMesh || !ShockMesh) { return 3; }
	auto* Wind = Asset<UNiagaraSystem>(TEXT("NS_BullCharge_FrontWind")); UNiagaraSystemFactoryNew::InitializeSystem(Wind, true);
	AddAccent(*Wind, TEXT("FrontAirflow"), *WindMat, WindMesh, true);
	auto* Rear = Sparks(TEXT("NS_BullCharge_RearSparks"), *Spark, true);
	auto* Contact = Sparks(TEXT("NS_BullCharge_ToonImpact"), *Spark, false);
	if (!Rear || !Contact) { return 3; }
	AddAccent(*Contact, TEXT("ContactFlash"), *FlashMat, nullptr, false);
	AddAccent(*Contact, TEXT("ContactShock"), *ShockMat, ShockMesh, false);
	for (auto* S : {Wind, Rear, Contact})
	{
		S->PostEditChange(); S->RequestCompile(true); S->WaitForCompilationComplete();
		if (!S->IsReadyToRun()) { UE_LOG(LogTemp, Error, TEXT("Niagara compilation failed: %s"), *S->GetName()); return 10; }
	}
	for (auto* Object : TArray<UObject*>{WindMat, Spark, FlashMat, ShockMat, WindMesh, ShockMesh, Wind, Rear, Contact}) { if (!Save(*Object)) { return 11; } }
	UE_LOG(LogTemp, Display, TEXT("Created nine Car04 VFX assets. Car03 resources, Blueprints and gameplay tuning were not saved."));
	return 0;
}
