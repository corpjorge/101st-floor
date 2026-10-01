using System.IO;
using UnrealBuildTool;

// Pure C++20 module (ADR-0002, ADR-0020): writes the neutral scene to exchange formats (glTF 2.0 for Twinmotion) with its
// own PNG encoder. Only Private/Floor101ExportersModule.cpp uses Unreal (module registration).
public class Floor101Exporters : ModuleRules
{
	public Floor101Exporters(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Floor101CadCore", "Floor101Geometry", "Floor101SceneModel", "Floor101Assets" });
		PrivateDependencyModuleNames.Add("Core");

		// Header-only third-party libraries (nlohmann/json for the glTF document).
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "..", "ThirdParty"));
	}
}
