using UnrealBuildTool;

// meshoptimizer 1.3 (MIT, https://github.com/zeux/meshoptimizer), compiled from source so it
// builds for every platform (including iOS) with the project. UBT defines MESHOPTIMIZER_API as
// the module's export macro (DLLEXPORT/DLLIMPORT), which meshoptimizer.h picks up; the PCH
// brings in the platform header that defines those macros for the library's own .cpp files.
public class MeshOptimizer : ModuleRules
{
	public MeshOptimizer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivatePCHHeaderFile = "Private/MeshOptimizerPCH.h";
		bUseUnity = false;
		UndefinedIdentifierWarningLevel = WarningLevel.Off;
		CppCompileWarningSettings.ShadowVariableWarningLevel = WarningLevel.Off;
		PublicDependencyModuleNames.Add("Core");
	}
}
