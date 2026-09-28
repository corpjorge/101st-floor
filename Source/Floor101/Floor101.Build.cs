using UnrealBuildTool;

// Application module: owns the document (CAD model being edited), drives scene compilation and the Unreal adapter.
// UI (Slate) will live here (MVP 1).
public class Floor101 : ModuleRules
{
	public Floor101(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Floor101CadCore", "Floor101SceneModel", "Floor101UnrealAdapter",
			// RHI/RenderCore are only used by the development render check (Private/RenderCheck.cpp).
			"RHI", "RenderCore",
		});
	}
}
