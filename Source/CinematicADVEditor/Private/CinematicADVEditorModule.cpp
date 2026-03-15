// Copyright 2026 kokage. All Rights Reserved.

#include "CinematicADVEditorModule.h"
#include "ClickWaitTrackEditor.h"
#include "ISequencerModule.h"

#define LOCTEXT_NAMESPACE "FCinematicADVEditorModule"

void FCinematicADVEditorModule::StartupModule()
{
	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
	TrackEditorBindingHandle = SequencerModule.RegisterTrackEditor(
		FOnCreateTrackEditor::CreateStatic(&FClickWaitTrackEditor::CreateTrackEditor));
}

void FCinematicADVEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("Sequencer"))
	{
		ISequencerModule& SequencerModule = FModuleManager::GetModuleChecked<ISequencerModule>("Sequencer");
		SequencerModule.UnRegisterTrackEditor(TrackEditorBindingHandle);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCinematicADVEditorModule, CinematicADVEditor)
