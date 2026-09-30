// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ADVUserSettings.generated.h"

/**
 * Player-facing CinematicADV options, saved per user in GameUserSettings.ini.
 * Change them with the functions of UADVSubsystem (Set Skip Unread, ...).
 */
UCLASS(Config = GameUserSettings)
class CINEMATICADV_API UADVUserSettings : public UObject
{
	GENERATED_BODY()

public:
	/** Fast-forward also skips lines not read yet. */
	UPROPERTY(Config)
	bool bSkipUnread = false;
};

/**
 * System data shared by all save slots (read history).
 * Saved to its own slot (Config: SystemSaveSlotName).
 */
UCLASS()
class CINEMATICADV_API UADVSystemSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Lines already shown (path of their subtitle section). */
	UPROPERTY(SaveGame)
	TSet<FString> ReadLines;
};
