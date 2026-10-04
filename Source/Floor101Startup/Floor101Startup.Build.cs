using UnrealBuildTool;

// Startup presentation loads before the product modules, without CAD or UObject assets.
public class Floor101Startup : ModuleRules
{
	public Floor101Startup(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "SlateCore" });
		PrivateDependencyModuleNames.AddRange(new[] { "Slate", "PreLoadScreen" });
		RuntimeDependencies.Add("$(ProjectDir)/Content/Splash/Splash.png", StagedFileType.NonUFS);
	}
}
