using UnrealBuildTool;

public class EchosTriforce : ModuleRules
{
	public EchosTriforce(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"Slate",
			"SlateCore",
			"Json",
			"JsonUtilities",
			"Niagara"
		});

		PublicIncludePaths.AddRange(new string[] {
			"EchosTriforce",
			"EchosTriforce/Core",
			"EchosTriforce/Game",
			"EchosTriforce/UI"
		});
	}
}
