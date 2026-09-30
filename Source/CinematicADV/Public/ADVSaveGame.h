// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ADVTypes.h"
#include "ADVSaveGame.generated.h"

/** One save slot (UADVSubsystem::SaveGameToSlot). */
UCLASS()
class CINEMATICADV_API UADVSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	FDateTime SaveTime;

	/** Package path of the level (without the PIE prefix). */
	UPROPERTY(SaveGame)
	FString LevelPath;

	/** The ADV sequence that was playing (empty if none). */
	UPROPERTY(SaveGame)
	FSoftObjectPath SequencePath;

	/** Name of the Level Sequence Actor that played it. */
	UPROPERTY(SaveGame)
	FString SequenceActorName;

	/** Playback position (player's time, seconds). */
	UPROPERTY(SaveGame)
	double TimeSeconds = 0.0;

	/** Saved at a wait: resume at it (range in the player's time, seconds). */
	UPROPERTY(SaveGame)
	bool bAtWait = false;

	UPROPERTY(SaveGame)
	double WaitStart = 0.0;

	UPROPERTY(SaveGame)
	double WaitEnd = 0.0;

	UPROPERTY(SaveGame)
	TArray<FADVBacklogEntry> Backlog;

	UPROPERTY(SaveGame)
	FADVVariables Variables;

	/** The last line shown (for the slot list). */
	UPROPERTY(SaveGame)
	FText SpeakerName;

	UPROPERTY(SaveGame)
	FText Text;

	/** PNG. */
	UPROPERTY(SaveGame)
	TArray<uint8> Thumbnail;
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
