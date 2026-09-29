using UnrealBuildTool;

// Editor-only development tools: exposes the 101st Floor document to Unreal MCP (docs/unreal-mcp.md).
// Never part of the game build; product logic stays in the CAD Core.
public class Floor101Editor : ModuleRules
{
	public Floor101Editor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "ToolsetRegistry" });
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"EditorSubsystem", "UnrealEd", "AssetRegistry",
			"Floor101", "Floor101CadCore", "Floor101Geometry", "Floor101SceneModel",
		});
	}
}
