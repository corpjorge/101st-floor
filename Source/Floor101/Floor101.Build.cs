using UnrealBuildTool;

// Application module: owns the document (CAD model being edited), drives scene compilation and the Unreal adapter,
// and turns user input into editing calls (ADR-0008).
public class Floor101 : ModuleRules
{
	public Floor101(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Floor101CadCore", "Floor101Geometry", "Floor101SceneModel", "Floor101UnrealAdapter",
			"InputCore", "Slate", "SlateCore",
			// RHI/RenderCore are only used by the development render check (Private/RenderCheck.cpp).
			"RHI", "RenderCore",
		});
	}
}
