/**
 * @file SavageSuperStormEditor.Build.cs
 * @brief Declares dependencies for the storm editor module.
 */

using UnrealBuildTool;

public class SavageSuperStormEditor : ModuleRules
{
	public SavageSuperStormEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"AppFramework",
			"AssetDefinition",
			"AssetRegistry",
			"ContentBrowser",
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"LevelEditor",
			"Projects",
			"PropertyEditor",
			"RenderCore",
			"RHI",
			"SavageSuperStormRuntime",
			"Slate",
			"SlateCore",
			"UnrealEd"
		});
	}
}
