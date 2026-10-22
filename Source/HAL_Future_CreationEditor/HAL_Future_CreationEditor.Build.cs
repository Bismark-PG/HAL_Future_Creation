using UnrealBuildTool;

public class HAL_Future_CreationEditor : ModuleRules
{
	public HAL_Future_CreationEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Existing exported runtime headers use the original flat module layout.
		PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "..", "HAL_Future_Creation"));
		PrivateDependencyModuleNames.AddRange(new[] {
			"Core", "CoreUObject", "Engine", "PhysicsCore", "UnrealEd",
			"Slate", "SlateCore", "PropertyEditor", "ToolMenus",
			"AssetRegistry", "AssetTools", "ContentBrowser", "MeshDescription", "StaticMeshDescription",
			"SkeletalMeshDescription", "AnimationCore", "HAL_Future_Creation"
		});
	}
}
