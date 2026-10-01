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
			"Floor101CadCore", "Floor101Geometry", "Floor101SceneModel", "Floor101Drawing", "Floor101Assets", "Floor101Exporters", "Floor101UnrealAdapter",
			"InputCore", "Slate", "SlateCore",
			// Plan PNG export (ADR-0015): widgets rendered offscreen and saved as an image.
			"UMG", "ImageCore",
			// RHI/RenderCore: the development render check (Private/RenderCheck.cpp) and the offscreen plan render.
			"RHI", "RenderCore",
			// Json: the integration tests read an exported glTF back (ADR-0020).
			"Json",
		});

		// Importing assets (ADR-0017): the system's open-file dialog.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.Add("comdlg32.lib");
		}
	}
}
