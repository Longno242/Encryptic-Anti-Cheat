using UnrealBuildTool;
using System.IO;

public class SentinelAntiCheat : ModuleRules
{
    public SentinelAntiCheat(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string ThirdPartyPath = Path.Combine(ModuleDirectory, "ThirdParty", "sentinel", "bin", "Win64");
            PublicDelayLoadDLLs.Add("sentinel_core.dll");
            RuntimeDependencies.Add(Path.Combine(ThirdPartyPath, "sentinel_core.dll"));
        }
    }
}
