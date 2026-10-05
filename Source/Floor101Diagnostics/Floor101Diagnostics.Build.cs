using UnrealBuildTool;

public class Floor101Diagnostics : ModuleRules
{
	public Floor101Diagnostics(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.Add("Core");
	}
}
