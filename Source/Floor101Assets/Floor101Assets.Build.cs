using System.IO;
using UnrealBuildTool;

// Pure C++20 module (ADR-0002, ADR-0017): reads imported textures and models (PNG and JPEG headers, glTF 2.0, OBJ) and
// copies their files into the project. Only Private/Floor101AssetsModule.cpp uses Unreal (module registration).
public class Floor101Assets : ModuleRules
{
	public Floor101Assets(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Floor101CadCore", "Floor101Geometry" });
		PrivateDependencyModuleNames.Add("Core");

		// Header-only third-party libraries (nlohmann/json for glTF).
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "..", "ThirdParty"));
	}
}
