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
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "Misc/ObjectThumbnail.h"
#include "ObjectTools.h"
#include "UObject/StrongObjectPtr.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"

namespace
{
	TSharedPtr<SButton> FindCollisionButton(const TSharedRef<SWidget>& Root, FName Tag)
	{
		if (Root->GetTag() == Tag) { return StaticCastSharedRef<SButton>(Root); }
		FChildren* Children = Root->GetChildren();
		for (int32 I = 0; I < Children->Num(); ++I)
		{
			if (auto Found = FindCollisionButton(Children->GetChildAt(I), Tag)) { return Found; }
		}
		return nullptr;
	}

	class FExerciseCollisionUpdate : public IAutomationLatentCommand
	{
	public:
		FExerciseCollisionUpdate(FAutomationTestBase* InTest, TSharedPtr<SDockTab> InTab, UStaticMesh* InFirst, UStaticMesh* InSecond, const TArray<FAssetData>& InSelection)
			: Test(InTest), Tab(InTab), First(InFirst), Second(InSecond), PreviousSelection(InSelection), Start(FPlatformTime::Seconds()) {}
		~FExerciseCollisionUpdate()
		{
			Tab->RequestCloseTab();
			FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().SyncBrowserToAssets(PreviousSelection);
			for (UStaticMesh* Mesh : { First.Get(), Second.Get() })
			{
				FAssetRegistryModule::AssetDeleted(Mesh);
				Mesh->ClearFlags(RF_Public | RF_Standalone);
				Mesh->GetOutermost()->SetDirtyFlag(false);
			}
		}
		bool Update() override
		{
			if (FPlatformTime::Seconds() - Start < 1) { return false; }
			auto Load = FindCollisionButton(Tab->GetContent(), TEXT("HALCollision.Load"));
			auto UpdateButton = FindCollisionButton(Tab->GetContent(), TEXT("HALCollision.Update"));
			if (!Test->TestTrue(TEXT("Load/update buttons present in real panel"), Load.IsValid() && UpdateButton.IsValid())) { return true; }
			auto& Browser = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get();
			TArray<FAssetData> Selected; Browser.GetSelectedAssets(Selected);
			if (Stage == 0)
			{
				if (!Test->TestTrue(TEXT("First temporary mesh selected"), Selected.Num() == 1 && Selected[0].GetAsset() == First.Get())) { return true; }
				Test->TestFalse(TEXT("Update disabled before loading"), UpdateButton->IsEnabled());
				Load->SimulateClick();
				Test->TestTrue(TEXT("Successful load enables update"), UpdateButton->IsEnabled());
				FirstGuid = First->GetBodySetup()->BodySetupGuid;
				SecondGuid = Second->GetBodySetup()->BodySetupGuid;
				First->GetOutermost()->SetDirtyFlag(false); Second->GetOutermost()->SetDirtyFlag(false);
				Browser.SyncBrowserToAssets(TArray<UObject*>{ Second.Get() });
				Stage = 1; Start = FPlatformTime::Seconds(); return false;
			}
			if (!Test->TestTrue(TEXT("Browser selection changed after loading"), Selected.Num() == 1 && Selected[0].GetAsset() == Second.Get())) { return true; }
			UpdateButton->SimulateClick();
			Test->TestTrue(TEXT("Update remains bound to originally loaded asset"), First->GetBodySetup()->BodySetupGuid != FirstGuid && First->GetOutermost()->IsDirty());
			Test->TestTrue(TEXT("New browser selection is not overwritten"), Second->GetBodySetup()->BodySetupGuid == SecondGuid && !Second->GetOutermost()->IsDirty());
			TArray<FColor> Pixels; FIntVector Size;
			if (Test->TestTrue(TEXT("Loaded update panel screenshot"), FSlateApplication::Get().TakeScreenshot(Tab->GetContent(), Pixels, Size)))
			{
				TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, PNG);
				const FString Directory = FPaths::ProjectSavedDir() / TEXT("CollisionGenerator");
				IFileManager::Get().MakeDirectory(*Directory, true);
				Test->TestTrue(TEXT("Update UI evidence saved"), FFileHelper::SaveArrayToFile(PNG, *(Directory / TEXT("Update_UI.png"))));
			}
			Second->GetOutermost()->GetMetaData().RemoveValue(Second.Get(), TEXT("HALCollision.Version"));
			Load->SimulateClick();
			Test->TestFalse(TEXT("Failed subsequent load clears old update target"), UpdateButton->IsEnabled());
			return true;
		}
	private:
		FAutomationTestBase* Test;
		TSharedPtr<SDockTab> Tab;
		TStrongObjectPtr<UStaticMesh> First, Second;
		TArray<FAssetData> PreviousSelection;
		FGuid FirstGuid, SecondGuid;
		double Start;
		int32 Stage = 0;
	};

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALCollisionUpdateUITest, "HAL.EditorUI.CollisionGenerator.UpdateLoadedTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALCollisionUpdateUITest::RunTest(const FString&)
{
	if (!FApp::CanEverRender() || !FSlateApplication::IsInitialized()) { AddError(TEXT("Run in a rendering Editor without NullRHI.")); return false; }
	TStrongObjectPtr<UHALVehicleCollisionGeneratorSettings> Settings(NewObject<UHALVehicleCollisionGeneratorSettings>());
	Settings->OutputFolder = TEXT("/Game/__HALCollisionUI_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	FString Error;
	UStaticMesh* First = HALVehicleCollision::CreateAsset(*Settings, Error);
	UStaticMesh* Second = HALVehicleCollision::CreateAsset(*Settings, Error);
	if (!TestTrue(TEXT("Unsaved temporary UI fixtures generated"), First && Second)) { AddError(Error); return false; }
	const auto Tab = FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("HALVehicleCollisionGenerator")));
	if (!TestTrue(TEXT("Update panel opens"), Tab.IsValid())) { return false; }
	auto& Browser = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get();
	TArray<FAssetData> PreviousSelection; Browser.GetSelectedAssets(PreviousSelection);
	Browser.SyncBrowserToAssets(TArray<UObject*>{ First });
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FExerciseCollisionUpdate>(this, Tab, First, Second, PreviousSelection));
	return true;
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
