#include "Modules/ModuleManager.h"

#include "SHALVehicleCollisionGenerator.h"
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
		bRegisteredUI = true;
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FHALFutureCreationEditorModule::RegisterMenus));
	}
	virtual void ShutdownModule() override
	{
		if (!bRegisteredUI) { return; }
		UToolMenus::UnRegisterStartupCallback(this);
		if (UToolMenus::IsToolMenuUIEnabled()) { UToolMenus::UnregisterOwner(this); }
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("HALVehicleCollisionGenerator"));
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
	}
};

IMPLEMENT_MODULE(FHALFutureCreationEditorModule, HAL_Future_CreationEditor)

#undef LOCTEXT_NAMESPACE
