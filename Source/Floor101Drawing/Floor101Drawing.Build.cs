using UnrealBuildTool;

// Pure C++20 module (ADR-0002, ADR-0015): plan drawings of the model and their SVG. Only
// Private/Floor101DrawingModule.cpp uses Unreal (module registration).
public class Floor101Drawing : ModuleRules
{
	public Floor101Drawing(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Floor101CadCore", "Floor101Geometry" });
		PrivateDependencyModuleNames.Add("Core");
	}
}
