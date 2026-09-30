// Copyright 2026 kokage. All Rights Reserved.

// UADVSubsystem: save / load, variables and save thumbnails.

#include "ADVSubsystem.h"
#include "ADVSaveGame.h"
#include "CinematicADVConfig.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlayer.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "ImageUtils.h"
#include "UnrealClient.h"
#include "Misc/PackageName.h"

// ---------------------------------------------------------------------------
// Save
// ---------------------------------------------------------------------------

FString UADVSubsystem::GetSaveSlotName(int32 SlotIndex)
{
	ResolveConfig();
	const FString Prefix = Config && !Config->SaveSlotPrefix.IsEmpty() ? Config->SaveSlotPrefix : FString(TEXT("CinematicADV_Slot_"));
	return Prefix + FString::FromInt(SlotIndex);
}

FString UADVSubsystem::GetWorldLevelPath(const UWorld* World)
{
	// The same in PIE and in packaged builds
	return World ? UWorld::RemovePIEPrefix(World->GetPackage()->GetName()) : FString();
}

bool UADVSubsystem::SaveGameToSlot(int32 SlotIndex)
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World || PendingLoad) { return false; }

	UADVSaveGame* Save = Cast<UADVSaveGame>(UGameplayStatics::CreateSaveGameObject(UADVSaveGame::StaticClass()));
	if (!Save) { return false; }

	Save->SaveTime  = FDateTime::Now();
	Save->LevelPath = GetWorldLevelPath(World);

	// The ADV sequence and where it is
	if (UMovieSceneSequencePlayer* Player = ActivePlayer.Get())
	{
		if (UMovieSceneSequence* Sequence = Player->GetSequence())
		{
			Save->SequencePath = FSoftObjectPath(Sequence);
			if (const ALevelSequenceActor* Actor = Player->GetTypedOuter<ALevelSequenceActor>())
			{
				Save->SequenceActorName = Actor->GetName();
			}
			Save->TimeSeconds = Player->GetCurrentTime().AsSeconds();
			Save->bAtWait     = bSectionActive;
			if (bSectionActive)
			{
				Save->WaitStart = ActiveDisplayRate.AsSeconds(ActiveSectionStart);
				Save->WaitEnd   = ActiveDisplayRate.AsSeconds(ActiveSectionEnd);
			}
		}
	}

	Save->Backlog   = BacklogEntries;
	Save->BacklogLinesAtWait = bSectionActive ? FMath::Min(BacklogLinesAtWait, BacklogEntries.Num()) : 0;
	Save->Variables = Variables;
	Save->Thumbnail = PendingThumbnail;
	if (BacklogEntries.Num() > 0)
	{
		Save->SpeakerName = BacklogEntries.Last().SpeakerName;
		Save->Text        = BacklogEntries.Last().Text;
	}

	if (!UGameplayStatics::SaveGameToSlot(Save, GetSaveSlotName(SlotIndex), 0))
	{
		return false;
	}
	OnGameSaved.Broadcast(SlotIndex);
	return true;
}

bool UADVSubsystem::DoesSaveSlotExist(int32 SlotIndex)
{
	return UGameplayStatics::DoesSaveGameExist(GetSaveSlotName(SlotIndex), 0);
}

bool UADVSubsystem::DeleteSaveSlot(int32 SlotIndex)
{
	return UGameplayStatics::DeleteGameInSlot(GetSaveSlotName(SlotIndex), 0);
}

bool UADVSubsystem::GetSaveSlotInfo(int32 SlotIndex, FADVSaveSlotInfo& OutInfo)
{
	OutInfo = FADVSaveSlotInfo();

	const FString SlotName = GetSaveSlotName(SlotIndex);
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0)) { return false; }

	const UADVSaveGame* Save = Cast<UADVSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!Save) { return false; }

	OutInfo.SaveTime    = Save->SaveTime;
	OutInfo.SpeakerName = Save->SpeakerName;
	OutInfo.Text        = Save->Text;
	OutInfo.LevelName   = FPackageName::GetShortName(Save->LevelPath);
	if (Save->Thumbnail.Num() > 0)
	{
		OutInfo.Thumbnail = FImageUtils::ImportBufferAsTexture2D(Save->Thumbnail);
	}
	return true;
}

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------

bool UADVSubsystem::LoadGameFromSlot(int32 SlotIndex)
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const FString SlotName = GetSaveSlotName(SlotIndex);
	if (!World || !UGameplayStatics::DoesSaveGameExist(SlotName, 0)) { return false; }

	UADVSaveGame* Save = Cast<UADVSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!Save || Save->LevelPath.IsEmpty()) { return false; }

	// Game state first: the new level's BeginPlay can already read it
	Variables      = Save->Variables;
	BacklogEntries = Save->Backlog;
	ResetBacklogWaitState();

	bFastForwardToggled = false;
	bResumeAfterBacklog = false;
	CloseBacklog();

	// Always open the level again: nothing of the current world (other sequences, gameplay) is left over
	PendingLoad          = Save;
	PendingLoadSlot      = SlotIndex;
	PendingLoadFromWorld = World;
	UGameplayStatics::OpenLevel(World, FName(*Save->LevelPath));
	return true;
}

void UADVSubsystem::TickPendingLoad()
{
	if (!PendingLoad) { return; }

	// Wait for the new level (travel happens on a later frame) to begin play
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World || World == PendingLoadFromWorld.Get() || !World->HasBegunPlay()) { return; }

	UADVSaveGame* Save = PendingLoad;
	const int32 SlotIndex = PendingLoadSlot;
	PendingLoad = nullptr;
	PendingLoadSlot = -1;
	PendingLoadFromWorld.Reset();

	if (GetWorldLevelPath(World) == Save->LevelPath)
	{
		RestoreFromSave(World, Save);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CinematicADV] Load: level %s was expected, but %s was opened. The sequence is not resumed."),
			*Save->LevelPath, *GetWorldLevelPath(World));
	}

	OnGameLoaded.Broadcast(SlotIndex);
}

void UADVSubsystem::RestoreFromSave(UWorld* World, UADVSaveGame* Save)
{
	// Lines started while restoring (first frame, the jump) are already in the restored backlog
	TGuardValue<bool> RestoringGuard(bRestoringGame, true);

	// Saved outside an ADV sequence: the level alone
	ULevelSequence* Sequence = Cast<ULevelSequence>(Save->SequencePath.TryLoad());
	if (!Sequence) { return; }

	// The actor that played it (same name first), otherwise a new player
	ALevelSequenceActor* Found = nullptr;
	for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
	{
		if (It->GetSequence() != Sequence) { continue; }

		const bool bSameName = It->GetName() == Save->SequenceActorName;
		if (!Found || bSameName)
		{
			Found = *It;
		}
		if (bSameName) { break; }
	}

	ULevelSequencePlayer* Player = Found ? Found->GetSequencePlayer() : nullptr;
	if (!Player)
	{
		ALevelSequenceActor* NewActor = nullptr;
		Player = ULevelSequencePlayer::CreateLevelSequencePlayer(World, Sequence, FMovieSceneSequencePlaybackSettings(), NewActor);
	}
	if (!Player) { return; }

	SetActivePlayer(Player);
	if (!Player->IsPlaying())
	{
		Player->Play();
	}

	const FFrameRate DisplayRate = Player->GetCurrentTime().Rate;

	// Resume at the saved wait
	if (Save->bAtWait)
	{
		TArray<FADVWaitPoint> Waits;
		CollectWaitPoints(Sequence, Waits);

		const double Tolerance = DisplayRate.AsInterval();
		for (const FADVWaitPoint& Wait : Waits)
		{
			if (FMath::Abs(Wait.Start - Save->WaitStart) <= Tolerance && FMath::Abs(Wait.End - Save->WaitEnd) <= Tolerance)
			{
				EnterWait(Wait, DisplayRate);

				// The voices before it were not heard in this session
				LastWaitEndSeconds = Wait.Mode == EClickWaitMode::Stop ? Wait.End : Wait.Start;

				// The lines of this wait are the last ones of the restored backlog (a Loop showing them again adds nothing)
				const int32 NumAtWait = FMath::Clamp(Save->BacklogLinesAtWait, 0, BacklogEntries.Num());
				for (int32 Index = BacklogEntries.Num() - NumAtWait; Index < BacklogEntries.Num(); ++Index)
				{
					RestoredWaitLines.Add(BacklogEntries[Index]);
				}
				BacklogLinesAtWait = NumAtWait;
				return;
			}
		}
	}

	// Not at a wait (or the wait was moved since): play on from the saved time
	bSectionActive = false;
	bPendingPlayTo = false;

	FMovieSceneSequencePlaybackParams Params;
	Params.Frame        = DisplayRate.AsFrameTime(Save->TimeSeconds);
	Params.PositionType = EMovieScenePositionType::Frame;
	Params.UpdateMethod = EUpdatePositionMethod::Jump;
	Player->SetPlaybackPosition(Params);
}

// ---------------------------------------------------------------------------
// Thumbnail
// ---------------------------------------------------------------------------

void UADVSubsystem::CaptureSaveThumbnail()
{
	if (ScreenshotHandle.IsValid()) { return; }

	ScreenshotHandle = UGameViewportClient::OnScreenshotCaptured().AddUObject(this, &UADVSubsystem::HandleScreenshotCaptured);
	FScreenshotRequest::RequestScreenshot(/*bInShowUI*/ false);
}

void UADVSubsystem::HandleScreenshotCaptured(int32 Width, int32 Height, const TArray<FColor>& Colors)
{
	UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotHandle);
	ScreenshotHandle.Reset();

	if (Width <= 0 || Height <= 0 || Colors.Num() != Width * Height) { return; }

	ResolveConfig();
	const int32 ThumbWidth  = FMath::Clamp(Config ? Config->ThumbnailWidth : 320, 32, Width);
	const int32 ThumbHeight = FMath::Max(1, FMath::RoundToInt(static_cast<float>(Height) * ThumbWidth / Width));

	TArray<FColor> Thumb;
	FImageUtils::ImageResize(Width, Height, Colors, ThumbWidth, ThumbHeight, Thumb, /*bLinearSpace*/ false);
	for (FColor& Color : Thumb)
	{
		Color.A = 255;
	}

	TArray64<uint8> Png;
	FImageUtils::PNGCompressImageArray(ThumbWidth, ThumbHeight, Thumb, Png);
	PendingThumbnail = TArray<uint8>(Png.GetData(), static_cast<int32>(Png.Num()));
}

// ---------------------------------------------------------------------------
// Variables
// ---------------------------------------------------------------------------

void UADVSubsystem::SetStringVariable(FName Name, const FString& Value)
{
	Variables.Strings.Add(Name, Value);
}

FString UADVSubsystem::GetStringVariable(FName Name, const FString& DefaultValue) const
{
	const FString* Value = Variables.Strings.Find(Name);
	return Value ? *Value : DefaultValue;
}

void UADVSubsystem::SetNumberVariable(FName Name, double Value)
{
	Variables.Numbers.Add(Name, Value);
}

double UADVSubsystem::GetNumberVariable(FName Name, double DefaultValue) const
{
	const double* Value = Variables.Numbers.Find(Name);
	return Value ? *Value : DefaultValue;
}

void UADVSubsystem::SetFlag(FName Name, bool bValue)
{
	Variables.Flags.Add(Name, bValue);
}

bool UADVSubsystem::GetFlag(FName Name) const
{
	const bool* Value = Variables.Flags.Find(Name);
	return Value && *Value;
}

void UADVSubsystem::ClearVariables()
{
	Variables = FADVVariables();
}
