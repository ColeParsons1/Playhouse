// Copyright PlayhouseCity. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class PlayhouseCityServerTarget : TargetRules
{
	public PlayhouseCityServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		ExtraModuleNames.Add("PlayhouseCity");
	}
}
