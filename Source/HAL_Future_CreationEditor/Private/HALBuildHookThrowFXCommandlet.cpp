#include "HALBuildHookThrowFXCommandlet.h"
#include "HALHookThrowAbility.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionPixelNormalWS.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Engine/StaticMesh.h"
#include "NiagaraSystem.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace
{
	const FString HookAssetRoot(TEXT("/Game/Assets/FX/HookThrow/"));
	const FString HookDAPath(TEXT("/Game/Data/Abilities/DA_Ability_HookThrow"));
	template<class T> T* HookAsset(const FString& Path) { return NewObject<T>(CreatePackage(*Path),*FPackageName::GetShortName(Path),RF_Public|RF_Standalone); }
	template<class T> T* HookNode(UMaterial& M) { auto* N=NewObject<T>(&M); M.GetExpressionCollection().AddExpression(N); return N; }
	void HookInput(UMaterialExpressionCustom& H,const TCHAR* Name,UMaterialExpression* E)
	{ auto& I=H.Inputs.AddDefaulted_GetRef(); I.InputName=Name; I.Input.Connect(0,E); }
	UMaterial* HookMetal()
	{
		auto* M=HookAsset<UMaterial>(HookAssetRoot+TEXT("M_HookThrow_ToonMetal")); M->SetShadingModel(MSM_Unlit); M->BlendMode=BLEND_Opaque;
		bool Changed=false; M->SetMaterialUsage(Changed,MATUSAGE_InstancedStaticMeshes);
		auto* H=HookNode<UMaterialExpressionCustom>(*M); H->OutputType=CMOT_Float3;
		HookInput(*H,TEXT("N"),HookNode<UMaterialExpressionPixelNormalWS>(*M)); HookInput(*H,TEXT("V"),HookNode<UMaterialExpressionCameraVectorWS>(*M));
		const FHALHookFX FX;
		const auto Color=[&](const TCHAR* Name,FLinearColor Value) { auto* P=HookNode<UMaterialExpressionVectorParameter>(*M); P->ParameterName=Name; P->DefaultValue=Value; HookInput(*H,Name,P); };
		const auto Scalar=[&](const TCHAR* Name,float Value) { auto* P=HookNode<UMaterialExpressionScalarParameter>(*M); P->ParameterName=Name; P->DefaultValue=Value; HookInput(*H,Name,P); };
		Color(TEXT("DarkColor"),FX.DarkColor); Color(TEXT("MiddleColor"),FX.MiddleColor); Color(TEXT("LightColor"),FX.LightColor);
		Color(TEXT("LightDirection"),FLinearColor(FX.LightDirection.GetSafeNormal())); Scalar(TEXT("ShadowThreshold"),FX.ShadowThreshold);
		Scalar(TEXT("HighlightThreshold"),FX.HighlightThreshold); Scalar(TEXT("ToneSoftness"),FX.ToneSoftness); Scalar(TEXT("GlintStrength"),FX.GlintStrength);
		Scalar(TEXT("GlintPower"),FX.GlintPower); Scalar(TEXT("ContourStrength"),FX.ContourStrength);
		H->Code=TEXT(R"HOOK(float3 n = normalize(N), v = normalize(V), l = normalize(LightDirection.rgb);
n *= dot(n,v) < 0.0 ? -1.0 : 1.0;
// Use the actual visible tube facets as well as the imported smooth normals.
float3 p = GetTranslatedWorldPosition(Parameters);
float3 facet = cross(ddx(p),ddy(p));
facet *= rsqrt(max(dot(facet,facet),1.e-12));
facet *= dot(facet,v) < 0.0 ? -1.0 : 1.0;
n = normalize(facet * 0.85 + n * 0.15);
float facing = dot(n, l);
float3 reflected = reflect(-v, n);
// A broad sky reflection separated from steel by a darker horizon.
float horizon = dot(reflected, normalize(float3(l.xy * 0.45, 1.0)));
float softness = max(ToneSoftness, 0.02);
float shade = smoothstep(ShadowThreshold - softness, ShadowThreshold + softness, facing);
float3 steel = lerp(DarkColor.rgb, MiddleColor.rgb, shade);
float darkBand = 1.0 - smoothstep(0.09, 0.36, abs(horizon + 0.16));
steel = lerp(steel, DarkColor.rgb, darkBand * 0.72);
float sky = smoothstep(0.12 - softness, 0.30 + softness, horizon);
float lit = smoothstep(HighlightThreshold - softness, HighlightThreshold + softness, facing);
steel = lerp(steel, LightColor.rgb, lit * (0.24 + 0.76 * sky));
float contour = smoothstep(0.68, 0.97, 1.0 - saturate(dot(n, v)));
steel = lerp(steel, DarkColor.rgb, contour * ContourStrength);
float3 h = normalize(l + v + float3(0.0001, 0, 0));
float glint = pow(saturate(dot(n, h)), GlintPower);
return steel + LightColor.rgb * glint * GlintStrength;)HOOK");
		M->GetEditorOnlyData()->EmissiveColor.Connect(0,H); M->PostEditChange(); return M;
	}
	UStaticMesh* HookTube(const FString& Name,UMaterial& Material,bool Link)
	{
		TArray<FVector3f> Path; TArray<float> Radii;
		if(Link)
		{
			for(int32 I=0;I<=32;++I) { const float A=I*UE_TWO_PI/32; Path.Emplace(11*FMath::Cos(A),5*FMath::Sin(A),0); Radii.Add(1.8f); }
		}
		else
		{
			Path.Emplace(-45,0,0); Radii.Add(5);
			for(int32 I=0;I<=32;++I)
			{
				const float T=I/32.f,A=PI-T*PI*1.3f;
				Path.Emplace(-10+24*FMath::Cos(A),24*FMath::Sin(A),0);
				Radii.Add(6*FMath::Clamp((1-T)*4,.04f,1.f));
			}
		}
		auto* Mesh=HookAsset<UStaticMesh>(HookAssetRoot+Name); FMeshDescription D; FStaticMeshAttributes At(D); At.Register();
		auto Positions=At.GetVertexPositions(); auto Normals=At.GetVertexInstanceNormals(); auto Tangents=At.GetVertexInstanceTangents();
		auto Signs=At.GetVertexInstanceBinormalSigns(); auto UVs=At.GetVertexInstanceUVs(); UVs.SetNumChannels(1);
		const auto Group=D.CreatePolygonGroup(); At.GetPolygonGroupMaterialSlotNames()[Group]=TEXT("ToonMetal");
		constexpr int32 Sides=10; TArray<FVertexID> Vertices; TArray<FVector3f> Norm,Along;
		for(int32 I=0;I<Path.Num();++I)
		{
			const int32 Prev=I?I-1:(Link?Path.Num()-2:0),Next=I+1<Path.Num()?I+1:(Link?1:I);
			const FVector3f T=(Path[Next]-Path[Prev]).GetSafeNormal(),B=FVector3f::CrossProduct(T,FVector3f::UpVector).GetSafeNormal();
			for(int32 J=0;J<=Sides;++J)
			{
				const float A=J*UE_TWO_PI/Sides; const FVector3f N=B*FMath::Cos(A)+FVector3f::UpVector*FMath::Sin(A);
				const auto ID=D.CreateVertex(); Positions[ID]=Path[I]+N*Radii[I]; Vertices.Add(ID); Norm.Add(N); Along.Add(T);
			}
		}
		const auto Triangle=[&](int32 A,int32 B,int32 C)
		{
			TArray<FVertexInstanceID,TInlineAllocator<3>> IDs;
			for(int32 K:{A,B,C}) { const auto V=D.CreateVertexInstance(Vertices[K]); IDs.Add(V); Normals[V]=Norm[K]; Tangents[V]=Along[K]; Signs[V]=1;
				UVs.Set(V,0,FVector2f(float(K/(Sides+1))/(Path.Num()-1),float(K%(Sides+1))/Sides)); }
			D.CreateTriangle(Group,IDs);
		};
		for(int32 I=0;I<Path.Num()-1;++I) for(int32 J=0;J<Sides;++J)
		{ const int32 A=I*(Sides+1)+J,B=A+Sides+1; Triangle(A,B,B+1); Triangle(A,B+1,A+1); }
		Mesh->GetStaticMaterials().Add(FStaticMaterial(&Material)); Mesh->SetNumSourceModels(1);
		auto& Build=Mesh->GetSourceModel(0).BuildSettings; Build.bRecomputeNormals=Build.bRecomputeTangents=Build.bGenerateLightmapUVs=false;
		UStaticMesh::FBuildMeshDescriptionsParams P; P.bBuildSimpleCollision=false; P.bCommitMeshDescription=true;
		return Mesh->BuildFromMeshDescriptions({&D},P)?Mesh:nullptr;
	}
	bool HookSave(UObject& O)
	{
		FAssetRegistryModule::AssetCreated(&O); O.GetPackage()->MarkPackageDirty(); FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
		return UPackage::SavePackage(O.GetPackage(),&O,*FPackageName::LongPackageNameToFilename(O.GetPackage()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
	}
}
UHALBuildHookThrowFXCommandlet::UHALBuildHookThrowFXCommandlet() { IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true; }
int32 UHALBuildHookThrowFXCommandlet::Main(const FString&)
{
	for(const auto& Path:{HookAssetRoot+TEXT("M_HookThrow_ToonMetal"),HookAssetRoot+TEXT("SM_HookThrow_ChainLink"),HookAssetRoot+TEXT("SM_HookThrow_Head"),HookDAPath})
	{ if(FPackageName::DoesPackageExist(Path)) { UE_LOG(LogTemp,Error,TEXT("Refusing to overwrite hook asset %s"),*Path); return 1; } }
	auto* M=HookMetal(); auto* Link=HookTube(TEXT("SM_HookThrow_ChainLink"),*M,true); auto* Head=HookTube(TEXT("SM_HookThrow_Head"),*M,false);
	if(!Link||!Head) { return 2; }
	auto* C=HookAsset<UHALHookThrowConfig>(HookDAPath); C->FX.ChainMesh=Link; C->FX.HookMesh=Head; C->FX.MetalMaterial=M;
	C->FX.CaptureSparks.System=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Assets/FX/BattleTop/NS_BattleTop_ToonContact.NS_BattleTop_ToonContact"));
	FString Error; if(!C->Validate(Error)) { UE_LOG(LogTemp,Error,TEXT("Hook config: %s"),*Error); return 3; }
	if(!HookSave(*M)||!HookSave(*Link)||!HookSave(*Head)||!HookSave(*C)) { return 4; }
	UE_LOG(LogTemp,Display,TEXT("Created hook toon metal, interlocking link, tapered hook head and one skill DA. Existing BPs/maps/DA untouched.")); return 0;
}
