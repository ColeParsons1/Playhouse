// Copyright PlayhouseCity. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class PlayhouseCityTarget : TargetRules
{
	public PlayhouseCityTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		ExtraModuleNames.Add("PlayhouseCity");
	}
}
