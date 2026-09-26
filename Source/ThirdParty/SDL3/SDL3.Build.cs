using System.IO;
using UnrealBuildTool;

public class SDL3 : ModuleRules
{
	public SDL3(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// Only Win64 binaries are bundled. Other platforms (iOS) compile the gamepad
		// subsystem as a no-op and rely on touch input instead.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string SdlRoot = ModuleDirectory;
			PublicSystemIncludePaths.Add(Path.Combine(SdlRoot, "include"));

			string LibDir = Path.Combine(SdlRoot, "lib", "x64");
			PublicAdditionalLibraries.Add(Path.Combine(LibDir, "SDL3.lib"));

			string DllName = "SDL3.dll";
			PublicDelayLoadDLLs.Add(DllName);
			RuntimeDependencies.Add(Path.Combine("$(BinaryOutputDir)", DllName), Path.Combine(LibDir, DllName));

			PublicDefinitions.Add("WITH_SDL3=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_SDL3=0");
		}
	}
}
