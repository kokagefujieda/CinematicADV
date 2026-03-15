// Copyright 2026 kokage. All Rights Reserved.

using UnrealBuildTool;

public class CinematicADVEditor : ModuleRules
{
	public CinematicADVEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CinematicADV",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"Sequencer",
				"SequencerCore",
				"MovieScene",
				"MovieSceneTools",
				"MovieSceneTracks",
				"UnrealEd",
				"WorkspaceMenuStructure",
				"SequencerSubtitles",
			}
		);
	}
}
