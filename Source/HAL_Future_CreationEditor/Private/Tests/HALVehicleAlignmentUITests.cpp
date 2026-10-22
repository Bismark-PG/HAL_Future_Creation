#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "VehicleDefinition.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SWindow.h"

namespace
{
	class FCaptureAlignment : public IAutomationLatentCommand
	{
	public:
		FCaptureAlignment(FAutomationTestBase* InTest, TSharedPtr<SDockTab> InTab, const TArray<FAssetData>& InSelection)
			: Test(InTest), Tab(InTab), Selection(InSelection), Start(FPlatformTime::Seconds()) {}
		virtual bool Update() override
		{
			if (FPlatformTime::Seconds()-Start < 1) { return false; }
			TArray<FColor> Pixels; FIntVector Size;
			if (Test->TestTrue(TEXT("Calibration panel screenshot"),FSlateApplication::Get().TakeScreenshot(Tab->GetContent(),Pixels,Size)))
			{
				Test->TestTrue(TEXT("Usable measurement/preview layout"),Size.X >= 1000 && Size.Y >= 500);
				TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);
				const FString Directory = FPaths::ProjectSavedDir()/TEXT("VehicleAlignment/20261022");
				IFileManager::Get().MakeDirectory(*Directory,true);
				Test->TestTrue(TEXT("Calibration UI proof saved"),FFileHelper::SaveArrayToFile(PNG,*(Directory/TEXT("Panel_UI.png"))));
			}
			Tab->RequestCloseTab();
			if (!Selection.IsEmpty()) { FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().SyncBrowserToAssets(Selection); }
			return true;
		}
	private:
		FAutomationTestBase* Test;
		TSharedPtr<SDockTab> Tab;
		TArray<FAssetData> Selection;
		double Start;
	};
	class FOpenAlignmentAfterSelection : public IAutomationLatentCommand
	{
	public:
		FOpenAlignmentAfterSelection(FAutomationTestBase* InTest, const TArray<FAssetData>& InSelection)
			: Test(InTest), Selection(InSelection), Start(FPlatformTime::Seconds()) {}
		virtual bool Update() override
		{
			// SyncBrowserToAssets queues a list refresh; opening the tool in the same frame sees the old selection.
			if (FPlatformTime::Seconds()-Start < 1) { return false; }
			TArray<FAssetData> Selected;
			FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().GetSelectedAssets(Selected);
			if (!Test->TestTrue(TEXT("Content Browser vehicle selection settled"),Selected.Num() == 1 && Selected[0].GetAsset()->IsA<UVehicleDefinition>())) { return true; }
			const auto Tab = FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("HALVehicleAlignment")));
			if (!Test->TestTrue(TEXT("Alignment native tool opened"),Tab.IsValid())) { return true; }
			// Use a real, sufficiently large physical client area rather than assuming one logical unit is one pixel.
			if (const auto Window = FSlateApplication::Get().FindWidgetWindow(Tab.ToSharedRef())) { Window->Resize(FVector2D(1380,860)); }
			FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCaptureAlignment>(Test,Tab,Selection));
			return true;
		}
	private:
		FAutomationTestBase* Test;
		TArray<FAssetData> Selection;
		double Start;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALAlignmentUITest,"HAL.EditorUI.VehicleAlignment.Panel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALAlignmentUITest::RunTest(const FString&)
{
	if (!FApp::CanEverRender() || !FSlateApplication::IsInitialized()) { AddError(TEXT("Run in a rendering Editor without NullRHI.")); return false; }
	UToolMenu* Menu = UToolMenus::Get()->FindMenu(TEXT("LevelEditor.MainMenu.Tools"));
	if (!TestNotNull(TEXT("Tools menu exists"),Menu)) { return false; }
	const FToolMenuSection* Section = Menu->FindSection(TEXT("HALTools"));
	if (!TestNotNull(TEXT("HAL tools section"),Section)) { return false; }
	TestNotNull(TEXT("Alignment menu entry"),Section->FindEntry(TEXT("HALVehicleAlignment")));
	auto* Vehicle = LoadObject<UVehicleDefinition>(nullptr,TEXT("/Game/Data/MVP_B/DA_Vehicle_Player_TestBaseline_v1.DA_Vehicle_Player_TestBaseline_v1"));
	if (!TestNotNull(TEXT("Select actual vehicle definition"),Vehicle)) { return false; }
	auto& Browser = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get();
	TArray<FAssetData> Selection; Browser.GetSelectedAssets(Selection);
	Browser.SyncBrowserToAssets(TArray<UObject*>{Vehicle});
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FOpenAlignmentAfterSelection>(this,Selection));
	return true;
}
#endif
