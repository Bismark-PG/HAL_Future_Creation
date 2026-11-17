#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "../HALSmokeScreenAbility.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace
{
	/** Real material/Slate/RHI proof: a wall-clipped footprint and a veil under a normal HUD. */
	class SSmokeCoverageProof : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SSmokeCoverageProof) : _Material(nullptr),_Veil(false),_Backdrop(.9f,.25f,.12f) {} SLATE_ARGUMENT(UMaterialInstanceDynamic*,Material) SLATE_ARGUMENT(bool,Veil) SLATE_ARGUMENT(FLinearColor,Backdrop) SLATE_END_ARGS()
		void Construct(const FArguments& A) { Material=A._Material; Veil=A._Veil; Backdrop=A._Backdrop; }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(256); }
		virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& E,int32 L,const FWidgetStyle&,bool) const override
		{
			const auto* White=FCoreStyle::Get().GetBrush("WhiteBrush");
			FSlateDrawElement::MakeBox(E,L,G.ToPaintGeometry(),White,ESlateDrawEffect::None,Backdrop);
			FSlateBrush Brush; Brush.SetResourceObject(Material); Brush.DrawAs=ESlateBrushDrawType::Image;
			if(Veil) { FSlateDrawElement::MakeBox(E,L+1,G.ToPaintGeometry(),&Brush,ESlateDrawEffect::None,FLinearColor::White); }
			else
			{
				const auto Resource=FSlateApplication::Get().GetRenderer()->GetResourceHandle(Brush); TArray<FSlateVertex> V; TArray<SlateIndex> I;
				const auto Add=[&](FVector2f P,FVector2f UV) { V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),P,FVector4f(UV.X,UV.Y,1,1),UV,FColor::White)); };
				Add(FVector2f(128),FVector2f(.5f));
				for(int32 J=0;J<=32;++J)
				{
					const float A=2*PI*J/32; const FVector2f D(FMath::Cos(A),FMath::Sin(A));
					// A blocking wall cuts the right half at X=173; no smoke should leak beyond it.
					const float R=D.X>0?FMath::Min(90.f,45.f/D.X):90.f; Add(FVector2f(128)+D*R,FVector2f(.5f)+D*.5f);
					if(J>0) { I.Append({SlateIndex(0),SlateIndex(J),SlateIndex(J+1)}); }
				}
				FSlateDrawElement::MakeCustomVerts(E,L+1,Resource,V,I,nullptr,0,0);
			}
			FSlateDrawElement::MakeBox(E,L+2,G.ToPaintGeometry(FVector2D(256,24),FSlateLayoutTransform(FVector2D::ZeroVector)),White,ESlateDrawEffect::None,FLinearColor(.1f,.8f,.2f));
			return L+2;
		}
	private: UMaterialInstanceDynamic* Material=nullptr; bool Veil=false; FLinearColor Backdrop;
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSmokeRenderingTest,"HAL.RenderSmoke.SmokeScreen.ClippedCoverageAnimatedVeilAndHUD",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALSmokeRenderingTest::RunTest(const FString&)
{
	if(!FApp::CanEverRender()) { AddError(TEXT("Real RHI required, do not run this with NullRHI.")); return false; }
	const auto* C=LoadObject<UHALSmokeScreenConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_Car08_SmokeScreen"));
	if(!TestTrue(TEXT("Smoke UI materials exist"),C&&C->FX.CloudMaterial&&C->FX.ScreenMaterial)) { return false; }
	C->FX.CloudMaterial->GetMaterial()->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous);
	C->FX.ScreenMaterial->GetMaterial()->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous);
	FAssetCompilingManager::Get().FinishAllCompilation(); if(GShaderCompilingManager) { GShaderCompilingManager->FinishAllCompilation(); }
	auto* Cloud=UMaterialInstanceDynamic::Create(C->FX.CloudMaterial,GetTransientPackage());
	auto* Veil=UMaterialInstanceDynamic::Create(C->FX.ScreenMaterial,GetTransientPackage());
	FWidgetRenderer Renderer(true,false);
	const auto Render=[&](UMaterialInstanceDynamic* M,bool Screen,float Clock,float Strength,FLinearColor Backdrop=FLinearColor(.9f,.25f,.12f))
	{
		M->SetScalarParameterValue(TEXT("Clock"),Clock); M->SetScalarParameterValue(TEXT("Strength"),Strength);
		auto* Target=Renderer.DrawWidget(SNew(SSmokeCoverageProof).Material(M).Veil(Screen).Backdrop(Backdrop),FVector2D(256));
		TArray<FColor> Pixels; FlushRenderingCommands(); FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
		Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags); return Pixels;
	};
	const auto A=Render(Cloud,false,0,1),B=Render(Cloud,false,2,1),Clear=Render(Veil,true,0,0),Fog=Render(Veil,true,2,.8f);
	const FLinearColor Alternate(.05f,.6f,.95f);
	const auto OtherCloud=Render(Cloud,false,0,1,Alternate),OtherClear=Render(Veil,true,0,0,Alternate),OtherFog=Render(Veil,true,2,.8f,Alternate);
	if(!TestTrue(TEXT("Actual 256x256 GPU readbacks"),A.Num()==65536&&B.Num()==A.Num()&&Clear.Num()==A.Num()&&Fog.Num()==A.Num())) { return false; }
	// The Slate target stores sRGB bytes. Decode before measuring information/contrast attenuation.
	const auto Difference=[](FColor P,FColor Q) { const FLinearColor X(P),Y(Q); return 255*(FMath::Abs(X.R-Y.R)+FMath::Abs(X.G-Y.G)+FMath::Abs(X.B-Y.B)); };
	AddInfo(FString::Printf(TEXT("Smoke GPU centers: cloud=%s clear=%s veil=%s"),*A[128*256+128].ToString(),*Clear[128*256+128].ToString(),*Fog[128*256+128].ToString()));
	AddInfo(FString::Printf(TEXT("Linear background contrast: clear=%.3f veil=%.3f"),Difference(Clear[128*256+128],OtherClear[128*256+128]),Difference(Fog[128*256+128],OtherFog[128*256+128])));
	TestTrue(TEXT("Opaque cloud conceals both contrasting backgrounds"),Difference(A[128*256+128],OtherCloud[128*256+128])<=3);
	TestTrue(TEXT("No cloud crosses the clipped wall"),A[128*256+210]==Clear[128*256+210]);
	TestTrue(TEXT("Smoke has a bounded footprint"),A[240*256+240]==Clear[240*256+240]);
	int32 Animated=0,Darkened=0,HUDChanged=0;
	for(int32 Y=0;Y<256;++Y) for(int32 X=0;X<256;++X)
	{
		const int32 I=Y*256+X; if(A[I]!=B[I]&&Y>=24) { ++Animated; }
		if(Y>=24&&Difference(Clear[I],OtherClear[I])>50&&Difference(Fog[I],OtherFog[I])<Difference(Clear[I],OtherClear[I])*.35) { ++Darkened; }
		if(Y<24&&(A[I]!=Clear[I]||Fog[I]!=Clear[I])) { ++HUDChanged; }
	}
	TestTrue(TEXT("Smoke texture changes with time"),Animated>1000); TestTrue(TEXT("Local veil removes at least 65 percent of background contrast"),Darkened>40000);
	TestEqual(TEXT("Normal HUD is unaffected by cloud and veil"),HUDChanged,0);
	const FString Folder=FPaths::ProjectSavedDir()/TEXT("Car08Smoke/20261117"); IFileManager::Get().MakeDirectory(*Folder,true);
	FFileHelper::CreateBitmap(*(Folder/TEXT("Coverage.bmp")),256,256,A.GetData()); FFileHelper::CreateBitmap(*(Folder/TEXT("Veil.bmp")),256,256,Fog.GetData());
	AddInfo(FString::Printf(TEXT("Smoke pixels: Animated=%d Obscured=%d HUDChanged=%d"),Animated,Darkened,HUDChanged)); return true;
}
#endif
