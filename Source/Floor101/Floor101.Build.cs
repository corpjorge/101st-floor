using UnrealBuildTool;

public class Floor101 : ModuleRules
{
	public Floor101(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		// RHI/RenderCore are only used by the development render check (Private/RenderCheck.cpp).
		PrivateDependencyModuleNames.AddRange(new string[] { "RHI", "RenderCore" });
	}
}
