#include "HALBuildArrestChainsFXCommandlet.h"
#include "HALPolicePursuitAbility.h"
#include "VehicleDefinition.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionPixelNormalWS.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"

namespace
{
	const FString Root(TEXT("/Game/Assets/FX/PolicePursuit/"));
	template<class T> T* Asset(const FString& Path) { return NewObject<T>(CreatePackage(*Path),*FPackageName::GetShortName(Path),RF_Public|RF_Standalone); }
	template<class T> T* Node(UMaterial& M) { auto* N=NewObject<T>(&M); M.GetExpressionCollection().AddExpression(N); return N; }
	void Input(UMaterialExpressionCustom& H,const TCHAR* Name,UMaterialExpression* E)
	{ auto& I=H.Inputs.AddDefaulted_GetRef(); I.InputName=Name; I.Input.Connect(0,E); }
	void Color(UMaterial& M,UMaterialExpressionCustom& H,const TCHAR* Name,FLinearColor V)
	{ auto* N=Node<UMaterialExpressionVectorParameter>(M); N->ParameterName=Name; N->DefaultValue=V; Input(H,Name,N); }
	void Scalar(UMaterial& M,UMaterialExpressionCustom& H,const TCHAR* Name,float V)
	{ auto* N=Node<UMaterialExpressionScalarParameter>(M); N->ParameterName=Name; N->DefaultValue=V; Input(H,Name,N); }
	bool InstallWireControl(UMaterial& M)
	{
		for(UMaterialExpression* Expression:M.GetExpressions())
		{ if(Expression&&Expression->Desc==TEXT("HALArrestChainWireV2")) { return true; } }
		if(M.GetEditorOnlyData()->WorldPositionOffset.IsConnected())
		{ UE_LOG(LogTemp,Error,TEXT("Refusing to overwrite an unrelated chain WorldPositionOffset graph")); return false; }
		auto* W=Node<UMaterialExpressionCustom>(M); W->Desc=TEXT("HALArrestChainWireV2"); W->OutputType=CMOT_Float3;
		Input(*W,TEXT("UV"),Node<UMaterialExpressionTextureCoordinate>(M)); Scalar(M,*W,TEXT("WireRadius"),FHALArrestChainFX().WireRadius);
		// Native link contract: UV0 = major/minor angular coordinates; ellipse radii 10/4.5, source tube radius 1.7 cm.
		// Replace the stretched source tube offset with a circular world-space tube, keeping its centerline fixed.
		W->Code=TEXT("#if VERTEXSHADER\n"
			"float u=UV.x*6.28318530718,v=UV.y*6.28318530718; "
			"float3 localN=normalize(float3(4.5*cos(u),10*sin(u),0)); "
			"float3 oldOffset=TransformLocalVectorToWorld(Parameters,(localN*cos(v)+float3(0,0,sin(v)))*1.7); "
			"float3 tangent=normalize(TransformLocalVectorToWorld(Parameters,float3(-10*sin(u),4.5*cos(u),0))); "
			"float3 plane=TransformLocalVectorToWorld(Parameters,float3(0,0,1)); plane=normalize(plane-tangent*dot(plane,tangent)); "
			"float3 outward=normalize(cross(tangent,plane)); "
			"float holeLimit=.45*min(length(TransformLocalVectorToWorld(Parameters,float3(10,0,0))),length(TransformLocalVectorToWorld(Parameters,float3(0,4.5,0)))); "
			"float radius=min(clamp(WireRadius,.25,6),holeLimit); "
			"return (outward*cos(v)+plane*sin(v))*radius-oldOffset;\n#else\nreturn float3(0,0,0);\n#endif");
		M.GetEditorOnlyData()->WorldPositionOffset.Connect(0,W); return true;
	}
	UMaterial* Material()
	{
		auto* M=Asset<UMaterial>(Root+TEXT("M_Pursuit_ChainToon")); M->SetShadingModel(MSM_Unlit); M->BlendMode=BLEND_Masked; M->TwoSided=false;
		bool Recompile=false; M->SetMaterialUsage(Recompile,MATUSAGE_InstancedStaticMeshes);
		auto* H=Node<UMaterialExpressionCustom>(*M); H->OutputType=CMOT_Float4;
		Input(*H,TEXT("N"),Node<UMaterialExpressionPixelNormalWS>(*M)); Input(*H,TEXT("V"),Node<UMaterialExpressionCameraVectorWS>(*M)); Input(*H,TEXT("P"),Node<UMaterialExpressionWorldPosition>(*M));
		FHALArrestChainFX C;
		Color(*M,*H,TEXT("DarkColor"),C.DarkColor); Color(*M,*H,TEXT("MiddleColor"),C.MiddleColor); Color(*M,*H,TEXT("LightColor"),C.LightColor);
		Color(*M,*H,TEXT("LightDirection"),FLinearColor(C.LightDirection.GetSafeNormal()));
		Scalar(*M,*H,TEXT("ShadowThreshold"),C.ShadowThreshold); Scalar(*M,*H,TEXT("HighlightThreshold"),C.HighlightThreshold);
		Scalar(*M,*H,TEXT("ToneSoftness"),C.ToneSoftness); Scalar(*M,*H,TEXT("GlintStrength"),C.GlintStrength); Scalar(*M,*H,TEXT("Visibility"),1);
		H->Code=TEXT("float3 n=normalize(N),v=normalize(V),l=normalize(LightDirection.rgb); float shade=dot(n,l); "
			"float3 c=lerp(DarkColor.rgb,MiddleColor.rgb,smoothstep(ShadowThreshold-ToneSoftness,ShadowThreshold+ToneSoftness,shade)); "
			"c=lerp(c,LightColor.rgb,smoothstep(HighlightThreshold-ToneSoftness,HighlightThreshold+ToneSoftness,shade)); "
			"float rim=smoothstep(.65,.96,1-abs(dot(n,v))); c=lerp(c,DarkColor.rgb,rim*.55); "
			"float3 h=normalize(l+v+float3(.0001,0,0)); c+=LightColor.rgb*pow(saturate(dot(n,h)),20)*GlintStrength; "
			"float noise=frac(sin(dot(floor(P*.8),float3(12.9898,78.233,37.719)))*43758.5453); "
			"float mask=Visibility>=.999?1:Visibility<=0?0:step(noise,Visibility); return float4(c,mask);");
		auto* RGB=Node<UMaterialExpressionComponentMask>(*M); RGB->R=RGB->G=RGB->B=true; RGB->A=false; RGB->Input.Connect(0,H);
		auto* A=Node<UMaterialExpressionComponentMask>(*M); A->R=A->G=A->B=false; A->A=true; A->Input.Connect(0,H);
		M->GetEditorOnlyData()->EmissiveColor.Connect(0,RGB); M->GetEditorOnlyData()->OpacityMask.Connect(0,A);
		InstallWireControl(*M); M->PostEditChange(); return M;
	}
	UStaticMesh* Link(UMaterial& Material)
	{
		auto* Mesh=Asset<UStaticMesh>(Root+TEXT("SM_Pursuit_ChainLink")); FMeshDescription D; FStaticMeshAttributes A(D); A.Register();
		auto Pos=A.GetVertexPositions(); auto Norm=A.GetVertexInstanceNormals(); auto Tangent=A.GetVertexInstanceTangents(); auto Sign=A.GetVertexInstanceBinormalSigns();
		auto UV=A.GetVertexInstanceUVs(); UV.SetNumChannels(1); const auto Group=D.CreatePolygonGroup(); A.GetPolygonGroupMaterialSlotNames()[Group]=TEXT("ChainMetal");
		constexpr int32 Major=32,Minor=8; TArray<FVertexID> Vertices;
		for(int32 I=0;I<Major;++I) for(int32 J=0;J<Minor;++J)
		{
			const float U=I*UE_TWO_PI/Major,V=J*UE_TWO_PI/Minor; const FVector3f N=FVector3f(4.5f*FMath::Cos(U),10*FMath::Sin(U),0).GetSafeNormal();
			const FVector3f Tube=N*FMath::Cos(V)+FVector3f::UpVector*FMath::Sin(V);
			const auto Id=D.CreateVertex(); Pos[Id]=FVector3f(10*FMath::Cos(U),4.5f*FMath::Sin(U),0)+Tube*1.7f; Vertices.Add(Id);
		}
		const auto Triangle=[&](FIntPoint P,FIntPoint Q,FIntPoint R)
		{
			TArray<FVertexInstanceID,TInlineAllocator<3>> Ids;
			for(const auto IJ:{P,Q,R})
			{
				const int32 I=IJ.X%Major,J=IJ.Y%Minor; const float U=I*UE_TWO_PI/Major,V=J*UE_TWO_PI/Minor;
				const auto VI=D.CreateVertexInstance(Vertices[I*Minor+J]); Ids.Add(VI);
				Norm[VI]=FVector3f(4.5f*FMath::Cos(U),10*FMath::Sin(U),0).GetSafeNormal()*FMath::Cos(V)+FVector3f::UpVector*FMath::Sin(V);
				Tangent[VI]=FVector3f(-10*FMath::Sin(U),4.5f*FMath::Cos(U),0).GetSafeNormal(); Sign[VI]=1; UV.Set(VI,0,FVector2f(float(IJ.X)/Major,float(IJ.Y)/Minor));
			}
			D.CreateTriangle(Group,Ids);
		};
		for(int32 I=0;I<Major;++I) for(int32 J=0;J<Minor;++J)
		{ Triangle({I,J},{I+1,J},{I+1,J+1}); Triangle({I,J},{I+1,J+1},{I,J+1}); }
		Mesh->GetStaticMaterials().Add(FStaticMaterial(&Material)); Mesh->SetNumSourceModels(1);
		auto& Build=Mesh->GetSourceModel(0).BuildSettings; Build.bRecomputeNormals=Build.bRecomputeTangents=Build.bGenerateLightmapUVs=false;
		UStaticMesh::FBuildMeshDescriptionsParams P; P.bBuildSimpleCollision=false; P.bCommitMeshDescription=true;
		return Mesh->BuildFromMeshDescriptions({&D},P)?Mesh:nullptr;
	}
	bool Save(UObject& O,bool New)
	{
		if(New) { FAssetRegistryModule::AssetCreated(&O); } O.GetPackage()->MarkPackageDirty(); FSavePackageArgs A; A.TopLevelFlags=RF_Public|RF_Standalone; A.SaveFlags=SAVE_NoError;
		return UPackage::SavePackage(O.GetPackage(),&O,*FPackageName::LongPackageNameToFilename(O.GetPackage()->GetName(),FPackageName::GetAssetPackageExtension()),A);
	}
}
int32 UHALBuildArrestChainsFXCommandlet::Main(const FString& Params)
{
	if(FParse::Param(*Params,TEXT("UpgradeWireControl")))
	{
		auto* Config=LoadObject<UHALPolicePursuitConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_Car06_PolicePursuit"));
		auto* M=LoadObject<UMaterial>(nullptr,*(Root+TEXT("M_Pursuit_ChainToon")));
		if(!Config||!M||Config->FX.Chains.Material!=M||!InstallWireControl(*M)) { return 6; }
		M->PostEditChange(); if(!Save(*M,false)||!Save(*Config,false)) { return 7; }
		UE_LOG(LogTemp,Display,TEXT("Installed independent chain wire radius; existing shading and all old skill fields preserved.")); return 0;
	}
	for(const auto P:{Root+TEXT("M_Pursuit_ChainToon"),Root+TEXT("SM_Pursuit_ChainLink")})
	{ if(FPackageName::DoesPackageExist(P)) { UE_LOG(LogTemp,Error,TEXT("Refusing to overwrite existing chain art: %s"),*P); return 1; } }
	auto* Config=LoadObject<UHALPolicePursuitConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_Car06_PolicePursuit"));
	if(!Config) { return 2; } auto* M=Material(); auto* Mesh=Link(*M); if(!Mesh) { return 3; }
	if(!Save(*M,true)||!Save(*Mesh,true)) { return 4; }
	// Existing tuning, bindings, roof lights, UI, siren and deprecated marker assets are untouched.
	Config->FX.Chains.LinkMesh=Mesh; Config->FX.Chains.Material=M;
	for(const auto* Path:{TEXT("/Game/Data/MVP_B/DA_Vehicle_Car_02"),TEXT("/Game/Data/Testing/DA_Vehicle_SkillDummy_Default")})
	{
		if(auto* Definition=LoadObject<UVehicleDefinition>(nullptr,Path))
		{ auto& Fit=Config->FX.Chains.VehicleFits.AddDefaulted_GetRef(); Fit.VehicleDefinition=Definition; Fit.CenterOffset=FVector(50,0,-2); Fit.BoundsScale=FVector(.8f,.78f,.75f); }
	}
	if(!Save(*Config,false)) { return 5; }
	UE_LOG(LogTemp,Display,TEXT("Created toon chain link/material; patched only new chain references in Car06 skill DA.")); return 0;
}
