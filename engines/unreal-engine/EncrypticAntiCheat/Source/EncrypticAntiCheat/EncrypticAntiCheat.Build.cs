using UnrealBuildTool;
using System.IO;

public class EncrypticAntiCheat : ModuleRules
{
    public EncrypticAntiCheat(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string ThirdPartyPath = Path.Combine(ModuleDirectory, "ThirdParty", "encryptic", "bin", "Win64");
            PublicDelayLoadDLLs.Add("encryptic_core.dll");
            RuntimeDependencies.Add(Path.Combine(ThirdPartyPath, "encryptic_core.dll"));
        }
    }
}
