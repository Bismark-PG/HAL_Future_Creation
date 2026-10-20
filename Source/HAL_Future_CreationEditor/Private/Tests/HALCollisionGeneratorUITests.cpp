#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HALVehicleCollisionGenerator.h"
#include "Engine/StaticMesh.h"
#include "Misc/ObjectThumbnail.h"
#include "ObjectTools.h"
#include "UObject/StrongObjectPtr.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

namespace
{
	class FCaptureCollisionGeneratorPanel : public IAutomationLatentCommand
	{
	public:
		FCaptureCollisionGeneratorPanel(FAutomationTestBase* InTest, TSharedPtr<SDockTab> InTab)
			: Test(InTest), Tab(InTab), Start(FPlatformTime::Seconds()) {}
		virtual bool Update() override
		{
			if (FPlatformTime::Seconds() - Start < 1) { return false; }
			TArray<FColor> Pixels;
			FIntVector Size;
			if (Test->TestTrue(TEXT("Slate panel screenshot"), FSlateApplication::Get().TakeScreenshot(Tab->GetContent(), Pixels, Size)))
			{
				Test->TestTrue(TEXT("Panel has a usable laid-out area"), Size.X >= 700 && Size.Y >= 400);
				TArray64<uint8> PNG;
				FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, PNG);
				const FString Directory = FPaths::ProjectSavedDir() / TEXT("CollisionGenerator");
				IFileManager::Get().MakeDirectory(*Directory, true);
				Test->TestTrue(TEXT("UI proof saved"), FFileHelper::SaveArrayToFile(PNG, *(Directory / TEXT("Panel_UI.png"))));
			}
			Tab->RequestCloseTab();
			return true;
		}
	private:
		FAutomationTestBase* Test;
		TSharedPtr<SDockTab> Tab;
		double Start;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCollisionGeneratorUITest, "HAL.EditorUI.CollisionGenerator.Panel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCollisionGeneratorUITest::RunTest(const FString&)
{
	if (!FApp::CanEverRender() || !FSlateApplication::IsInitialized())
	{
		AddError(TEXT("Run this UI test in a rendering Editor, without -NullRHI."));
		return false;
	}
	UToolMenu* Menu = UToolMenus::Get()->FindMenu(TEXT("LevelEditor.MainMenu.Tools"));
	if (!TestNotNull(TEXT("Tools menu registered"), Menu)) { return false; }
	const FToolMenuSection* Section = Menu->FindSection(TEXT("HALTools"));
	if (!TestNotNull(TEXT("HAL section"), Section)) { return false; }
	TestNotNull(TEXT("HAL tool menu entry"), Section->FindEntry(TEXT("HALVehicleCollisionGenerator")));
	TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("HALVehicleCollisionGenerator")));
	if (!TestTrue(TEXT("Native tool tab opened"), Tab.IsValid())) { return false; }
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCaptureCollisionGeneratorPanel>(this, Tab));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCollisionGeneratorThumbnailTest, "HAL.EditorUI.CollisionGenerator.MeshThumbnail",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCollisionGeneratorThumbnailTest::RunTest(const FString&)
{
	if (!FApp::CanEverRender())
	{
		AddError(TEXT("Run this thumbnail test in a rendering Editor, without -NullRHI."));
		return false;
	}
	// A transient proxy with the reported dimensions: never load, overwrite or save a team mesh.
	TStrongObjectPtr<UStaticMesh> Mesh(NewObject<UStaticMesh>());
	FHALVehicleCollisionParameters P;
	P.LengthCm = 456; P.WidthCm = 288; P.HeightCm = 269;
	P.FrontRadiusCm = 120; P.RearRadiusCm = 120;
	FString Error;
	if (!TestTrue(TEXT("Thumbnail proxy built"), HALVehicleCollision::BuildMesh(*Mesh, P, Error)))
	{
		AddError(Error);
		return false;
	}
	FObjectThumbnail Thumbnail;
	ThumbnailTools::RenderThumbnail(Mesh.Get(), 512, 512, ThumbnailTools::EThumbnailTextureFlushMode::AlwaysFlush, nullptr, &Thumbnail);
	const TArray<uint8>& Data = Thumbnail.GetUncompressedImageData();
	const int32 Width = Thumbnail.GetImageWidth(), Height = Thumbnail.GetImageHeight();
	if (!TestTrue(TEXT("Actual Static Mesh thumbnail rendered"), Width > 0 && Height > 0 && Data.Num() == Width * Height * sizeof(FColor)))
	{
		return false;
	}
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Width * Height);
	FMemory::Memcpy(Pixels.GetData(), Data.GetData(), Data.Num());
	TArray64<uint8> PNG;
	FImageUtils::PNGCompressImageArray(Width, Height, Pixels, PNG);
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("CollisionGenerator");
	IFileManager::Get().MakeDirectory(*Directory, true);
	TestTrue(TEXT("Mesh thumbnail proof saved"), FFileHelper::SaveArrayToFile(PNG, *(Directory / TEXT("GeneratedMesh_Thumbnail.png"))));
	return true;
}

#endif
