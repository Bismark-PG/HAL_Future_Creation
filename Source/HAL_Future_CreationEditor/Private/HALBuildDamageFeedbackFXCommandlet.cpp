#include "HALBuildDamageFeedbackFXCommandlet.h"
#include "HALDamageFeedbackConfig.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
	const FString MaterialPath(TEXT("/Game/Assets/FX/Materials/M_FX_VehicleDamage_PP"));
	const FString ConfigPath(TEXT("/Game/Data/StatusFX/DA_DamageFeedback_Default"));
	template<class T> T* Asset(const FString& P) { return NewObject<T>(CreatePackage(*P),*FPackageName::GetShortName(P),RF_Public|RF_Standalone); }
	template<class T> T* Node(UMaterial& M) { auto* N=NewObject<T>(&M); M.GetExpressionCollection().AddExpression(N); return N; }
	void Input(UMaterialExpressionCustom& H,FName Name,UMaterialExpression* E,int32 Output=0) { auto& I=H.Inputs.AddDefaulted_GetRef(); I.InputName=Name; I.Input.Connect(Output,E); }
	void Vector(UMaterial& M,UMaterialExpressionCustom& H,FName Name,FLinearColor Value)
	{ auto* P=Node<UMaterialExpressionVectorParameter>(M); P->ParameterName=Name; P->DefaultValue=Value; Input(H,Name,P,5); }
	bool Save(UObject& O)
	{
		FAssetRegistryModule::AssetCreated(&O); O.GetPackage()->MarkPackageDirty(); FSavePackageArgs A; A.TopLevelFlags=RF_Public|RF_Standalone; A.SaveFlags=SAVE_NoError;
		return UPackage::SavePackage(O.GetPackage(),&O,*FPackageName::LongPackageNameToFilename(O.GetPackage()->GetName(),FPackageName::GetAssetPackageExtension()),A);
	}
}
int32 UHALBuildDamageFeedbackFXCommandlet::Main(const FString&)
{
	if(FPackageName::DoesPackageExist(MaterialPath)||FPackageName::DoesPackageExist(ConfigPath))
	{ UE_LOG(LogTemp,Error,TEXT("Refusing to overwrite existing shared damage assets")); return 1; }
	auto* M=Asset<UMaterial>(MaterialPath); M->MaterialDomain=MD_PostProcess; M->BlendableLocation=BL_SceneColorAfterTonemapping;
	M->BlendablePriority=1000; M->SetShadingModel(MSM_Unlit);
	auto* H=Node<UMaterialExpressionCustom>(*M); H->OutputType=CMOT_Float3;
	for(const auto Id:{PPI_PostProcessInput0,PPI_SceneDepth,PPI_CustomDepth,PPI_CustomStencil})
	{ auto* Texture=Node<UMaterialExpressionSceneTexture>(*M); Texture->SceneTextureId=Id; Input(*H,FName(*FString::Printf(TEXT("Scene%d"),int32(Id))),Texture); }
	const FHALDamageFeedbackStyle S;
	Vector(*M,*H,TEXT("BodyColor"),S.BodyColor); Vector(*M,*H,TEXT("EdgeColor"),S.EdgeColor);
	Vector(*M,*H,TEXT("EdgeSettings"),FLinearColor(S.OutlinePixels,S.OutlineSoftness,S.DepthTolerance,0));
	const auto D=S.SeparationDirection.GetSafeNormal(); Vector(*M,*H,TEXT("SplitSettings"),FLinearColor(D.X,D.Y,S.SeparationPixels,0));
	for(int32 I=0;I<8;++I) { Vector(*M,*H,FName(*FString::Printf(TEXT("Target%d"),I)),FLinearColor(0,0,0,0)); }
	H->Code=TEXT(R"HAL(
struct DamageFX
{
    float4 Targets[8];
    float Tolerance;
    float4 Event(float id)
    {
        if(id<.5) return 0;
        [unroll] for(int i=0;i<8;i++) if(abs(Targets[i].r-id)<.25) return Targets[i];
        return 0;
    }
    float2 BufferUV(float2 p,int textureId) { return ClampSceneTextureUV(ViewportUVToSceneTextureUV(p,textureId),textureId); }
    float Depth(float2 p) { return SceneTextureLookup(BufferUV(p,1),1,false).r; }
    float3 Color(float2 p) { return SceneTextureLookup(BufferUV(p,14),14,false).rgb; }
    // xyz = independent body/white/RGB weights; w = visible custom depth.
    float4 Mask(float2 p)
    {
        if(any(p<0)||any(p>1)) return float4(0,0,0,1e8);
        float id=SceneTextureLookup(BufferUV(p,25),25,false).r;
        float4 e=Event(id);
        if(max(e.g,max(e.b,e.a))<=0) return float4(0,0,0,1e8);
        float d=SceneTextureLookup(BufferUV(p,13),13,false).r;
        if(d>Depth(p)+Tolerance) return float4(0,0,0,1e8);
        return float4(e.gba,d);
    }
    float3 Red(float3 color,float3 tint,float weight)
    {
        float shade=.35+.65*saturate(dot(color,float3(.299,.587,.114))*1.5);
        return lerp(color,tint*shade,weight);
    }
};
DamageFX fx;
fx.Targets[0]=Target0; fx.Targets[1]=Target1; fx.Targets[2]=Target2; fx.Targets[3]=Target3;
fx.Targets[4]=Target4; fx.Targets[5]=Target5; fx.Targets[6]=Target6; fx.Targets[7]=Target7;
fx.Tolerance=EdgeSettings.b;
float2 uv=GetViewportUV(Parameters),pixel=GetSceneTextureViewSize(14).zw;
float4 center=fx.Mask(uv);
float3 result=fx.Red(Scene14.rgb,BodyColor.rgb,center.r);
float2 shift=SplitSettings.rg*SplitSettings.b*pixel;
float2 qR=uv-shift,qB=uv+shift,qG=uv+float2(-shift.y,shift.x)*.4;
float4 mR=fx.Mask(qR),mG=fx.Mask(qG),mB=fx.Mask(qB);
float destinationDepth=fx.Depth(uv);
mR.rgb*=step(mR.a,destinationDepth+fx.Tolerance);
mG.rgb*=step(mG.a,destinationDepth+fx.Tolerance);
mB.rgb*=step(mB.a,destinationDepth+fx.Tolerance);
if(center.b>0&&SplitSettings.b>0)
{
    float3 r=fx.Red(mR.r>0?fx.Color(qR):Scene14.rgb,BodyColor.rgb,center.r);
    float3 g=fx.Red(mG.r>0?fx.Color(qG):Scene14.rgb,BodyColor.rgb,center.r);
    float3 b=fx.Red(mB.r>0?fx.Color(qB):Scene14.rgb,BodyColor.rgb,center.r);
    result=lerp(result,float3(r.r,g.g,b.b),center.b*.6);
}
float present=step(.0001,max(center.r,max(center.g,center.b)));
float3 fringe=float3(mR.b,mG.b,mB.b)*(1-present)*step(.001,SplitSettings.b);
float fringeAlpha=max(fringe.r,max(fringe.g,fringe.b));
result=lerp(result,fringe/max(fringeAlpha,.0001),fringeAlpha);
float outline=0;
if(EdgeSettings.r>0&&present<.5)
{
    [unroll] for(int i=0;i<8;i++)
    {
        float angle=i*.78539816339;
        float2 dir=float2(cos(angle),sin(angle))*pixel;
        float4 inner=fx.Mask(uv+dir*max(0,EdgeSettings.r-EdgeSettings.g*.5));
        float4 outer=fx.Mask(uv+dir*(EdgeSettings.r+EdgeSettings.g));
        float a=inner.g*step(inner.a,destinationDepth+fx.Tolerance);
        float b=outer.g*.35*step(outer.a,destinationDepth+fx.Tolerance);
        outline=max(outline,max(a,b));
    }
}
return lerp(result,EdgeColor.rgb,outline);
)HAL");
	M->GetEditorOnlyData()->EmissiveColor.Connect(0,H); M->PostEditChange();
	auto* C=Asset<UHALDamageFeedbackConfig>(ConfigPath); C->Material=M;
	if(!Save(*M)||!Save(*C)) { return 2; }
	UE_LOG(LogTemp,Display,TEXT("Created global damage feedback Post Process material and shared DA; no existing art/BP saved.")); return 0;
}
