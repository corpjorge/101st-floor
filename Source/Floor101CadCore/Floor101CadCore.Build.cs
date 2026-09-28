using UnrealBuildTool;

// Pure C++20 module (ADR-0002): only Private/Floor101CadCoreModule.cpp uses Unreal (module registration).
// The same sources are compiled by Tests/Unit/CMakeLists.txt without Unreal.
public class Floor101CadCore : ModuleRules
{
	public Floor101CadCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		PrivateDependencyModuleNames.Add("Core");
	}
}
