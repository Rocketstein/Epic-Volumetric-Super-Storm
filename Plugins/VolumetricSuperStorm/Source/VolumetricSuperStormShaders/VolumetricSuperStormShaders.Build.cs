// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file VolumetricSuperStormShaders.Build.cs
 * @brief Declares shader module dependencies.
 */

using UnrealBuildTool;

public class VolumetricSuperStormShaders : ModuleRules
{
	public VolumetricSuperStormShaders(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"RHI",
			"RenderCore"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"CoreUObject",
			"Engine",
			"Projects"
		});
	}
}
