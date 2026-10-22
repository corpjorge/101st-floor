using UnrealBuildTool;

// Application module: owns the document (CAD model being edited), drives scene compilation and the Unreal adapter,
// and turns user input into editing calls (ADR-0008).
public class Floor101 : ModuleRules
{
	public Floor101(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Floor101Diagnostics", "Core", "CoreUObject", "Engine", "InputCore" });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Floor101CadCore", "Floor101Geometry", "Floor101SceneModel", "Floor101Drawing", "Floor101Assets", "Floor101Exporters", "Floor101UnrealAdapter",
			"Slate", "SlateCore", "ApplicationCore", "Floor101Startup",
			"GeometryFramework", "GeometryCore",
			// Plan PNG export (ADR-0015): widgets rendered offscreen and saved as an image.
			"UMG", "ImageCore",
			// RHI/RenderCore: the development render check (Private/RenderCheck.cpp) and the offscreen plan render.
			"RHI", "RenderCore", "Renderer",
			// Json: the integration tests read an exported glTF back (ADR-0020).
			"Json",
			// HTTP: updates from the project's GitHub releases (ADR-0039).
			"HTTP",
		});

		// Importing assets (ADR-0017): the system's open-file dialog.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.Add("comdlg32.lib");
			// Windows CNG: SHA-256 and the ECDSA signature of updates (ADR-0039).
			PublicSystemLibraries.Add("bcrypt.lib");
		}
	}
}
