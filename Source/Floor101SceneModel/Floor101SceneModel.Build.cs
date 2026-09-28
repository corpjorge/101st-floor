using UnrealBuildTool;

// Pure C++20 module (ADR-0002, ADR-0003): only Private/Floor101SceneModelModule.cpp uses Unreal (module registration).
public class Floor101SceneModel : ModuleRules
{
	public Floor101SceneModel(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Floor101CadCore", "Floor101Geometry" });
		PrivateDependencyModuleNames.Add("Core");
	}
}
