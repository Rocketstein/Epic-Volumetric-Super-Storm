/**
 * @file SavageSuperStormShaders.Build.cs
 * @brief Declares shader module dependencies.
 */

using UnrealBuildTool;

public class SavageSuperStormShaders : ModuleRules
{
	public SavageSuperStormShaders(ReadOnlyTargetRules Target) : base(Target)
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
