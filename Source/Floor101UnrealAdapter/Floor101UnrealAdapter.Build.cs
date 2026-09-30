using UnrealBuildTool;

// Boundary between the neutral scene and Unreal (docs/architecture.md, ADR-0005). Knows the scene, never the CAD model.
public class Floor101UnrealAdapter : ModuleRules
{
	public Floor101UnrealAdapter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Floor101SceneModel" });
		PrivateDependencyModuleNames.AddRange(new string[] { "GeometryCore", "GeometryFramework", "ImageCore" });
	}
}
