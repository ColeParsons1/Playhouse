// Copyright PlayhouseCity. All Rights Reserved.

using UnrealBuildTool;

public class PlayhouseCity : ModuleRules
{
	public PlayhouseCity(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivatePCHHeaderFile = "PlayhouseCity.h";
		CppStandard = CppStandardVersion.Cpp20;

		// Suppress MSVC warnings treated as errors (project developed on Clang/Mac)
		UnsafeTypeCastWarningLevel = WarningLevel.Off;
		bEnableUndefinedIdentifierWarnings = false;
		ShadowVariableWarningLevel = WarningLevel.Off;

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

		// Exclude iOS-only Objective-C++ files on non-iOS platforms
		if (Target.Platform != UnrealTargetPlatform.IOS)
		{
			var iosDir = System.IO.Path.Combine(ModuleDirectory, "Voice/IOS");
			if (System.IO.Directory.Exists(iosDir))
			{
				foreach (var f in System.IO.Directory.GetFiles(iosDir, "*.mm"))
				{
					PublicDefinitions.Add("AGORA_VOICE_DISABLED=1");
				}
			}
		}

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicFrameworks.Add("AuthenticationServices");
			PublicFrameworks.Add("UserNotifications");
			PublicFrameworks.Add("AVFoundation");

			// Agora Voice SDK — all 5 required frameworks embedded
			string AgoraPath = System.IO.Path.Combine(ModuleDirectory, "../../ThirdParty/Agora/iOS");
			PublicAdditionalFrameworks.Add(new Framework("AgoraRtcKit",
				System.IO.Path.Combine(AgoraPath, "AgoraRtcKit.embeddedframework.zip"), "", true));
			PublicAdditionalFrameworks.Add(new Framework("aosl",
				System.IO.Path.Combine(AgoraPath, "aosl.embeddedframework.zip"), "", true));
			PublicAdditionalFrameworks.Add(new Framework("Agoraffmpeg",
				System.IO.Path.Combine(AgoraPath, "Agoraffmpeg.embeddedframework.zip"), "", true));
			PublicAdditionalFrameworks.Add(new Framework("Agorafdkaac",
				System.IO.Path.Combine(AgoraPath, "Agorafdkaac.embeddedframework.zip"), "", true));
			PublicAdditionalFrameworks.Add(new Framework("AgoraSoundTouch",
				System.IO.Path.Combine(AgoraPath, "AgoraSoundTouch.embeddedframework.zip"), "", true));
		}

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicDependencyModuleNames.Add("Launch");
		}

		DynamicallyLoadedModuleNames.AddRange(new string[]
		{
		});
	}
}
