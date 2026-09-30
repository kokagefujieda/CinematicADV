// Copyright 2026 kokage. All Rights Reserved.

#include "CinematicADVEditorModule.h"
#include "ClickWaitTrackEditor.h"
#include "ISequencerModule.h"
#include "CinematicADVConfig.h"
#include "ToolMenus.h"
#include "ContentBrowserMenuContexts.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "ScopedTransaction.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "FCinematicADVEditorModule"

namespace CinematicADVEditor
{
	/** Sets the Sound Class of the selected sounds (undoable). */
	static void SetSoundClass(const TArray<FAssetData>& Assets, USoundClass* SoundClass)
	{
		if (!SoundClass) { return; }

		FScopedTransaction Transaction(LOCTEXT("SetVoiceTransaction", "Set as Voice"));

		int32 Count = 0;
		for (const FAssetData& Asset : Assets)
		{
			USoundBase* Sound = Cast<USoundBase>(Asset.GetAsset());
			if (!Sound || Sound->SoundClassObject == SoundClass) { continue; }

			Sound->Modify();
			Sound->SoundClassObject = SoundClass;
			Sound->PostEditChange();
			++Count;
		}

		FNotificationInfo Info(FText::Format(
			LOCTEXT("SetVoiceDone", "{0} sound(s) now use {1}. Save them to keep the change."),
			FText::AsNumber(Count), FText::FromName(SoundClass->GetFName())));
		Info.ExpireDuration = 4.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

void FCinematicADVEditorModule::StartupModule()
{
	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
	TrackEditorBindingHandle = SequencerModule.RegisterTrackEditor(
		FOnCreateTrackEditor::CreateStatic(&FClickWaitTrackEditor::CreateTrackEditor));

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FCinematicADVEditorModule::RegisterMenus));
}

void FCinematicADVEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	if (FModuleManager::Get().IsModuleLoaded("Sequencer"))
	{
		ISequencerModule& SequencerModule = FModuleManager::GetModuleChecked<ISequencerModule>("Sequencer");
		SequencerModule.UnRegisterTrackEditor(TrackEditorBindingHandle);
	}
}

void FCinematicADVEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	// Sound Waves, Sound Cues, MetaSounds, ... (all USoundBase)
	UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(USoundBase::StaticClass());
	if (!Menu) { return; }

	FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
	Section.AddDynamicEntry("CinematicADV_SetVoice", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
	{
		const UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
		if (!Context || Context->SelectedAssets.Num() == 0) { return; }

		const TArray<FAssetData> Assets = Context->SelectedAssets;
		const UCinematicADVConfig* Config = UCinematicADVConfig::FindConfig();

		// One entry per voice Sound Class of the config
		int32 Index = 0;
		if (Config)
		{
			for (USoundClass* VoiceClass : Config->VoiceSoundClasses)
			{
				if (!VoiceClass) { continue; }

				TWeakObjectPtr<USoundClass> WeakClass(VoiceClass);
				InSection.AddMenuEntry(
					FName(*FString::Printf(TEXT("CinematicADV_SetVoice_%d"), Index++)),
					FText::Format(LOCTEXT("SetVoice", "Set as Voice ({0})"), FText::FromName(VoiceClass->GetFName())),
					LOCTEXT("SetVoiceTip", "CinematicADV: set the Sound Class of the selected sounds to this voice Sound Class "
						"(used for auto mode, the voice volume and fast-forward)."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([Assets, WeakClass]()
					{
						CinematicADVEditor::SetSoundClass(Assets, WeakClass.Get());
					})));
			}
		}

		// Nothing to set: say where to set it
		if (Index == 0)
		{
			InSection.AddMenuEntry(
				"CinematicADV_SetVoice_None",
				LOCTEXT("SetVoiceNone", "Set as Voice (CinematicADV)"),
				LOCTEXT("SetVoiceNoneTip", "No voice Sound Class. Set Voice Sound Classes in the CinematicADV Config asset "
					"(Project Settings → Plugins → CinematicADV → Config Asset)."),
				FSlateIcon(),
				FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([]() { return false; })));
		}
	}));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCinematicADVEditorModule, CinematicADVEditor)
