// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ADVUserSettings.generated.h"

/**
 * Player-facing CinematicADV options, saved per user in GameUserSettings.ini.
 * Change them with the functions of UADVSubsystem (Set Skip Unread, Set Voice Volume).
 */
UCLASS(Config = GameUserSettings)
class CINEMATICADV_API UADVUserSettings : public UObject
{
	GENERATED_BODY()

public:
	/** Fast-forward also skips lines not read yet. */
	UPROPERTY(Config)
	bool bSkipUnread = false;

	/** Volume of the voice Sound Classes (Config: VoiceSoundClasses), 0 - 1. */
	UPROPERTY(Config)
	float VoiceVolume = 1.0f;
};
