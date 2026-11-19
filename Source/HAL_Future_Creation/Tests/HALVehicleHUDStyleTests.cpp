#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../HALVehicleHUDStyleLibrary.h"
#include "../HALVehicleCatalog.h"
#include "Engine/SkeletalMesh.h"
#include "Components/Image.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALVehicleHUDColorTest,"HAL.FutureCreation.UI.VehicleOutline.AreaColorAndSelection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALVehicleHUDColorTest::RunTest(const FString&)
{
	using namespace HALVehicleHUD;
	TArray<FColorAreaSample> Samples;
	for (int32 I=0; I<100; ++I) { Samples.Add({FColor::Blue,.01}); }
	Samples.Add({FColor(244,30,120),5}); Samples.Add({FColor(245,31,121),3});
	Samples.Add({FColor::Green,0}); Samples.Add({FColor::Green,-100}); Samples.Add({FColor::Green,std::numeric_limits<double>::quiet_NaN()});
	FLinearColor Dominant; TestTrue(TEXT("Valid weighted samples"),DominantColor(Samples,Dominant));
	TestTrue(TEXT("Surface area beats triangle/sample count and nearby shades share a bin"),Dominant.R>.8f && Dominant.B<.3f && Dominant.G<.03f);
	FLinearColor TieA,TieB; const TArray<FColorAreaSample> AB{{FColor::Red,1},{FColor::Blue,1}}, BA{{FColor::Blue,1},{FColor::Red,1}};
	DominantColor(AB,TieA); DominantColor(BA,TieB); TestTrue(TEXT("Tie independent of traversal order"),TieA.Equals(TieB));
	const FLinearColor Input(.02f,.2f,.1f,0), Bright=BrightOutline(Input);
	TestTrue(TEXT("Hue preserved and dark paint lifted for a visible opaque edge"),FMath::IsNearlyEqual(Input.LinearRGBToHSV().R,Bright.LinearRGBToHSV().R) && Bright.GetMax()>=.85f && Bright.A==1);
	auto* Catalog=NewObject<UHALVehicleCatalog>(); auto& A=Catalog->Vehicles.AddDefaulted_GetRef(); A.VehicleId=TEXT("A"); A.bHasHUDDominantColor=true; A.HUDDominantColor=FLinearColor::Red; A.HUDColorSource=NewObject<USkeletalMesh>();
	auto& B=Catalog->Vehicles.AddDefaulted_GetRef(); B.VehicleId=TEXT("B"); B.bHasHUDDominantColor=true; B.HUDDominantColor=FLinearColor::Blue; B.HUDColorSource=NewObject<USkeletalMesh>();
	FLinearColor Color;
	TestTrue(TEXT("First participant uses own selected car"),UHALVehicleHUDStyleLibrary::GetVehicleOutlineColor(Catalog,TEXT("A"),Color)&&Color.R>.8f&&Color.B==0);
	TestTrue(TEXT("Second participant can use another car simultaneously"),UHALVehicleHUDStyleLibrary::GetVehicleOutlineColor(Catalog,TEXT("B"),Color)&&Color.B>.8f&&Color.R==0);
	TestFalse(TEXT("Missing ID resets colour"),UHALVehicleHUDStyleLibrary::GetVehicleOutlineColor(Catalog,TEXT("Missing"),Color)); TestTrue(TEXT("No stale previous vehicle tint"),Color==FLinearColor::White);
	TestFalse(TEXT("Missing catalog is safe"),UHALVehicleHUDStyleLibrary::GetVehicleOutlineColor(nullptr,TEXT("A"),Color));
	TestFalse(TEXT("Plain image does not create a wrong material"),UHALVehicleHUDStyleLibrary::ApplyVehicleOutline(NewObject<UImage>(),Catalog,TEXT("A")));
	return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "AssetCompilingManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "MaterialShaderPrecompileMode.h"
#include "RenderingThread.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Slate/WidgetRenderer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALVehicleHUDRenderTest,"HAL.RenderSmoke.UI.VehicleOutline.GeometryColorAndOpacity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALVehicleHUDRenderTest::RunTest(const FString&)
{
	if (!FApp::CanEverRender()) { AddError(TEXT("Actual RHI required")); return false; }
	auto* Material=LoadObject<UMaterial>(nullptr,TEXT("/Game/UI/Materials/M_UI_PlayerStatusRow_Outlined.M_UI_PlayerStatusRow_Outlined"));
	if (!TestNotNull(TEXT("Authored UI material"),Material)) { return false; }
	Material->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous); FAssetCompilingManager::Get().FinishAllCompilation();
	auto* Catalog=NewObject<UHALVehicleCatalog>();
	for (const auto& Pair:TArray<TPair<FName,FLinearColor>>{{TEXT("Red"),FLinearColor::Red},{TEXT("Blue"),FLinearColor::Blue}})
	{ auto& E=Catalog->Vehicles.AddDefaulted_GetRef(); E.VehicleId=Pair.Key; E.bHasHUDDominantColor=true; E.HUDDominantColor=Pair.Value; E.HUDColorSource=NewObject<USkeletalMesh>(); }
	auto* Red=NewObject<UImage>(); Red->SetBrushFromMaterial(Material); Red->SetDesiredSizeOverride(FVector2D(1280,256));
	auto* Blue=NewObject<UImage>(); Blue->SetBrushFromMaterial(Material); Blue->SetDesiredSizeOverride(FVector2D(1280,256));
	TestTrue(TEXT("Red selected car applied"),UHALVehicleHUDStyleLibrary::ApplyVehicleOutline(Red,Catalog,TEXT("Red")));
	TestTrue(TEXT("Blue selected car applied"),UHALVehicleHUDStyleLibrary::ApplyVehicleOutline(Blue,Catalog,TEXT("Blue")));
	TestTrue(TEXT("Each row owns its own material"),Red->GetDynamicMaterial()!=Blue->GetDynamicMaterial());
	FWidgetRenderer Renderer(true,false); const FString Folder=FPaths::ProjectSavedDir()/TEXT("HUDRowArt/Render"); IFileManager::Get().MakeDirectory(*Folder,true);
	const auto Capture=[&](UImage* Image,const TCHAR* Name)
	{
		auto* Target=Renderer.DrawWidget(Image->TakeWidget(),FVector2D(1280,256)); TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
		Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags);
		TestTrue(TEXT("Actual Slate proof saved"),FFileHelper::CreateBitmap(*(Folder/Name),1280,256,Pixels.GetData())); return Pixels;
	};
	auto* Texture=LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/UI_Textures/T_PlayerStatus_Row_BG.T_PlayerStatus_Row_BG"));
	if (!TestNotNull(TEXT("Saved base texture"),Texture)) { return false; }
	FAssetCompilingManager::Get().FinishAllCompilation(); Texture->SetForceMipLevelsToBeResident(30.f); Texture->WaitForStreaming(); FlushRenderingCommands();
	auto* Reference=NewObject<UImage>(); Reference->SetBrushFromTexture(Texture); Reference->SetDesiredSizeOverride(FVector2D(1280,256));
	const auto R=Capture(Red,TEXT("Outline_Red.bmp")), B=Capture(Blue,TEXT("Outline_Blue.bmp")), Original=Capture(Reference,TEXT("Original_Texture.bmp"));
	if (R.Num()!=1280*256 || B.Num()!=R.Num() || Original.Num()!=R.Num()) { return false; }
	const FColor Center=R[128*1280+600], Left=R[128*1280+134], Other=B[128*1280+134];
	const FColor Baseline=Original[128*1280+600]; FLinearColor FillParameter; Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("FillColor")),FillParameter);
	AddInfo(FString::Printf(TEXT("Texture/material center comparison: texture=%s material=%s textureSRGB=%d fill=%s"),*Baseline.ToString(),*Center.ToString(),Texture->SRGB,*FillParameter.ToString()));
	TestTrue(TEXT("Final UI grey and opacity match the original texture"),FMath::Abs(int32(Center.R)-Baseline.R)<=2 && FMath::Abs(int32(Center.G)-Baseline.G)<=2 && FMath::Abs(int32(Center.B)-Baseline.B)<=2 && FMath::Abs(int32(Center.A)-Baseline.A)<=2);
	TestTrue(TEXT("Interior retains 80 percent opacity"),FMath::Abs(int32(Center.A)-204)<=3);
	TestTrue(TEXT("Interior remains neutral dark grey"),FMath::Abs(int32(Center.R)-Center.G)<=2 && FMath::Abs(int32(Center.G)-Center.B)<=2);
	TestTrue(TEXT("Outer background is transparent"),R[0].A==0 && R[128*1280+40].A==0);
	TestTrue(TEXT("Steeper side starts near x128, not old x57"),R[128*1280+100].A==0 && Left.A>240);
	TestTrue(TEXT("First row uses red edge"),Left.R>Left.B+100);
	TestTrue(TEXT("Other row independently uses blue edge"),Other.B>Other.R+100);
	TestFalse(TEXT("Missing selection reports fallback"),UHALVehicleHUDStyleLibrary::ApplyVehicleOutline(Red,Catalog,NAME_None));
	TestTrue(TEXT("Missing selection resets the existing MID to white"),Red->GetDynamicMaterial()->K2_GetVectorParameterValue(TEXT("OutlineColor"))==FLinearColor::White);
	AddInfo(FString::Printf(TEXT("Actual Slate pixels: center=%s left=%s other=%s"),*Center.ToString(),*Left.ToString(),*Other.ToString()));
	return true;
}
#endif
