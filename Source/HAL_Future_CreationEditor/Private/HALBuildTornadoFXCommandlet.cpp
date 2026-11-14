#include "HALBuildTornadoFXCommandlet.h"
#include "HALTornadoAbility.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraEditorUtilities.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "NiagaraMeshRendererProperties.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionPreSkinnedPosition.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "UObject/UnrealType.h"

namespace
{
	const FString Root(TEXT("/Game/Assets/FX/Tornado/"));
	template<class T> T* Asset(const FString& Path)
	{ return NewObject<T>(CreatePackage(*Path),*FPackageName::GetShortName(Path),RF_Public|RF_Standalone); }
	template<class T> T* Node(UMaterial& M)
	{ auto* N=NewObject<T>(&M); M.GetExpressionCollection().AddExpression(N); return N; }
	void Input(UMaterialExpressionCustom& H,const TCHAR* Name,UMaterialExpression* E)
	{ auto& I=H.Inputs.AddDefaulted_GetRef(); I.InputName=Name; I.Input.Connect(0,E); }
	void Scalar(UMaterial& M,UMaterialExpressionCustom& H,const TCHAR* Name,float Value)
	{ auto* P=Node<UMaterialExpressionScalarParameter>(M); P->ParameterName=Name; P->DefaultValue=Value; Input(H,Name,P); }
	void Color(UMaterial& M,UMaterialExpressionCustom& H,const TCHAR* Name,FLinearColor Value)
	{ auto* P=Node<UMaterialExpressionVectorParameter>(M); P->ParameterName=Name; P->DefaultValue=Value; Input(H,Name,P); }
	UMaterial* Material(bool Gather)
	{
		auto* M=Asset<UMaterial>(Root+(Gather?TEXT("M_Tornado_Gather"):TEXT("M_Tornado_Body")));
		M->BlendMode=BLEND_Translucent; M->SetShadingModel(MSM_Unlit); M->TwoSided=true; M->SetMaterialUsage(MATUSAGE_NiagaraMeshParticles);
		const FHALTornadoFX D;
		auto* H=Node<UMaterialExpressionCustom>(*M); H->OutputType=CMOT_Float4;
		Input(*H,TEXT("UV"),Node<UMaterialExpressionTextureCoordinate>(*M)); Input(*H,TEXT("N"),Node<UMaterialExpressionVertexNormalWS>(*M));
		Scalar(*M,*H,TEXT("Envelope"),1); Scalar(*M,*H,TEXT("Opacity"),D.Opacity); Scalar(*M,*H,TEXT("MotionTime"),1);
		Scalar(*M,*H,TEXT("FlowSpeed"),D.FlowSpeed); Scalar(*M,*H,TEXT("ToneSoftness"),D.ToneSoftness);
		Scalar(*M,*H,TEXT("ShadowThreshold"),D.ShadowThreshold); Scalar(*M,*H,TEXT("LightThreshold"),D.LightThreshold);
		Color(*M,*H,TEXT("ShadowColor"),D.ShadowColor); Color(*M,*H,TEXT("MidColor"),D.MidColor); Color(*M,*H,TEXT("LightColor"),D.LightColor);
		H->Code=TEXT("float shade=saturate(dot(normalize(N),normalize(float3(.5,-.4,.75)))*.5+.5); "
			"float w=.5*min(max(ToneSoftness,.02),max(.018,(LightThreshold-ShadowThreshold)*.9)); "
			"float3 col=lerp(lerp(ShadowColor.rgb,MidColor.rgb,smoothstep(ShadowThreshold-w,ShadowThreshold+w,shade)),LightColor.rgb,smoothstep(LightThreshold-w,LightThreshold+w,shade)); "
			"float edge=smoothstep(0,.12,UV.y)*(1-smoothstep(.88,1,UV.y)); "
			"float ends=smoothstep(0,.035,UV.x)*(1-smoothstep(.94,1,UV.x)); "
			"float flow=.82+.18*sin((UV.x*3-MotionTime*FlowSpeed)*6.2831853); "
			"return float4(col,saturate(Envelope*Opacity*edge*ends*flow));");
		auto* RGB=Node<UMaterialExpressionComponentMask>(*M); RGB->R=RGB->G=RGB->B=true; RGB->A=false; RGB->Input.Connect(0,H);
		auto* A=Node<UMaterialExpressionComponentMask>(*M); A->R=A->G=A->B=false; A->A=true; A->Input.Connect(0,H);
		M->GetEditorOnlyData()->EmissiveColor.Connect(0,RGB); M->GetEditorOnlyData()->Opacity.Connect(0,A);
		auto* Motion=Node<UMaterialExpressionCustom>(*M); Motion->OutputType=CMOT_Float3;
		Input(*Motion,TEXT("V"),Node<UMaterialExpressionVertexColor>(*M)); Input(*Motion,TEXT("UV"),Node<UMaterialExpressionTextureCoordinate>(*M));
		Scalar(*M,*Motion,TEXT("MotionTime"),1); Scalar(*M,*Motion,TEXT("SpinRate"),D.SpinRate); Scalar(*M,*Motion,TEXT("Shape"),1);
		Scalar(*M,*Motion,TEXT("FlowSpeed"),D.FlowSpeed); Scalar(*M,*Motion,TEXT("Turbulence"),D.Turbulence);
		Scalar(*M,*Motion,TEXT("GroundLift"),D.GroundLift); Color(*M,*Motion,TEXT("GroundNormal"),FLinearColor(0,0,1,1));
		Motion->Code=Gather
			? TEXT("#if VERTEXSHADER\nfloat3 P=(V.rgb*2-1)*100; float a=MotionTime*SpinRate*.0052359878; float s=sin(a),c=cos(a); float3 Q=float3(P.x*c-P.y*s,P.x*s+P.y*c,0); Q.xy*=Shape; Q.z=GroundLift; float3 N=normalize(GroundNormal.rgb); float3 T=normalize(cross(float3(0,1,0),N)); float3 B=cross(N,T); Q=T*Q.x+B*Q.y+N*Q.z; return TransformLocalVectorToWorld(Parameters,Q-P);\n#else\nreturn float3(0,0,0);\n#endif")
			: TEXT("#if VERTEXSHADER\nfloat3 P=(V.rgb*2-1)*100; float a=MotionTime*SpinRate*.0174532925; float s=sin(a),c=cos(a); float3 Q=float3(P.x*c-P.y*s,P.x*s+P.y*c,P.z); Q.xy*=lerp(.22,1,Shape); Q.z*=Shape; Q.x+=sin(P.z*.08+MotionTime*FlowSpeed*6.2831853)*Turbulence*UV.x; Q.y+=cos(P.z*.07+MotionTime*FlowSpeed*6.2831853)*Turbulence*UV.x; return TransformLocalVectorToWorld(Parameters,Q-P);\n#else\nreturn float3(0,0,0);\n#endif");
		M->GetEditorOnlyData()->WorldPositionOffset.Connect(0,Motion); M->PostEditChange(); return M;
	}
	UStaticMesh* Mesh(UMaterial& M,bool Gather)
	{
		auto* S=Asset<UStaticMesh>(Root+(Gather?TEXT("SM_Tornado_Gather"):TEXT("SM_Tornado_Funnel")));
		FMeshDescription D; FStaticMeshAttributes A(D); A.Register(); auto P=A.GetVertexPositions(); auto N=A.GetVertexInstanceNormals();
		auto T=A.GetVertexInstanceTangents(); auto Sign=A.GetVertexInstanceBinormalSigns(); auto UV=A.GetVertexInstanceUVs(); UV.SetNumChannels(1);
		auto Color=A.GetVertexInstanceColors(); const auto Group=D.CreatePolygonGroup(); A.GetPolygonGroupMaterialSlotNames()[Group]=TEXT("Wind");
		const auto Triangle=[&](FVector3f A0,FVector3f B,FVector3f C,FVector2f UA,FVector2f UB,FVector2f UC)
		{
			TArray<FVertexInstanceID,TInlineAllocator<3>> IDs; const FVector3f Points[]={A0,B,C}; const FVector2f Coords[]={UA,UB,UC};
			for(int32 I=0;I<3;++I)
			{
				const auto V=D.CreateVertex(); P[V]=Points[I]; const auto VI=D.CreateVertexInstance(V); IDs.Add(VI);
				N[VI]=Gather?FVector3f::UpVector:FVector3f(Points[I].X,Points[I].Y,15).GetSafeNormal();
				T[VI]=(B-A0).GetSafeNormal(); Sign[VI]=1; UV.Set(VI,0,Coords[I]); Color[VI]=FVector4f(Points[I]/200+FVector3f(.5),1);
			}
			D.CreateTriangle(Group,IDs);
		};
		const int32 Lanes=Gather?5:6, Segments=72;
		for(int32 Lane=0;Lane<Lanes;++Lane)
		{
			const auto Point=[&](float U,float V)
			{
				const float Angle=2*UE_PI*(float(Lane)/Lanes+(Gather?.65f:1.65f)*U)
					+(Gather?0.f:(V-.5f)*.65f*(.4f+.6f*FMath::Sin(U*UE_PI)));
				const float Radius=Gather?12+88*U:22+78*FMath::Pow(U,.75f);
				const float Width=(Gather?4.5f:8.f)*(FMath::Sin(U*UE_PI)*.7f+.3f);
				const float R=Radius+(Gather?(V-.5f)*Width*2:0.f);
				return FVector3f(FMath::Cos(Angle)*R,FMath::Sin(Angle)*R,Gather?0.f:U*100);
			};
			for(int32 I=0;I<Segments;++I)
			{
				const float U=float(I)/Segments,V=float(I+1)/Segments;
				Triangle(Point(U,0),Point(V,0),Point(V,1),FVector2f(U,0),FVector2f(V,0),FVector2f(V,1));
				Triangle(Point(U,0),Point(V,1),Point(U,1),FVector2f(U,0),FVector2f(V,1),FVector2f(U,1));
			}
		}
		S->GetStaticMaterials().Add(FStaticMaterial(&M)); S->SetNumSourceModels(1); auto& B=S->GetSourceModel(0).BuildSettings;
		B.bRecomputeNormals=B.bRecomputeTangents=B.bGenerateLightmapUVs=false;
		UStaticMesh::FBuildMeshDescriptionsParams Params; Params.bBuildSimpleCollision=false; Params.bCommitMeshDescription=true;
		return S->BuildFromMeshDescriptions({&D},Params)?S:nullptr;
	}
	void AlignOrigin(UNiagaraSystem& S)
	{
		for(auto& Handle:S.GetEmitterHandles())
		{
			auto* Data=Handle.GetInstance().Emitter->GetLatestEmitterData();
			auto* Source=Cast<UNiagaraScriptSource>(Data->GraphSource);
			for(const auto& Node:Source->NodeGraph->Nodes) if(auto* Assignment=Cast<UNiagaraNodeAssignment>(Node.Get()))
			{
				const int32 Index=Assignment->FindAssignmentTarget(TEXT("Particles.Position")); if(Index==INDEX_NONE) { continue; }
				const FString Zero(TEXT("(X=0,Y=0,Z=0)"));
				Assignment->SetAssignmentTarget(Index,Assignment->GetAssignmentTarget(Index),&Zero); Assignment->RefreshFromExternalChanges();
				const auto Name=FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle::CreateModuleParameterHandle(TEXT("Particles.Position")),Assignment);
				auto& Pin=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*Assignment,Name,FNiagaraTypeDefinition::GetPositionDef(),FGuid(),FGuid());
				Pin.BreakAllPinLinks(); Pin.DefaultValue=Zero;
				Assignment->MarkNodeRequiresSynchronization(TEXT("TornadoLocalOrigin"),true);
				UE_LOG(LogTemp,Display,TEXT("Aligned tornado particle origin to local zero in %s"),*Handle.GetName().ToString());
			}
		}
	}
	bool Save(UObject& O,bool bNew=true)
	{
		if(bNew) { FAssetRegistryModule::AssetCreated(&O); } auto* P=O.GetPackage(); P->MarkPackageDirty(); FSavePackageArgs A;
		A.TopLevelFlags=RF_Public|RF_Standalone; A.SaveFlags=SAVE_NoError;
		return UPackage::SavePackage(P,&O,*FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension()),A);
	}
	// New recipe: an animated low-opacity funnel, independent short rising gusts, and
	// a sparse outer layer. UV1 stores a per-gust seed and a surface-kind identifier.
	bool InstallSurfacePolish(UMaterial& M);
	UMaterial* LayeredMaterial(bool Outer)
	{
		auto* M=Asset<UMaterial>(Root+(Outer?TEXT("M_Tornado_OuterGusts"):TEXT("M_Tornado_LayeredFlow")));
		M->BlendMode=BLEND_Translucent; M->SetShadingModel(MSM_Unlit); M->TwoSided=true; M->SetMaterialUsage(MATUSAGE_NiagaraMeshParticles);
		const FHALTornadoFX D;
		const auto Inputs=[&](UMaterialExpressionCustom& H)
		{
			Input(H,TEXT("UV"),Node<UMaterialExpressionTextureCoordinate>(*M));
			auto* Extra=Node<UMaterialExpressionTextureCoordinate>(*M); Extra->CoordinateIndex=1; Input(H,TEXT("Stream"),Extra);
			Scalar(*M,H,TEXT("MotionTime"),1); Scalar(*M,H,TEXT("SpinRate"),D.SpinRate); Scalar(*M,H,TEXT("FlowSpeed"),D.FlowSpeed);
			Scalar(*M,H,TEXT("RiseSpeed"),D.RiseSpeed); Scalar(*M,H,TEXT("DifferentialRotation"),D.DifferentialRotation);
			Scalar(*M,H,TEXT("OuterSpeedScale"),D.OuterSpeedScale);
		};
		auto* H=Node<UMaterialExpressionCustom>(*M); H->OutputType=CMOT_Float4; Inputs(*H);
		Input(*H,TEXT("N"),Node<UMaterialExpressionVertexNormalWS>(*M));
		Scalar(*M,*H,TEXT("Envelope"),1); Scalar(*M,*H,TEXT("Opacity"),D.Opacity);
		Scalar(*M,*H,TEXT("CoreOpacity"),D.CoreOpacity); Scalar(*M,*H,TEXT("OuterOpacity"),D.OuterOpacity);
		Scalar(*M,*H,TEXT("ToneSoftness"),D.ToneSoftness); Scalar(*M,*H,TEXT("ShadowThreshold"),D.ShadowThreshold);
		Scalar(*M,*H,TEXT("LightThreshold"),D.LightThreshold); Scalar(*M,*H,TEXT("GrayMix"),D.GrayMix);
		Scalar(*M,*H,TEXT("ColorCycleSeconds"),D.ColorCycleSeconds);
		Color(*M,*H,TEXT("ShadowColor"),D.ShadowColor); Color(*M,*H,TEXT("MidColor"),D.MidColor);
		Color(*M,*H,TEXT("LightColor"),D.LightColor); Color(*M,*H,TEXT("GrayColor"),D.GrayColor);
		H->Code=TEXT(R"HAL(
float core=1-step(.5,Stream.y), outer=step(1.5,Stream.y), seed=Stream.x;
float rate=max(.15,1+DifferentialRotation*(seed*2-1))*lerp(1,OuterSpeedScale,outer);
float orbit=MotionTime*SpinRate*.0174532925*rate;
float3 n=normalize(N); float sn=sin(orbit), cs=cos(orbit);
n.xy=lerp(float2(n.x*cs-n.y*sn,n.x*sn+n.y*cs),n.xy,core);
float shade=saturate(dot(n,normalize(float3(.5,-.4,.75)))*.5+.5);
float w=.5*min(max(ToneSoftness,.02),max(.018,(LightThreshold-ShadowThreshold)*.9));
float3 col=lerp(lerp(ShadowColor.rgb,MidColor.rgb,smoothstep(ShadowThreshold-w,ShadowThreshold+w,shade)),LightColor.rgb,smoothstep(LightThreshold-w,LightThreshold+w,shade));
float gray=.5+.5*sin(6.2831853*(MotionTime/max(.2,ColorCycleSeconds)+seed*.91+UV.x*.32));
col=lerp(col,GrayColor.rgb*lerp(.48,1.08,shade),GrayMix*gray);
float h=frac(seed+MotionTime*RiseSpeed*lerp(1,.65,outer));
float edge=smoothstep(0,.16,UV.y)*(1-smoothstep(.84,1,UV.y));
float tips=smoothstep(0,.12,UV.x)*(1-smoothstep(.80,1,UV.x));
float gate=smoothstep(0,.10,h)*(1-smoothstep(.84,1,h));
float alpha=edge*tips*gate*lerp(1,OuterOpacity,outer);
// The faint continuous funnel has independently advected broad curls, not fixed rings.
float a=UV.x*6.2831853-MotionTime*SpinRate*.0174532925*(.65+DifferentialRotation*UV.y);
float z=UV.y-MotionTime*RiseSpeed;
float curl=sin(a*3+z*14+sin(a+z*9)*1.2);
float cloud=.30+.70*smoothstep(-.65,.80,curl);
float ends=smoothstep(0,.05,UV.y)*(1-smoothstep(.87,1,UV.y));
alpha=lerp(alpha,CoreOpacity*cloud*ends,core);
return float4(col,saturate(Envelope*Opacity*alpha));
)HAL");
		auto* RGB=Node<UMaterialExpressionComponentMask>(*M); RGB->R=RGB->G=RGB->B=true; RGB->A=false; RGB->Input.Connect(0,H);
		auto* A=Node<UMaterialExpressionComponentMask>(*M); A->R=A->G=A->B=false; A->A=true; A->Input.Connect(0,H);
		M->GetEditorOnlyData()->EmissiveColor.Connect(0,RGB); M->GetEditorOnlyData()->Opacity.Connect(0,A);
		auto* Motion=Node<UMaterialExpressionCustom>(*M); Motion->OutputType=CMOT_Float3; Inputs(*Motion);
		Input(*Motion,TEXT("V"),Node<UMaterialExpressionPreSkinnedPosition>(*M));
		Scalar(*M,*Motion,TEXT("Shape"),1); Scalar(*M,*Motion,TEXT("Turbulence"),D.Turbulence);
		Scalar(*M,*Motion,TEXT("OuterRadiusScale"),D.OuterRadiusScale);
		Motion->Code=TEXT(R"HAL(
#if VERTEXSHADER
float3 P=V;
float core=1-step(.5,Stream.y), outer=step(1.5,Stream.y), seed=Stream.x;
float rate=max(.15,1+DifferentialRotation*(seed*2-1))*lerp(1,OuterSpeedScale,outer);
float h=frac(seed+MotionTime*RiseSpeed*lerp(1,.65,outer));
float radius=(22+78*pow(max(.001,h),.72))/(22+78*pow(max(.001,seed),.72));
float a=MotionTime*SpinRate*.0174532925*rate;
float sn=sin(a),cs=cos(a);
float3 Q=float3(P.x*cs-P.y*sn,P.x*sn+P.y*cs,(P.z-seed*100)+h*100);
Q.xy*=radius*lerp(1,OuterRadiusScale,outer);
Q=lerp(Q,P,core); // An axisymmetric core is shaded by flow, never spun as a rigid spring.
Q.xy*=lerp(.22,1,Shape); Q.z*=Shape;
float bend=sin(Q.z*.045+MotionTime*FlowSpeed*2.4)*Turbulence*(Q.z/100);
Q.xy+=float2(bend,cos(Q.z*.038+MotionTime*FlowSpeed*1.8)*Turbulence*(Q.z/100));
return TransformLocalVectorToWorld(Parameters,Q-P);
#else
return float3(0,0,0);
#endif
)HAL");
		M->GetEditorOnlyData()->WorldPositionOffset.Connect(0,Motion);
		if(!InstallSurfacePolish(*M)) { return nullptr; } M->PostEditChange(); return M;
	}
	bool InstallSurfacePolish(UMaterial& M)
	{
		UMaterialExpressionCustom* Surface=nullptr;
		for(auto& Expression:M.GetExpressionCollection().Expressions) if(auto* H=Cast<UMaterialExpressionCustom>(Expression))
		{ if(H->OutputType==CMOT_Float4) { Surface=H; break; } }
		if(!Surface) { return false; }
		const FString Marker(TEXT("// HAL_TornadoFormationGlow"));
		if(Surface->Code.Contains(Marker)) { return true; }
		const FString Original(TEXT("return float4(col,saturate(Envelope*Opacity*alpha));"));
		if(!Surface->Code.Contains(Original))
		{ UE_LOG(LogTemp,Error,TEXT("Unexpected custom surface in %s; refusing to replace teammate material code."),*M.GetName()); return false; }
		const FHALTornadoFX D;
		Scalar(M,*Surface,TEXT("Formation"),1); Scalar(M,*Surface,TEXT("FormationFeather"),D.FormationFeather);
		Scalar(M,*Surface,TEXT("GlowStrength"),D.GlowStrength); Scalar(M,*Surface,TEXT("GlowPower"),D.GlowPower);
		Input(*Surface,TEXT("View"),Node<UMaterialExpressionCameraVectorWS>(M));
		Surface->Code=Surface->Code.Replace(*Original,TEXT(R"HAL(
// HAL_TornadoFormationGlow
float surfaceHeight=lerp(h,UV.y,core);
float formed=smoothstep(-max(.03,FormationFeather),0,Formation-surfaceHeight);
alpha*=formed;
float rim=pow(1-saturate(abs(dot(normalize(n),normalize(View)))),max(1,GlowPower));
col*=1+GlowStrength*rim*lerp(1,.4,core);
return float4(col,saturate(Envelope*Opacity*alpha));
)HAL"));
		return true;
	}
	int32 PolishSurface()
	{
		if(const auto* Config=LoadObject<UHALTornadoConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_Car05_Tornado.DA_Ability_Car05_Tornado")))
		{
			FString Text; FHALTornadoFX::StaticStruct()->ExportText(Text,&Config->FX,nullptr,nullptr,PPF_None,nullptr);
			UE_LOG(LogTemp,Display,TEXT("Preserving saved skill DA, current FX: %s"),*Text);
		}
		for(const auto* Name:{TEXT("M_Tornado_LayeredFlow"),TEXT("M_Tornado_OuterGusts")})
		{
			auto* M=LoadObject<UMaterial>(nullptr,*(Root+Name)); if(!M) { return 30; }
			M->Modify(); if(!InstallSurfacePolish(*M)) { return 31; } M->PostEditChange(); if(!Save(*M,false)) { return 32; }
		}
		UE_LOG(LogTemp,Display,TEXT("Patched only two current wind materials: bottom-up formation and palette rim emission. DA, Niagara, mesh and BP assets were not saved.")); return 0;
	}
	UStaticMesh* LayeredMesh(UMaterial& M,bool Outer)
	{
		auto* S=Asset<UStaticMesh>(Root+(Outer?TEXT("SM_Tornado_OuterGusts"):TEXT("SM_Tornado_LayeredFlow")));
		FMeshDescription D; FStaticMeshAttributes A(D); A.Register(); auto Positions=A.GetVertexPositions();
		auto Normals=A.GetVertexInstanceNormals(); auto Tangents=A.GetVertexInstanceTangents(); auto Signs=A.GetVertexInstanceBinormalSigns();
		auto UV=A.GetVertexInstanceUVs(); UV.SetNumChannels(2); auto Colors=A.GetVertexInstanceColors();
		const auto Group=D.CreatePolygonGroup(); A.GetPolygonGroupMaterialSlotNames()[Group]=TEXT("SoftWind");
		const auto Quad=[&](const FVector3f* P,const FVector2f* Coords,FVector2f Data)
		{
			for(const auto Indices:{FIntVector(0,1,2),FIntVector(0,2,3)})
			{
				TArray<FVertexInstanceID,TInlineAllocator<3>> IDs;
				for(int32 K=0;K<3;++K)
				{
					const int32 I=Indices[K]; const auto V=D.CreateVertex(); Positions[V]=P[I]; const auto VI=D.CreateVertexInstance(V); IDs.Add(VI);
					Normals[VI]=FVector3f(P[I].X,P[I].Y,20).GetSafeNormal(); Tangents[VI]=FVector3f(-P[I].Y,P[I].X,0).GetSafeNormal();
					Signs[VI]=1; UV.Set(VI,0,Coords[I]); UV.Set(VI,1,Data); Colors[VI]=FVector4f(P[I]/360+FVector3f(.5),1);
				}
				D.CreateTriangle(Group,IDs);
			}
		};
		if(!Outer) for(int32 Z=0;Z<18;++Z) for(int32 J=0;J<48;++J)
		{
			FVector3f P[4]; FVector2f Coords[4]; const FIntPoint Corners[]={FIntPoint(J,Z),FIntPoint(J+1,Z),FIntPoint(J+1,Z+1),FIntPoint(J,Z+1)};
			for(int32 K=0;K<4;++K)
			{
				const float U=Corners[K].X/48.f,H=Corners[K].Y/18.f,R=.88f*(22+78*FMath::Pow(FMath::Max(.001f,H),.72f));
				P[K]=FVector3f(FMath::Cos(U*2*UE_PI)*R,FMath::Sin(U*2*UE_PI)*R,H*100); Coords[K]=FVector2f(U,H);
			}
			Quad(P,Coords,FVector2f(0,0));
		}
		const int32 Count=Outer?5:22;
		for(int32 I=0;I<Count;++I)
		{
			const float Seed=.04f+.90f*FMath::Frac((I+.35f)*.618034f), Angle=I*2.39996f;
			const float Span=Outer?.85f+.13f*(I%3):1.15f+.18f*(I%4);
			const float Radius=(22+78*FMath::Pow(Seed,.72f))*(Outer?1.02f:.79f+.07f*(I%4));
			const float Width=Outer?2.8f:4.5f+1.3f*(I%4);
			const auto Point=[&](float U,float V)
			{
				const float Theta=Angle+(U-.5f)*Span;
				const float Z=Seed*100+(V-.5f)*Width*2*FMath::Sin((.08f+.84f*U)*UE_PI)+(U-.5f)*8;
				const float R=Radius+FMath::Sin(U*UE_PI)*2;
				return FVector3f(FMath::Cos(Theta)*R,FMath::Sin(Theta)*R,Z);
			};
			for(int32 J=0;J<20;++J)
			{
				const float U=J/20.f,V=(J+1)/20.f; const FVector3f P[]={Point(U,0),Point(V,0),Point(V,1),Point(U,1)};
				const FVector2f Coords[]={FVector2f(U,0),FVector2f(V,0),FVector2f(V,1),FVector2f(U,1)};
				Quad(P,Coords,FVector2f(Seed,Outer?2:1));
			}
		}
		S->GetStaticMaterials().Add(FStaticMaterial(&M)); S->SetNumSourceModels(1); auto& B=S->GetSourceModel(0).BuildSettings;
		B.bRecomputeNormals=B.bRecomputeTangents=B.bGenerateLightmapUVs=false;
		UStaticMesh::FBuildMeshDescriptionsParams Params; Params.bBuildSimpleCollision=false; Params.bCommitMeshDescription=true;
		return S->BuildFromMeshDescriptions({&D},Params)?S:nullptr;
	}
	void UseLocalMeshPosition(UMaterial& M)
	{
		for(auto& Expression:M.GetExpressionCollection().Expressions) if(auto* H=Cast<UMaterialExpressionCustom>(Expression))
		{
			if(H->OutputType!=CMOT_Float3) { continue; }
			H->Code=H->Code.Replace(TEXT("float3 P=(V.rgb*2-1)*180;"),TEXT("float3 P=V;"));
			H->Code=H->Code.Replace(TEXT("float3 P=(V.rgb*2-1)*100;"),TEXT("float3 P=V;"));
			for(auto& I:H->Inputs) if(I.InputName==TEXT("V"))
			{
				if(!Cast<UMaterialExpressionPreSkinnedPosition>(I.Input.Expression))
				{
					auto* Position=NewObject<UMaterialExpressionPreSkinnedPosition>(&M);
					I.Input.Connect(0,Position); M.GetExpressionCollection().AddExpression(Position);
				}
				break;
			}
			break;
		}
		M.PostEditChange();
	}
	UMaterial* GatherFlow(UNiagaraSystem& S)
	{
		const FString Path=Root+TEXT("M_Tornado_GatherFlow"); const bool Exists=FPackageName::DoesPackageExist(Path);
		const FNiagaraVariable V(FNiagaraTypeDefinition(UMaterialInterface::StaticClass()),TEXT("User.PressureMaterial"));
		auto* Source=Cast<UMaterial>(S.GetExposedParameters().GetUObject(V)); if(!Source) { return nullptr; }
		auto* M=Exists?LoadObject<UMaterial>(nullptr,*Path):DuplicateObject<UMaterial>(Source,CreatePackage(*Path),TEXT("M_Tornado_GatherFlow"));
		if(!M) { return nullptr; } M->SetFlags(RF_Public|RF_Standalone); UseLocalMeshPosition(*M);
		if(!Save(*M,!Exists)) { return nullptr; }
		S.GetExposedParameters().SetUObject(M,V);
		for(auto* Renderer:S.GetEmitterHandles()[1].GetInstance().Emitter->GetLatestEmitterData()->GetRenderers())
			if(auto* R=Cast<UNiagaraMeshRendererProperties>(Renderer)) { for(auto& O:R->OverrideMaterials) { O.ExplicitMat=M; } }
		return M;
	}
	int32 BuildLayered()
	{
		for(const auto* Name:{TEXT("M_Tornado_LayeredFlow"),TEXT("M_Tornado_OuterGusts"),TEXT("M_Tornado_GatherFlow"),TEXT("SM_Tornado_LayeredFlow"),TEXT("SM_Tornado_OuterGusts"),TEXT("NS_Tornado_LayeredFlow")})
		{ if(FPackageName::DoesPackageExist(Root+Name)) { UE_LOG(LogTemp,Error,TEXT("Refusing layered overwrite: %s"),Name); return 10; } }
		auto* Config=LoadObject<UHALTornadoConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_Car05_Tornado.DA_Ability_Car05_Tornado"));
		if(!Config||!Config->FX.System||Config->FX.System->GetEmitterHandles().Num()!=2) { return 11; }
		const auto OriginalRules=Config->Rules; const auto OriginalFX=Config->FX; const float OriginalCooldown=Config->CooldownSeconds;
		FString RulesText,FXText; FHALTornadoRules::StaticStruct()->ExportText(RulesText,&OriginalRules,nullptr,nullptr,PPF_None,nullptr);
		FHALTornadoFX::StaticStruct()->ExportText(FXText,&OriginalFX,nullptr,nullptr,PPF_None,nullptr);
		UE_LOG(LogTemp,Display,TEXT("Before layered rebuild: Cooldown=%.3f Rules=%s FX=%s"),OriginalCooldown,*RulesText,*FXText);
		auto* Body=LayeredMaterial(false); auto* Outer=LayeredMaterial(true); auto* BodyMesh=LayeredMesh(*Body,false); auto* OuterMesh=LayeredMesh(*Outer,true);
		if(!BodyMesh||!OuterMesh) { return 12; }
		auto* S=DuplicateObject<UNiagaraSystem>(Config->FX.System,CreatePackage(*(Root+TEXT("NS_Tornado_LayeredFlow"))),TEXT("NS_Tornado_LayeredFlow"));
		S->SetFlags(RF_Public|RF_Standalone); const auto Template=S->GetEmitterHandles()[0];
		// The editor utility also rebuilds system spawn/update graph nodes. A raw
		// DuplicateEmitterHandle leaves the new emitter absent from system execution.
		FNiagaraEditorUtilities::AddEmitterToSystem(*S,*Template.GetInstance().Emitter,Template.GetInstance().Version);
		S->GetEmitterHandles().Last().GetInstance().Emitter->GetLatestEmitterData()->RemoveParent();
		int32 I=0;
		for(auto& Handle:S->GetEmitterHandles())
		{
			if(I!=1)
			{
				auto* D=Handle.GetInstance().Emitter->GetLatestEmitterData(); D->bLocalSpace=true;
				for(auto* Renderer:D->GetRenderers()) if(auto* R=Cast<UNiagaraMeshRendererProperties>(Renderer))
				{
					R->Meshes.Reset(); R->Meshes.AddDefaulted_GetRef().Mesh=I==0?BodyMesh:OuterMesh; R->MeshBoundsScale=FVector(3);
					R->bOverrideMaterials=true; R->OverrideMaterials.Reset(); auto& Override=R->OverrideMaterials.AddDefaulted_GetRef(); Override.ExplicitMat=I==0?Body:Outer;
					const FNiagaraVariable V(FNiagaraTypeDefinition(UMaterialInterface::StaticClass()),I==0?TEXT("User.WindMaterial"):TEXT("User.OuterMaterial"));
					Override.UserParamBinding.Parameter=V; S->GetExposedParameters().AddParameter(V);
					S->GetExposedParameters().SetUObject(I==0?Body:Outer,V);
				}
			}
			++I;
		}
		if(I!=3||!GatherFlow(*S)) { return 13; } AlignOrigin(*S); S->PostEditChange(); S->RequestCompile(true); S->WaitForCompilationComplete();
		if(!S->IsReadyToRun()) { return 14; }
		for(auto* O:TArray<UObject*>{Body,Outer,BodyMesh,OuterMesh,S}) { if(!Save(*O)) { return 15; } }
		Config->FX.System=S;
		if(!FHALTornadoRules::StaticStruct()->CompareScriptStruct(&Config->Rules,&OriginalRules,0)||Config->CooldownSeconds!=OriginalCooldown) { return 16; }
		for(TFieldIterator<FProperty> It(FHALTornadoFX::StaticStruct());It;++It)
		{ if(It->GetFName()!=TEXT("System") && !It->Identical_InContainer(&Config->FX,&OriginalFX)) { return 17; } }
		if(!Save(*Config,false)) { return 18; }
		UE_LOG(LogTemp,Display,TEXT("Created six independent layered FX assets. Only skill DA FX.System changed; rules, cooldown and all other FX fields preserved.")); return 0;
	}
	int32 RepairLayered()
	{
		// This maintenance path touches only the new layered assets, never the skill DA.
		UMaterial* Outer=nullptr;
		for(const auto* Name:{TEXT("M_Tornado_LayeredFlow"),TEXT("M_Tornado_OuterGusts")})
		{
			auto* M=LoadObject<UMaterial>(nullptr,*(Root+Name)); if(!M) { return 20; }
			M->Modify(); UseLocalMeshPosition(*M); if(!Save(*M,false)) { return 21; } Outer=M;
		}
		auto* S=LoadObject<UNiagaraSystem>(nullptr,*(Root+TEXT("NS_Tornado_LayeredFlow")));
		auto* Mesh=LoadObject<UStaticMesh>(nullptr,*(Root+TEXT("SM_Tornado_OuterGusts")));
		if(!S||!Mesh||S->GetEmitterHandles().Num()!=3) { return 22; }
		const TSet<FGuid> Removed{S->GetEmitterHandles().Last().GetId()}; S->RemoveEmitterHandlesById(Removed);
		const auto Template=S->GetEmitterHandles()[0];
		FNiagaraEditorUtilities::AddEmitterToSystem(*S,*Template.GetInstance().Emitter,Template.GetInstance().Version);
		auto* Data=S->GetEmitterHandles().Last().GetInstance().Emitter->GetLatestEmitterData(); Data->RemoveParent();
		for(auto* Renderer:Data->GetRenderers()) if(auto* R=Cast<UNiagaraMeshRendererProperties>(Renderer))
		{
			R->Meshes.Reset(); R->Meshes.AddDefaulted_GetRef().Mesh=Mesh; R->MeshBoundsScale=FVector(3);
			R->bOverrideMaterials=true; R->OverrideMaterials.Reset(); auto& Override=R->OverrideMaterials.AddDefaulted_GetRef(); Override.ExplicitMat=Outer;
			const FNiagaraVariable V(FNiagaraTypeDefinition(UMaterialInterface::StaticClass()),TEXT("User.OuterMaterial"));
			Override.UserParamBinding.Parameter=V;
		}
		if(!GatherFlow(*S)) { return 24; }
		AlignOrigin(*S); S->PostEditChange(); S->RequestCompile(true); S->WaitForCompilationComplete();
		if(!S->IsReadyToRun()||!Save(*S,false)) { return 23; }
		UE_LOG(LogTemp,Display,TEXT("Repaired layered-only mesh coordinates and outer emitter system execution; DA/BP/legacy assets untouched.")); return 0;
	}
}
int32 UHALBuildTornadoFXCommandlet::Main(const FString& Params)
{
	if(FParse::Param(*Params,TEXT("Layered"))) { return BuildLayered(); }
	if(FParse::Param(*Params,TEXT("PolishSurface"))) { return PolishSurface(); }
	if(FParse::Param(*Params,TEXT("RepairLayered"))) { return RepairLayered(); }
	if(FParse::Param(*Params,TEXT("AlignOrigin")))
	{
		auto* S=LoadObject<UNiagaraSystem>(nullptr,*(Root+TEXT("NS_Tornado"))); if(!S) { return 6; }
		S->Modify(); AlignOrigin(*S); S->PostEditChange(); S->RequestCompile(true); S->WaitForCompilationComplete();
		return S->IsReadyToRun() && Save(*S,false)?0:7;
	}
	const FString ConfigPath(TEXT("/Game/Data/Abilities/DA_Ability_Car05_Tornado"));
	for(const auto& Path:TArray<FString>{Root+TEXT("M_Tornado_Body"),Root+TEXT("M_Tornado_Gather"),Root+TEXT("SM_Tornado_Funnel"),Root+TEXT("SM_Tornado_Gather"),Root+TEXT("NS_Tornado"),ConfigPath})
	{ if(FPackageName::DoesPackageExist(Path)) { UE_LOG(LogTemp,Error,TEXT("Refusing to overwrite %s"),*Path); return 1; } }
	auto* Body=Material(false); auto* Gather=Material(true); auto* Funnel=Mesh(*Body,false); auto* Ring=Mesh(*Gather,true);
	auto* Source=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Assets/FX/BullCharge/NS_BullCharge_ContourWind.NS_BullCharge_ContourWind"));
	if(!Source||!Funnel||!Ring) { return 2; }
	auto* S=DuplicateObject<UNiagaraSystem>(Source,CreatePackage(*(Root+TEXT("NS_Tornado"))),TEXT("NS_Tornado")); S->SetFlags(RF_Public|RF_Standalone);
	int32 I=0;
	for(auto& Handle:S->GetEmitterHandles())
	{
		auto* E=Handle.GetInstance().Emitter.Get(); auto* D=E->GetLatestEmitterData(); D->bLocalSpace=true;
		for(auto* Renderer:D->GetRenderers()) if(auto* R=Cast<UNiagaraMeshRendererProperties>(Renderer))
		{
			R->Meshes.Reset(); R->Meshes.AddDefaulted_GetRef().Mesh=I==0?Funnel:Ring; R->MeshBoundsScale=FVector(3);
			R->bOverrideMaterials=true; R->OverrideMaterials.Reset(); auto& O=R->OverrideMaterials.AddDefaulted_GetRef(); O.ExplicitMat=I==0?Body:Gather;
			const FNiagaraVariable V(FNiagaraTypeDefinition(UMaterialInterface::StaticClass()),I==0?TEXT("User.WindMaterial"):TEXT("User.PressureMaterial"));
			O.UserParamBinding.Parameter=V; S->GetExposedParameters().SetUObject(I==0?Body:Gather,V);
		}
		++I;
	}
	if(I!=2) { return 3; }
	AlignOrigin(*S);
	S->GetExposedParameters().SetParameterValue(FVector3f::ZeroVector,FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.FlowOrigin")));
	S->PostEditChange(); S->RequestCompile(true); S->WaitForCompilationComplete(); if(!S->IsReadyToRun()) { return 4; }
	auto* Config=Asset<UHALTornadoConfig>(ConfigPath); Config->FX.System=S;
	for(auto* O:TArray<UObject*>{Body,Gather,Funnel,Ring,S,Config}) { if(!Save(*O)) { return 5; } }
	UE_LOG(LogTemp,Display,TEXT("Created five independent Tornado FX assets and one skill DA. Existing materials, Niagara, vehicles, BP and GA assets were not saved.")); return 0;
}
