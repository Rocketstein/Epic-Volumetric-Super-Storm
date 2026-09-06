/**
 * @file SavageSuperStormRuntime.Build.cs
 * @brief Declares runtime module dependencies.
 */

using UnrealBuildTool;

public class SavageSuperStormRuntime : ModuleRules
{
	public SavageSuperStormRuntime(ReadOnlyTargetRules Target) : base(Target)
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
			"SavageSuperStormShaders"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("Projects");
		}
	}
}
