using UnrealBuildTool;
using System.Collections.Generic;

public class PuzzleGame5x5Target : TargetRules
{
	public PuzzleGame5x5Target(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("PuzzleGame5x5");
	}
}
