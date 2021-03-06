// Copyright 2020-2021 Sheffer Online Services.
// MIT License. See LICENSE for details.

using UnrealBuildTool;
using System.IO;

public class RyRuntime : ModuleRules
{
    public RyRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Turn this off if you want to dis-include dangerous functions from blueprint.
		// This blocks functions which access engine internals which only advanced engine users should consider.
		PrivateDefinitions.Add("RY_INCLUDE_DANGEROUS_FUNCTIONS=1");
		
		//OptimizeCode = CodeOptimization.Never;

		PrivateIncludePaths.AddRange(
            new string[]
            {
                "RyRuntime/Private",
            }
        );
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"HeadMountedDisplay",
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
				"NavigationSystem",
				"AIModule",
				"UMG",
				"ApplicationCore",
				"Voice",
				"EngineSettings",
            }
        );
        
        /* Add to plugins section for messing with getting some extra HMD info
        	"Plugins": [
		{
			"Name": "SteamVR",
			"Enabled": true,
			"Optional": true,
			"WhitelistPlatforms": [
				"Win32", "Win64", "Linux", "Mac"
			]
		},
		{
			"Name": "OculusVR",
			"Enabled": true,
			"Optional": true,
			"WhitelistPlatforms": [
				"Win32", "Win64", "Android"
			]
		}
	]
         */

        if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
        {
	        /*PrivateDependencyModuleNames.AddRange(
		        new string[]
		        {
			        "SteamVR",
			        "OculusHMD",
		        }
	        );*/
	        AddEngineThirdPartyPrivateStaticDependencies(Target, "DirectSound");
	        //AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenVR");
	        
	        PrivateIncludePaths.AddRange(
		        new string[]
		        {
			        Path.GetFullPath(Path.Combine(EngineDirectory, "Source/Runtime/Online/Voice/Private")),
		        }
	        );
        }

        // if (Target.Platform.IsInGroup(UnrealPlatformGroup.Android))
        // {
	       //  PrivateDependencyModuleNames.AddRange(
		      //   new string[]
		      //   {
			     //    "OculusHMD",
		      //   }
	       //  );
        // }
    }
}
