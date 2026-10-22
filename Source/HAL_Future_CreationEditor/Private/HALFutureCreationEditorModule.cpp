#include "Modules/ModuleManager.h"

#include "SHALVehicleCollisionGenerator.h"
#include "SHALVehicleAlignment.h"
#include "VehicleAssetGenerator/HALVehicleAssetGenerator.h"
#include "Framework/Docking/TabManager.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "HALFutureCreationEditor"

class FHALFutureCreationEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (IsRunningCommandlet() || IsRunningGame()) { return; }
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TEXT("HALVehicleCollisionGenerator"),
			FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&) {
				return SNew(SDockTab).TabRole(ETabRole::NomadTab) [ MakeHALVehicleCollisionGenerator() ];
			}))
			.SetDisplayName(LOCTEXT("Tab", "HAL Vehicle Collision Generator"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TEXT("HALVehicleAlignment"),
			FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&) {
				return SNew(SDockTab).TabRole(ETabRole::NomadTab) [ MakeHALVehicleAlignment() ];
			}))
			.SetDisplayName(LOCTEXT("AlignmentTab", "HAL Vehicle Visual Alignment"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TEXT("HALVehicleAssetGenerator"),
			FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&) {
				return SNew(SDockTab).TabRole(ETabRole::NomadTab) [ HALVehicleAssets::MakePanel() ];
			}))
			.SetDisplayName(LOCTEXT("AssetTab", "HAL Vehicle Asset Generator"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);
		bRegisteredUI = true;
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FHALFutureCreationEditorModule::RegisterMenus));
	}
	virtual void ShutdownModule() override
	{
		if (!bRegisteredUI) { return; }
		UToolMenus::UnRegisterStartupCallback(this);
		if (UToolMenus::IsToolMenuUIEnabled()) { UToolMenus::UnregisterOwner(this); }
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("HALVehicleCollisionGenerator"));
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("HALVehicleAlignment"));
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("HALVehicleAssetGenerator"));
	}
private:
	bool bRegisteredUI = false;
	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
		Menu->FindOrAddSection(TEXT("HALTools"), LOCTEXT("Section", "HAL Tools")).AddMenuEntry(
			TEXT("HALVehicleCollisionGenerator"), LOCTEXT("Entry", "HAL Vehicle Collision Generator"),
			LOCTEXT("Hint", "Generate a flat convex collision mesh from vehicle dimensions and corner radii."), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([] { FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("HALVehicleCollisionGenerator"))); })));
		Menu->FindOrAddSection(TEXT("HALTools")).AddMenuEntry(
			TEXT("HALVehicleAlignment"), LOCTEXT("AlignmentEntry", "HAL Vehicle Visual Alignment"),
			LOCTEXT("AlignmentHint", "Measure reference wheel geometry and preview an undoable visual height alignment."), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([] { FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("HALVehicleAlignment"))); })));
		Menu->FindOrAddSection(TEXT("HALTools")).AddMenuEntry(
			TEXT("HALVehicleAssetGenerator"), LOCTEXT("AssetEntry", "HAL Vehicle Asset Generator"),
			LOCTEXT("AssetHint", "Create the vehicle Definition, Presentation, empty AnimBP and vehicle Blueprint for SKM_<model>."), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([] { FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("HALVehicleAssetGenerator"))); })));
	}
};

IMPLEMENT_MODULE(FHALFutureCreationEditorModule, HAL_Future_CreationEditor)

#undef LOCTEXT_NAMESPACE
