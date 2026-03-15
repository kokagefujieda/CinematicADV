// Copyright 2026 kokage. All Rights Reserved.

using UnrealBuildTool;

public class CinematicADV : ModuleRules
{
	public CinematicADV(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"MovieScene",
				"MovieSceneTracks",
				"LevelSequence",
				"UMG",
				"EnhancedInput",
				"SequencerSubtitles",
				"DeveloperSettings",
			"AssetRegistry",
	}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"InputCore",
			}
		);
	}
}
