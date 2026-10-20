using UnrealBuildTool;

public class HAL_Future_CreationEditor : ModuleRules
{
	public HAL_Future_CreationEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new[] {
			"Core", "CoreUObject", "Engine", "PhysicsCore", "UnrealEd",
			"Slate", "SlateCore", "PropertyEditor", "ToolMenus",
			"AssetRegistry", "AssetTools", "ContentBrowser", "MeshDescription", "StaticMeshDescription"
		});
	}
}
