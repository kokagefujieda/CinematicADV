// Copyright 2026 kokage. All Rights Reserved.

#include "CinematicADVConfig.h"
#include "CinematicADVSettings.h"
#include "Sound/SoundClass.h"
#include "UObject/ConstructorHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"

UCinematicADVConfig::UCinematicADVConfig()
{
	// The plugin's voice Sound Class (Content/Audio/SC_Voice)
	static ConstructorHelpers::FObjectFinderOptional<USoundClass> VoiceClass(TEXT("/CinematicADV/Audio/SC_Voice.SC_Voice"));
	if (USoundClass* Class = VoiceClass.Get())
	{
		VoiceSoundClasses.Add(Class);
	}
}

UCinematicADVConfig* UCinematicADVConfig::FindConfig()
{
	// 1. Project Settings (this reference is what gets the asset into packaged builds)
	if (const UCinematicADVSettings* Settings = UCinematicADVSettings::Get())
	{
		if (UCinematicADVConfig* Config = Settings->ConfigAsset.LoadSynchronous())
		{
			return Config;
		}
	}

	// 2. Fallback: search the Asset Registry.
	//    /Game/ paths take priority over plugin Content paths; multiple /Game/ configs → warn and give up.
	FAssetRegistryModule* ARModule = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry");
	if (!ARModule) { return nullptr; }

	TArray<FAssetData> AllAssets;
	ARModule->Get().GetAssetsByClass(UCinematicADVConfig::StaticClass()->GetClassPathName(), AllAssets);

	TArray<FAssetData> UserAssets;
	TArray<FAssetData> PluginAssets;
	for (const FAssetData& Asset : AllAssets)
	{
		if (Asset.PackagePath.ToString().StartsWith(TEXT("/Game/")))
			UserAssets.Add(Asset);
		else
			PluginAssets.Add(Asset);
	}

	if (UserAssets.Num() == 1)
	{
		return Cast<UCinematicADVConfig>(UserAssets[0].GetAsset());
	}
	if (UserAssets.Num() > 1)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CinematicADV] %d UCinematicADVConfig assets found under /Game/. "
			     "Set one in Project Settings → Plugins → CinematicADV → Config Asset."),
			UserAssets.Num());
		return nullptr;
	}
	if (PluginAssets.Num() > 0)
	{
		return Cast<UCinematicADVConfig>(PluginAssets[0].GetAsset());
	}
	return nullptr;
}
