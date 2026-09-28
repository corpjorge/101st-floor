using UnrealBuildTool;

// Pure C++20 module (ADR-0002): only Private/Floor101GeometryModule.cpp uses Unreal (module registration).
public class Floor101Geometry : ModuleRules
{
	public Floor101Geometry(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		PublicDependencyModuleNames.Add("Floor101CadCore");
		PrivateDependencyModuleNames.Add("Core");
	}
}
