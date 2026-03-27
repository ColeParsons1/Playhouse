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
			"Voice",
			"Niagara"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});

		// Agora voice: .mm files are guarded by PLATFORM_IOS / PLATFORM_MAC internally
		if (Target.Platform != UnrealTargetPlatform.IOS && Target.Platform != UnrealTargetPlatform.Mac)
		{
			PublicDefinitions.Add("AGORA_VOICE_DISABLED=1");
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

			// Agora C++ SDK for Android
			PublicDefinitions.Add("AGORA_NATIVE_CPP=1");
			string AgoraAndroidPath = System.IO.Path.Combine(ModuleDirectory, "../../ThirdParty/Agora/Android");
			PublicIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "../../ThirdParty/Agora/Include"));
			PublicAdditionalLibraries.Add(System.IO.Path.Combine(AgoraAndroidPath, "arm64-v8a/libagora-rtc-sdk.so"));
			PublicAdditionalLibraries.Add(System.IO.Path.Combine(AgoraAndroidPath, "arm64-v8a/libagora-fdkaac.so"));
			PublicAdditionalLibraries.Add(System.IO.Path.Combine(AgoraAndroidPath, "arm64-v8a/libagora-ffmpeg.so"));
			PublicAdditionalLibraries.Add(System.IO.Path.Combine(AgoraAndroidPath, "arm64-v8a/libagora-soundtouch.so"));
		}

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Agora C++ SDK for Windows
			PublicDefinitions.Add("AGORA_NATIVE_CPP=1");
			string AgoraWinPath = System.IO.Path.Combine(ModuleDirectory, "../../ThirdParty/Agora/Win/x86_64");
			PublicIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "../../ThirdParty/Agora/Include"));
			PublicAdditionalLibraries.Add(System.IO.Path.Combine(AgoraWinPath, "agora_rtc_sdk.dll.lib"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/agora_rtc_sdk.dll",
				System.IO.Path.Combine(AgoraWinPath, "agora_rtc_sdk.dll"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/libaosl.dll",
				System.IO.Path.Combine(AgoraWinPath, "libaosl.dll"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/libagora-fdkaac.dll",
				System.IO.Path.Combine(AgoraWinPath, "libagora-fdkaac.dll"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/libagora-ffmpeg.dll",
				System.IO.Path.Combine(AgoraWinPath, "libagora-ffmpeg.dll"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/libagora-soundtouch.dll",
				System.IO.Path.Combine(AgoraWinPath, "libagora-soundtouch.dll"));
		}

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicFrameworks.Add("AVFoundation");
			PublicFrameworks.Add("CoreAudio");
			PublicFrameworks.Add("AudioToolbox");

			// Agora Voice SDK for macOS
			string AgoraMacPath = System.IO.Path.Combine(ModuleDirectory, "../../ThirdParty/Agora/Mac");
			PublicFrameworks.Add(System.IO.Path.Combine(AgoraMacPath, "AgoraRtcKit.framework"));
			PublicFrameworks.Add(System.IO.Path.Combine(AgoraMacPath, "Agorafdkaac.framework"));
			PublicFrameworks.Add(System.IO.Path.Combine(AgoraMacPath, "Agoraffmpeg.framework"));

			// Add header search path so #import <AgoraRtcKit/...> resolves
			PublicSystemLibraryPaths.Add(AgoraMacPath);
			PublicIncludePaths.Add(AgoraMacPath + "/AgoraRtcKit.framework/Headers");

			// Add rpath so frameworks are found at runtime
			string AbsAgoraMacPath = System.IO.Path.GetFullPath(AgoraMacPath);
			RuntimeDependencies.Add(System.IO.Path.Combine(AbsAgoraMacPath, "AgoraRtcKit.framework/Versions/A/AgoraRtcKit"));
			RuntimeDependencies.Add(System.IO.Path.Combine(AbsAgoraMacPath, "Agorafdkaac.framework/Versions/A/Agorafdkaac"));
			RuntimeDependencies.Add(System.IO.Path.Combine(AbsAgoraMacPath, "Agoraffmpeg.framework/Versions/A/Agoraffmpeg"));
		}

		DynamicallyLoadedModuleNames.AddRange(new string[]
		{
		});
	}
}
