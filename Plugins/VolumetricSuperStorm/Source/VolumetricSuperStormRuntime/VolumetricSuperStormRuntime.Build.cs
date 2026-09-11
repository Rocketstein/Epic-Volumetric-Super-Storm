// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file VolumetricSuperStormRuntime.Build.cs
 * @brief Declares runtime module dependencies.
 */

using UnrealBuildTool;

public class VolumetricSuperStormRuntime : ModuleRules
{
	public VolumetricSuperStormRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Landscape",
			"RHI",
			"RenderCore",
			"Renderer",
			"VolumetricSuperStormShaders"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("Projects");
		}
	}
}
