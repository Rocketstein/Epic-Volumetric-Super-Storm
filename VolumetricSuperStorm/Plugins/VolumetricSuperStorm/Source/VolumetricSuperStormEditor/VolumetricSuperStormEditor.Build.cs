// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file VolumetricSuperStormEditor.Build.cs
 * @brief Declares dependencies for the storm editor module.
 */

using UnrealBuildTool;

public class VolumetricSuperStormEditor : ModuleRules
{
	public VolumetricSuperStormEditor(ReadOnlyTargetRules Target) : base(Target)
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
			"DeveloperSettings",
			"Engine",
			"InputCore",
			"LevelEditor",
			"Projects",
			"PropertyEditor",
			"RenderCore",
			"RHI",
			"VolumetricSuperStormRuntime",
			"Slate",
			"SlateCore",
			"UnrealEd"
		});
	}
}
