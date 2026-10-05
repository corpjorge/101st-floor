using UnrealBuildTool;

public class Floor101Target : TargetRules
{
	public Floor101Target(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("Floor101");
		// Installed Shipping engine libraries strip UE logging; product diagnostics are independent (ADR-0028).
	}
}
