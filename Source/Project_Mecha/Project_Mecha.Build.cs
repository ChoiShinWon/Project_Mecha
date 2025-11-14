using UnrealBuildTool;

public class Project_Mecha : ModuleRules
{
    public Project_Mecha(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core","CoreUObject","Engine","InputCore","EnhancedInput",
            "GameplayAbilities","GameplayTasks","GameplayTags", "UMG", "Slate", "SlateCore"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });
    }
}
