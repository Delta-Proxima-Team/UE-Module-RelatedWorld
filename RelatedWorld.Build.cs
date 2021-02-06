// Copyright Delta-Proxima Team (c) 2007-2020

using UnrealBuildTool;
using System;
using System.IO;

public class RelatedWorld : ModuleRules
{
	public RelatedWorld(ReadOnlyTargetRules Target) : base(Target)
	{
		bLegacyPublicIncludePaths = false;
		ShadowVariableWarningLevel = WarningLevel.Error;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDefinitions.Add("SUPPORT_DEPRECATED_CHARACTER_MOVEMENT_RPCS = 1");

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"RenderCore",
			"OnlineSubsystemUtils"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ReplicationGraph"
		});
	}
}
