// Copyright PlayhouseCity. All Rights Reserved.

using UnrealBuildTool;

public class PlayhouseCity : ModuleRules
{
	public PlayhouseCity(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivatePCHHeaderFile = "PlayhouseCity.h";
		CppStandard = CppStandardVersion.Cpp20;

		// Suppress MSVC warnings that Clang (iOS/Android) allows
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDefinitions.Add("_CRT_SECURE_NO_WARNINGS");
		}
		bWarningsAsErrors = false;

		// Suppress shadow variable warnings (C4456/C4457/C4458/C4459 on MSVC)
		// These are allowed by Clang on iOS/Android but MSVC treats them as errors
		CppCompileWarningSettings.ShadowVariableWarningLevel = WarningLevel.Off;
		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Off;

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
			"Networking"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});

		DynamicallyLoadedModuleNames.AddRange(new string[]
		{
		});
	}
}
