// Copyright Delta-Proxima Team (c) 2007-2020

using UnrealBuildTool;
using System;
using System.IO;

public class RelatedWorldEditor : ModuleRules
{
	public RelatedWorldEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});
	}
}
