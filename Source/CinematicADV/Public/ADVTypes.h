// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ADVTypes.generated.h"

class USoundBase;
class UTexture2D;

/** One line in the backlog. */
USTRUCT(BlueprintType)
struct CINEMATICADV_API FADVBacklogEntry
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="CinematicADV|Backlog")
	FText SpeakerName;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="CinematicADV|Backlog")
	FText Text;

	/** Voice of the line (empty if the line has no voice). */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="CinematicADV|Backlog")
	TObjectPtr<USoundBase> Voice;
};

/** Game state kept in save data (Set String Variable / Set Number Variable / Set Flag). */
USTRUCT(BlueprintType)
struct CINEMATICADV_API FADVVariables
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="CinematicADV|Variables")
	TMap<FName, FString> Strings;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="CinematicADV|Variables")
	TMap<FName, double> Numbers;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="CinematicADV|Variables")
	TMap<FName, bool> Flags;
};

/** What a save slot holds, for a save / load screen. */
USTRUCT(BlueprintType)
struct CINEMATICADV_API FADVSaveSlotInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="CinematicADV|Save")
	FDateTime SaveTime;

	/** The last line shown when the game was saved. */
	UPROPERTY(BlueprintReadOnly, Category="CinematicADV|Save")
	FText SpeakerName;

	UPROPERTY(BlueprintReadOnly, Category="CinematicADV|Save")
	FText Text;

	/** Level name (without its path). */
	UPROPERTY(BlueprintReadOnly, Category="CinematicADV|Save")
	FString LevelName;

	/** Screen when the game was saved (Capture Save Thumbnail). Empty if none was captured. */
	UPROPERTY(BlueprintReadOnly, Category="CinematicADV|Save")
	TObjectPtr<UTexture2D> Thumbnail;
};
