using System.IO;
using UnrealBuildTool;

// FastNoise2 v1.1.1 (MIT, https://github.com/Auburn/FastNoise2): SIMD noise with runtime
// SSE2/SSE4.1/AVX2/AVX-512 dispatch. Built as static libs with CMake + MSVC (/MD) for Win64;
// an iOS build of the library has to be made on a Mac, so the game only links it on Win64.
public class FastNoise2 : ModuleRules
{
	public FastNoise2(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));
		PublicDefinitions.Add("FASTNOISE_STATIC_LIB=1");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string LibDir = Path.Combine(ModuleDirectory, "lib", "Win64");
			PublicAdditionalLibraries.Add(Path.Combine(LibDir, "FastNoise.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(LibDir, "FastSIMD_FastNoise.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(LibDir, "FastSIMD.lib"));
		}
	}
}
