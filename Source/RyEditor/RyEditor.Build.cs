// Copyright 2020 Sheffer Online Services.
// MIT License. See LICENSE for details.

using UnrealBuildTool;

public class RyEditor : ModuleRules
{
    public RyEditor(ReadOnlyTargetRules Target) : base(Target)
    {
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.AddRange(
			new string[] 
            {
				"RyEditor/Private",
            }
		);
        PublicDependencyModuleNames.AddRange(
			new string[]
			{
                "PropertyEditor",
                "EditorStyle",
                "LevelSequence",
            }
		);
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "Core",
                "CoreUObject",
                "Slate",
                "SlateCore",
                "Engine",
                "InputCore",
                "UnrealEd",
                "AnimGraph",
                "BlueprintGraph",
                "AnimGraphRuntime",
				"AssetRegistry",
				"RenderCore",
				"UMGEditor",
				"RyRuntime",
				"DesktopPlatform"
			}
		);
	}
}
