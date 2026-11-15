using UnrealBuildTool;

public class HAL_Future_CreationEditor : ModuleRules
{
	public HAL_Future_CreationEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Authoring commandlets use translation-unit-local helpers with overlapping names.
		// Keep their isolation when committed files re-enter UBT's non-adaptive working set.
		bUseUnity = false;
		// Existing exported runtime headers use the original flat module layout.
		PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "..", "HAL_Future_Creation"));
		PrivateDependencyModuleNames.AddRange(new[] {
			"Core", "CoreUObject", "Engine", "PhysicsCore", "UnrealEd",
			"Slate", "SlateCore", "PropertyEditor", "ToolMenus",
			"AssetRegistry", "AssetTools", "ContentBrowser", "MeshDescription", "StaticMeshDescription", "AudioEditor",
			"SkeletalMeshDescription", "AnimationCore", "GeometryCore", "Niagara", "NiagaraEditor", "HAL_Future_Creation"
		});
	}
}
