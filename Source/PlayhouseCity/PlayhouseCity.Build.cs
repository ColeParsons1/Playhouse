// Copyright PlayhouseCity. All Rights Reserved.

using UnrealBuildTool;

public class PlayhouseCity : ModuleRules
{
	public PlayhouseCity(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivatePCHHeaderFile = "PlayhouseCity.h";
		CppStandard = CppStandardVersion.Cpp20;

		// Allow subdirectory includes like "Games/Base/PHCMiniGameMode.h"
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"Slate",
			"SlateCore",
			"HTTP",
			"Json",
			"JsonUtilities",
			"WebSockets",
			"Paper2D",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"PhysicsCore",
			"NavigationSystem",
			"GameplayTags",
			"Networking",
			"ProceduralMeshComponent",
			"AudioMixer",
			"AudioCapture",
			"Voice"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicFrameworks.Add("AuthenticationServices");
			PublicFrameworks.Add("UserNotifications");
			PublicFrameworks.Add("AVFoundation");
		}

		DynamicallyLoadedModuleNames.AddRange(new string[]
		{
		});
	}
}
