using UnrealBuildTool;

public class PuzzleGame5x5 : ModuleRules
{
	public PuzzleGame5x5(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"SDL3",
			"ProceduralMeshComponent",
			"RealtimeMeshComponent",
			"MeshOptimizer",
			"AudioMixer",
			"Synthesis"
		});

		// FastNoise2 is prebuilt for Win64 only (an iOS build of it needs a Mac); elsewhere the
		// game falls back to FMath::PerlinNoise.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.Add("FastNoise2");
			PublicDefinitions.Add("WITH_FASTNOISE2=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_FASTNOISE2=0");
		}
	}
}
