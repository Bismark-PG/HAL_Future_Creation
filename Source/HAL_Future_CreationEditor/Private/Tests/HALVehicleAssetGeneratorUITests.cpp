#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

namespace
{
	class FCaptureVehicleAssetGenerator : public IAutomationLatentCommand
	{
	public:
		FCaptureVehicleAssetGenerator(FAutomationTestBase* InTest, TSharedPtr<SDockTab> InTab)
			: Test(InTest), Tab(InTab), Start(FPlatformTime::Seconds()) {}
		bool Update() override
		{
			if (FPlatformTime::Seconds() - Start < 1) { return false; }
			TArray<FColor> Pixels;
			FIntVector Size;
			if (Test->TestTrue(TEXT("Tool panel rendered"), FSlateApplication::Get().TakeScreenshot(Tab->GetContent(), Pixels, Size)))
			{
				Test->TestTrue(TEXT("Usable panel area"), Size.X >= 700 && Size.Y >= 400);
				TArray64<uint8> PNG;
				FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, PNG);
				const FString Directory = FPaths::ProjectSavedDir() / TEXT("VehicleAssetGenerator");
				IFileManager::Get().MakeDirectory(*Directory, true);
				Test->TestTrue(TEXT("UI evidence saved"), FFileHelper::SaveArrayToFile(PNG, *(Directory / TEXT("Panel_UI.png"))));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALVehicleAssetGeneratorUITest, "HAL.EditorUI.VehicleAssetGenerator.Panel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALVehicleAssetGeneratorUITest::RunTest(const FString&)
{
	if (!FApp::CanEverRender() || !FSlateApplication::IsInitialized())
	{
		AddError(TEXT("Run the panel test in a rendering Editor, without -NullRHI."));
		return false;
	}
	UToolMenu* Menu = UToolMenus::Get()->FindMenu(TEXT("LevelEditor.MainMenu.Tools"));
	if (!TestNotNull(TEXT("Tools menu registered"), Menu)) { return false; }
	const FToolMenuSection* Section = Menu->FindSection(TEXT("HALTools"));
	if (!TestNotNull(TEXT("HAL tools section"), Section)) { return false; }
	TestNotNull(TEXT("Vehicle generator menu entry"), Section->FindEntry(TEXT("HALVehicleAssetGenerator")));
	TestNotNull(TEXT("Existing collision tool retained"), Section->FindEntry(TEXT("HALVehicleCollisionGenerator")));
	TestNotNull(TEXT("Synced visual alignment tool retained"), Section->FindEntry(TEXT("HALVehicleAlignment")));
	TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("HALVehicleAssetGenerator")));
	if (!TestTrue(TEXT("Vehicle generator tab opens"), Tab.IsValid())) { return false; }
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCaptureVehicleAssetGenerator>(this, Tab));
	return true;
}

#endif
