using UnrealBuildTool;
using System.Collections.Generic;

public class PuzzleGame5x5EditorTarget : TargetRules
{
	public PuzzleGame5x5EditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("PuzzleGame5x5");
	}
}
