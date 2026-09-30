// Copyright 2026 kokage. All Rights Reserved.

#include "ADVSubsystem.h"
#include "ADVUserSettings.h"
#include "ADVSaveGame.h"
#include "CinematicADVConfig.h"
#include "SSkipGaugeWidget.h"
#include "SADVBacklogWidget.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SOverlay.h"
#include "EngineUtils.h"
#include "LevelSequenceActor.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "Sections/MovieSceneSubSection.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "SubtitleSubsystem.h"
#include "SubtitleSection.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Styling/CoreStyle.h"
#include "Engine/Font.h"
#include "AudioDevice.h"
#include "Sound/SoundMix.h"

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UADVSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UADVSubsystem::HandleWorldCleanup);

	// Listen to the subtitles of a new world before its actors begin play (a sequence may start on the first frame)
	WorldActorsInitializedHandle = FWorldDelegates::OnWorldInitializedActors.AddWeakLambda(this,
		[this](const UWorld::FActorsInitializedParams& Params)
		{
			if (Params.World && Params.World->GetGameInstance() == GetGameInstance())
			{
				BindSubtitleEvents(Params.World);
				ApplyVoiceVolume(Params.World);
			}
		});
}

void UADVSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldActorsInitializedHandle);
	HideSkipGauge();
	bResumeAfterBacklog = false;
	CloseBacklog();
	ClearActivePlayer();
	bFastForwardToggled = false;
	bFastForwardHeld    = false;
	UpdateFastForward();
	BindSubtitleEvents(nullptr);
	RemoveVoiceVolume(VoiceVolumeWorld.Get());
	SaveSystemData(/*bAsync*/ false);
	Super::Deinitialize();
}

void UADVSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	// The backlog of the game world closes with it
	UWorld* GameWorld = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	UMovieSceneSequencePlayer* Player = ActivePlayer.Get();
	if (!GameWorld || GameWorld == World || (Player && Player->GetWorld() == World))
	{
		bResumeAfterBacklog = false;
		CloseBacklog();
	}

	// The player may be destroyed without OnStop (level travel)
	if (Player && Player->GetWorld() == World)
	{
		bFastForwardToggled = false;
		SaveSystemData(/*bAsync*/ true);
	}
	if (!Player || Player->GetWorld() == World)
	{
		ClearActivePlayer();
	}

	USubtitleSubsystem* Subtitles = BoundSubtitles.Get();
	if (!Subtitles || Subtitles->GetWorld() == World)
	{
		BindSubtitleEvents(nullptr);
	}

	RemoveVoiceVolume(World);
}

// ---------------------------------------------------------------------------
// Player registration
// ---------------------------------------------------------------------------

void UADVSubsystem::RegisterSequencePlayer(ULevelSequencePlayer* Player)
{
	SetActivePlayer(Player);
}

void UADVSubsystem::SetActivePlayer(UMovieSceneSequencePlayer* Player)
{
	if (Player == ActivePlayer.Get()) { return; }

	ClearActivePlayer();
	if (!Player) { return; }

	ActivePlayer = Player;
	Player->OnStop.AddUniqueDynamic(this, &UADVSubsystem::OnPlayerStopped);
	Player->OnFinished.AddUniqueDynamic(this, &UADVSubsystem::OnPlayerStopped);

	// Input is active only while an ADV sequence plays
	EnsureInputBound();
	AddInputContext();
}

void UADVSubsystem::ClearActivePlayer()
{
	if (UMovieSceneSequencePlayer* OldPlayer = ActivePlayer.Get())
	{
		OldPlayer->OnStop.RemoveDynamic(this, &UADVSubsystem::OnPlayerStopped);
		OldPlayer->OnFinished.RemoveDynamic(this, &UADVSubsystem::OnPlayerStopped);
	}
	ActivePlayer.Reset();

	// Cancel any in-progress skip hold
	OnSkipReleased();

	bSectionActive    = false;
	bAdvanceRequested = false;
	bPendingPlayTo    = false;
	ActiveSectionKey  = 0;
	LastWaitEndSeconds = TNumericLimits<double>::Lowest();
	ResetAutoTimer();
	BacklogSlotsAtWait.Reset();
	bResumeAfterBacklog = false;

	// Back to the normal play rate and sound (the old player)
	UpdateFastForward();

	RemoveInputContext();
}

// ---------------------------------------------------------------------------
// Config / input
// ---------------------------------------------------------------------------

UCinematicADVConfig* UADVSubsystem::ResolveConfig()
{
	if (Config) { return Config; }

	Config = UCinematicADVConfig::FindConfig();

	if (Config)
	{
		ConfigClickOutsideWait = Config->ClickOutsideWait;
		ConfigFadeDuration     = Config->FadeOutDuration;
		bConfigFadeInAfterSkip = Config->bFadeInAfterSkip;
		ConfigFadeInDuration   = Config->FadeInDuration;
		ConfigHoldDuration     = FMath::Max(Config->HoldDuration, 0.1f);
		ConfigGaugeSize        = Config->GaugeSize;
		ConfigGaugeColor       = Config->GaugeColor;
		ConfigGaugeBgColor     = Config->GaugeBackgroundColor;
		ConfigAutoBaseDelay       = FMath::Max(Config->AutoBaseDelay, 0.0f);
		ConfigAutoDelayPerChar    = FMath::Max(Config->AutoDelayPerChar, 0.0f);
		ConfigAutoDelayAfterVoice = FMath::Max(Config->AutoDelayAfterVoice, 0.0f);
	}
	return Config;
}

APlayerController* UADVSubsystem::GetLocalController() const
{
	return GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
}

void UADVSubsystem::EnsureInputBound()
{
	// Bind once per controller: a new controller (e.g. after level travel) needs its own binding
	APlayerController* PC = GetLocalController();
	if (!PC || PC == BoundController.Get()) { return; }

	UCinematicADVConfig* Cfg = ResolveConfig();
	if (!Cfg) { return; }

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent);
	if (!EIC) { return; }

	if (Cfg->AdvanceAction)
	{
		EIC->BindAction(Cfg->AdvanceAction, ETriggerEvent::Started, this, &UADVSubsystem::Advance);
	}
	if (Cfg->SkipAction)
	{
		EIC->BindAction(Cfg->SkipAction, ETriggerEvent::Started,   this, &UADVSubsystem::OnSkipPressed);
		EIC->BindAction(Cfg->SkipAction, ETriggerEvent::Completed, this, &UADVSubsystem::OnSkipReleased);
		EIC->BindAction(Cfg->SkipAction, ETriggerEvent::Canceled,  this, &UADVSubsystem::OnSkipReleased);
	}
	if (Cfg->AutoAction)
	{
		EIC->BindAction(Cfg->AutoAction, ETriggerEvent::Started, this, &UADVSubsystem::ToggleAutoMode);
	}
	if (Cfg->BacklogAction)
	{
		EIC->BindAction(Cfg->BacklogAction, ETriggerEvent::Started, this, &UADVSubsystem::ToggleBacklog);
	}
	if (Cfg->FastForwardAction)
	{
		EIC->BindAction(Cfg->FastForwardAction, ETriggerEvent::Started,   this, &UADVSubsystem::OnFastForwardPressed);
		EIC->BindAction(Cfg->FastForwardAction, ETriggerEvent::Completed, this, &UADVSubsystem::OnFastForwardReleased);
		EIC->BindAction(Cfg->FastForwardAction, ETriggerEvent::Canceled,  this, &UADVSubsystem::OnFastForwardReleased);
	}
	if (Cfg->FastForwardToggleAction)
	{
		EIC->BindAction(Cfg->FastForwardToggleAction, ETriggerEvent::Started, this, &UADVSubsystem::ToggleFastForwardMode);
	}
	BoundController = PC;
}

void UADVSubsystem::AddInputContext()
{
	if (bContextAdded) { return; }

	UCinematicADVConfig* Cfg = ResolveConfig();
	APlayerController* PC = GetLocalController();
	ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	if (!Cfg || !Cfg->InputMappingContext || !LP) { return; }

	if (UEnhancedInputLocalPlayerSubsystem* InputSub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
	{
		InputSub->AddMappingContext(Cfg->InputMappingContext, 90);
		ContextLocalPlayer = LP;
		bContextAdded = true;
	}
}

void UADVSubsystem::RemoveInputContext()
{
	if (!bContextAdded) { return; }
	bContextAdded = false;

	// Outside ADV sequences the keys go back to the game
	ULocalPlayer* LP = ContextLocalPlayer.Get();
	if (!LP || !Config || !Config->InputMappingContext) { return; }

	if (UEnhancedInputLocalPlayerSubsystem* InputSub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
	{
		InputSub->RemoveMappingContext(Config->InputMappingContext);
	}
	ContextLocalPlayer.Reset();
}

// ---------------------------------------------------------------------------
// Section evaluated (called by FClickWaitEvalTemplate every frame inside a section)
// ---------------------------------------------------------------------------

void UADVSubsystem::OnSectionEvaluated(UMovieSceneSequencePlayer* Player, uint32 SectionKey, EClickWaitMode Mode,
	double LocalNow, double LocalStart, double LocalEnd)
{
	if (!Player) { return; }

	// The player evaluating the section is the one to control
	if (Player != ActivePlayer.Get())
	{
		SetActivePlayer(Player);
	}
	EnsureInputBound();
	AddInputContext();

	if (bSectionActive && ActiveSectionKey == SectionKey) { return; }

	// Convert the section range to the player's time: offset from the time evaluated right now
	const FQualifiedFrameTime Now = Player->GetCurrentTime();
	const double PlayerNow = Now.AsSeconds();

	ActiveDisplayRate  = Now.Rate;
	ActiveSectionStart = ActiveDisplayRate.AsFrameTime(PlayerNow + (LocalStart - LocalNow));
	ActiveSectionEnd   = ActiveDisplayRate.AsFrameTime(PlayerNow + (LocalEnd - LocalNow));
	ActiveSectionKey   = SectionKey;
	ActiveMode         = Mode;
	bSectionActive     = true;
	bAdvanceRequested  = false;
	ResetAutoTimer();

	// Stop exactly at the section end (set after this evaluation, in Tick)
	bPendingPlayTo = true;
}

// ---------------------------------------------------------------------------
// Input — Advance
// ---------------------------------------------------------------------------

void UADVSubsystem::Advance()
{
	// A click while the backlog is open closes it
	if (bBacklogOpen)
	{
		CloseBacklog();
		return;
	}

	// Handled in Tick, after this frame's evaluation
	if (ActivePlayer.IsValid())
	{
		bAdvanceRequested = true;
	}
}

USubtitleSubsystem* UADVSubsystem::GetSubtitleSubsystem() const
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	return World ? World->GetSubsystem<USubtitleSubsystem>() : nullptr;
}

bool UADVSubsystem::IsTextRevealing() const
{
	const USubtitleSubsystem* Subtitles = GetSubtitleSubsystem();
	return Subtitles && Subtitles->IsTypewriterRevealing();
}

void UADVSubsystem::HandleAdvance()
{
	UMovieSceneSequencePlayer* Player = ActivePlayer.Get();
	if (!Player) { return; }

	if (bSectionActive)
	{
		// First click in a Stop section while the text is still being typed: show it all (wait position)
		if (ActiveMode == EClickWaitMode::Stop && !IsAtSectionEnd(Player) && IsTextRevealing())
		{
			JumpToWaitPosition();
			return;
		}

		// Otherwise continue after the section
		AdvancePastWait();
		return;
	}

	// Outside a wait section
	ResolveConfig();
	if (ConfigClickOutsideWait == EADVClickOutsideWait::JumpToNextWait)
	{
		JumpToNextWait();
	}
}

void UADVSubsystem::JumpToWaitPosition()
{
	UMovieSceneSequencePlayer* Player = ActivePlayer.Get();
	if (!Player) { return; }

	// Just inside the section end (where an exclusive PlayTo would stop), then wait there
	FMovieSceneSequencePlaybackParams Params;
	Params.Frame        = ActiveSectionEnd - FFrameTime(FFrameNumber(0), 0.5f);
	Params.PositionType = EMovieScenePositionType::Frame;
	Params.UpdateMethod = EUpdatePositionMethod::Jump;
	Player->SetPlaybackPosition(Params);
	Player->Pause();
	bPendingPlayTo = false;
}

void UADVSubsystem::JumpToNextWait()
{
	UMovieSceneSequencePlayer* Player = ActivePlayer.Get();
	if (!Player) { return; }

	TArray<FADVWaitPoint> Waits;
	CollectWaitPoints(Player->GetSequence(), Waits);

	const FQualifiedFrameTime Now = Player->GetCurrentTime();
	const double NowSeconds = Now.AsSeconds();

	const FADVWaitPoint* Next = nullptr;
	for (const FADVWaitPoint& Wait : Waits)
	{
		if (Wait.End > NowSeconds + KINDA_SMALL_NUMBER && (!Next || Wait.Start < Next->Start))
		{
			Next = &Wait;
		}
	}
	if (!Next) { return; }

	// Wait on it as if it had been reached by playing
	EnterWait(*Next, Now.Rate);

	// The voices jumped over were not heard (auto mode then waits as for a line without a voice)
	LastWaitEndSeconds = Next->Mode == EClickWaitMode::Stop ? Next->End : Next->Start;
}

void UADVSubsystem::EnterWait(const FADVWaitPoint& Wait, FFrameRate DisplayRate)
{
	ActiveDisplayRate  = DisplayRate;
	ActiveSectionStart = ActiveDisplayRate.AsFrameTime(Wait.Start);
	ActiveSectionEnd   = ActiveDisplayRate.AsFrameTime(Wait.End);
	ActiveSectionKey   = Wait.Key;
	ActiveMode         = Wait.Mode;
	bSectionActive     = true;
	bAdvanceRequested  = false;
	ResetAutoTimer();
	BacklogSlotsAtWait.Reset();

	if (ActiveMode == EClickWaitMode::Stop)
	{
		JumpToWaitPosition();
	}
	else
	{
		LoopToStart();
	}
}

void UADVSubsystem::AdvancePastWait()
{
	LastWaitEndSeconds = ActiveDisplayRate.AsSeconds(ActiveSectionEnd);
	bSectionActive     = false;
	bPendingPlayTo     = false;
	ResetAutoTimer();
	BacklogSlotsAtWait.Reset();
	JumpPastSection();
}

void UADVSubsystem::ForEachSectionInRootTime(UMovieSceneSequence* Sequence,
	TFunctionRef<void(const UMovieSceneSection* Section, double Start, double End)> Visit)
{
	const UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!MovieScene) { return; }

	// Master tracks and object-binding tracks (muted tracks are left out)
	auto ForEachTrack = [](const UMovieScene* Scene, TFunctionRef<void(const UMovieSceneTrack*)> Fn)
	{
		for (const UMovieSceneTrack* Track : Scene->GetTracks())
		{
			if (Track && !Track->IsEvalDisabled()) { Fn(Track); }
		}
		for (const FMovieSceneBinding& Binding : Scene->GetBindings())
		{
			for (const UMovieSceneTrack* Track : Binding.GetTracks())
			{
				if (Track && !Track->IsEvalDisabled()) { Fn(Track); }
			}
		}
	};

	auto IsUsable = [](const UMovieSceneSection* Section)
	{
		return Section && Section->IsActive() && Section->HasStartFrame() && Section->HasEndFrame();
	};

	const FFrameRate TickResolution = MovieScene->GetTickResolution();

	ForEachTrack(MovieScene, [&](const UMovieSceneTrack* Track)
	{
		for (const UMovieSceneSection* Section : Track->GetAllSections())
		{
			if (!IsUsable(Section)) { continue; }

			const double OuterStart = TickResolution.AsSeconds(FFrameTime(Section->GetInclusiveStartFrame()));
			const double OuterEnd   = TickResolution.AsSeconds(FFrameTime(Section->GetExclusiveEndFrame()));
			Visit(Section, OuterStart, OuterEnd);

			// Sections inside sub-sequences / shots (one level; play rate 1 assumed)
			const UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section);
			UMovieSceneSequence* SubSequence = SubSection ? SubSection->GetSequence() : nullptr;
			const UMovieScene* SubMovieScene = SubSequence ? SubSequence->GetMovieScene() : nullptr;
			if (!SubMovieScene || !SubMovieScene->GetPlaybackRange().HasLowerBound()) { continue; }

			const FFrameRate SubTickResolution = SubMovieScene->GetTickResolution();
			const double InnerStart = SubTickResolution.AsSeconds(FFrameTime(
				SubMovieScene->GetPlaybackRange().GetLowerBoundValue() + SubSection->Parameters.StartFrameOffset));

			ForEachTrack(SubMovieScene, [&](const UMovieSceneTrack* SubTrack)
			{
				for (const UMovieSceneSection* Inner : SubTrack->GetAllSections())
				{
					if (!IsUsable(Inner)) { continue; }

					const double Start = OuterStart + (SubTickResolution.AsSeconds(FFrameTime(Inner->GetInclusiveStartFrame())) - InnerStart);
					const double End   = OuterStart + (SubTickResolution.AsSeconds(FFrameTime(Inner->GetExclusiveEndFrame())) - InnerStart);
					const double ClippedStart = FMath::Max(Start, OuterStart);
					const double ClippedEnd   = FMath::Min(End, OuterEnd);
					if (ClippedEnd > ClippedStart)
					{
						Visit(Inner, ClippedStart, ClippedEnd);
					}
				}
			});
		}
	});
}

void UADVSubsystem::CollectWaitPoints(UMovieSceneSequence* Sequence, TArray<FADVWaitPoint>& OutWaits)
{
	ForEachSectionInRootTime(Sequence, [&OutWaits](const UMovieSceneSection* Section, double Start, double End)
	{
		const UClickWaitSection* WaitSection = Cast<UClickWaitSection>(Section);
		if (!WaitSection) { return; }

		FADVWaitPoint Wait;
		Wait.Key   = WaitSection->GetUniqueID();
		Wait.Mode  = WaitSection->Mode;
		Wait.Start = Start;
		Wait.End   = End;
		OutWaits.Add(Wait);
	});
}

// ---------------------------------------------------------------------------
// Auto mode
// ---------------------------------------------------------------------------

void UADVSubsystem::SetAutoMode(bool bEnabled)
{
	if (bAutoMode == bEnabled) { return; }

	bAutoMode = bEnabled;
	ResetAutoTimer();
	OnAutoModeChanged.Broadcast(bAutoMode);
}

void UADVSubsystem::ToggleAutoMode()
{
	SetAutoMode(!bAutoMode);
}

void UADVSubsystem::ResetAutoTimer()
{
	AutoElapsed = 0.0f;
	AutoDelay   = -1.0f;
}

bool UADVSubsystem::IsWaitReached(UMovieSceneSequencePlayer* Player) const
{
	// Stop: the wait is reached once the player has paused at the section end. Loop: as soon as it is entered.
	return ActiveMode == EClickWaitMode::Loop || (Player->IsPaused() && IsAtSectionEnd(Player));
}

void UADVSubsystem::TickAuto(UMovieSceneSequencePlayer* Player, float DeltaTime)
{
	if (!IsWaitReached(Player) || IsTextRevealing()) { return; }

	if (AutoDelay < 0.0f)
	{
		ResolveConfig();
		AutoDelay = ComputeAutoDelay(Player);
	}

	AutoElapsed += DeltaTime;
	if (AutoElapsed >= AutoDelay)
	{
		AdvancePastWait();
	}
}

float UADVSubsystem::ComputeAutoDelay(UMovieSceneSequencePlayer* Player) const
{
	const double WaitStart = ActiveDisplayRate.AsSeconds(ActiveSectionStart);
	const double WaitEnd   = ActiveDisplayRate.AsSeconds(ActiveSectionEnd);

	// Where the wait is reached: Stop pauses at the section end, Loop waits from its start
	const double Reached = ActiveMode == EClickWaitMode::Stop ? WaitEnd : WaitStart;

	// Lines on screen while waiting: Stop → at the pause (last frame of the section), Loop → during the section
	const double LineFrom = ActiveMode == EClickWaitMode::Stop ? WaitEnd - ActiveDisplayRate.AsInterval() : WaitStart;

	bool   bHasVoice = false;
	double VoiceEnd  = TNumericLimits<double>::Lowest();
	bool   bHasLine  = false;
	int32  Chars     = 0;

	auto CountChars = [](const FText& Text)
	{
		int32 Count = 0;
		for (const TCHAR Char : Text.ToString())
		{
			if (!FChar::IsWhitespace(Char)) { ++Count; }
		}
		return Count;
	};

	ForEachSectionInRootTime(Player->GetSequence(), [&](const UMovieSceneSection* Section, double Start, double End)
	{
		if (const UMovieSceneSeqSubtitleSection* Line = Cast<UMovieSceneSeqSubtitleSection>(Section))
		{
			if (Start < WaitEnd && End > LineFrom)
			{
				Chars += CountChars(Line->SubtitleText);
				bHasLine = true;
			}
			return;
		}

		// A voice of this line: started after playback left the previous wait, before this wait ends
		if (Start >= LastWaitEndSeconds - KINDA_SMALL_NUMBER && Start < WaitEnd && IsVoiceSection(Section))
		{
			// The section may be longer than the sound
			USoundBase* Sound = CastChecked<UMovieSceneAudioSection>(Section)->GetSound();
			const float Duration = Sound->GetDuration();
			const double SoundEnd = Duration > 0.0f && !Sound->IsLooping() ? Start + Duration : End;

			bHasVoice = true;
			VoiceEnd  = FMath::Max(VoiceEnd, FMath::Min(SoundEnd, End));
		}
	});

	if (bHasVoice)
	{
		// Wait for the voice to finish, then a short pause
		return static_cast<float>(FMath::Max(VoiceEnd - Reached, 0.0)) + ConfigAutoDelayAfterVoice;
	}

	// No line in the sequence (e.g. ShowMessage): the text shown right now
	if (!bHasLine)
	{
		const USubtitleSubsystem* Subtitles = GetSubtitleSubsystem();
		if (Subtitles && Subtitles->bIsSubtitleActive)
		{
			Chars = CountChars(Subtitles->CurrentSubtitleText);
		}
	}

	// Reading time
	return ConfigAutoBaseDelay + ConfigAutoDelayPerChar * Chars;
}

bool UADVSubsystem::IsVoiceSection(const UMovieSceneSection* Section) const
{
	const UMovieSceneAudioSection* Audio = Cast<UMovieSceneAudioSection>(Section);
	USoundBase* Sound = Audio ? Audio->GetSound() : nullptr;
	if (!Sound || !Config) { return false; }

	// Sound Class (or one of its parents) in the list
	if (Config->VoiceSoundClasses.Num() > 0)
	{
		int32 Depth = 0;
		for (USoundClass* Class = Sound->GetSoundClass(); Class && Depth < 16; Class = Class->ParentClass, ++Depth)
		{
			if (Config->VoiceSoundClasses.Contains(Class)) { return true; }
		}
	}

	// Asset path contains a keyword
	const FString Path = Sound->GetPathName();
	for (const FString& Keyword : Config->VoiceAssetKeywords)
	{
		if (!Keyword.IsEmpty() && Path.Contains(Keyword, ESearchCase::IgnoreCase)) { return true; }
	}
	return false;
}

void UADVSubsystem::PollForAdvPlayer(float DeltaTime)
{
	PollElapsed += DeltaTime;
	if (PollElapsed < 0.25f || ActivePlayer.IsValid()) { return; }
	PollElapsed = 0.0f;

	TryFindAdvPlayer();
}

void UADVSubsystem::TryFindAdvPlayer()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World) { return; }

	for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
	{
		ULevelSequencePlayer* Player = It->GetSequencePlayer();
		if (!Player || !Player->IsPlaying()) { continue; }

		UMovieSceneSequence* Sequence = Player->GetSequence();
		if (!Sequence) { continue; }

		bool* bCached = AdvSequenceCache.Find(Sequence);
		if (!bCached)
		{
			TArray<FADVWaitPoint> Waits;
			CollectWaitPoints(Sequence, Waits);
			bCached = &AdvSequenceCache.Add(Sequence, Waits.Num() > 0);
		}

		// A sequence with Click Wait sections: enable input from its first frame
		if (*bCached)
		{
			SetActivePlayer(Player);
			return;
		}
	}
}

// ---------------------------------------------------------------------------
// Backlog
// ---------------------------------------------------------------------------

void UADVSubsystem::BindSubtitleEvents(UWorld* World)
{
	USubtitleSubsystem* Subtitles = World ? World->GetSubsystem<USubtitleSubsystem>() : nullptr;
	if (Subtitles == BoundSubtitles.Get()) { return; }

	if (USubtitleSubsystem* Old = BoundSubtitles.Get())
	{
		Old->OnSubtitleSlotStarted.RemoveDynamic(this, &UADVSubsystem::HandleSubtitleSlotStarted);
	}
	BoundSubtitles = Subtitles;
	if (Subtitles)
	{
		Subtitles->OnSubtitleSlotStarted.AddUniqueDynamic(this, &UADVSubsystem::HandleSubtitleSlotStarted);
	}
}

void UADVSubsystem::HandleSubtitleSlotStarted(int32 SlotID, const FText& SubtitleText, const FText& SpeakerName,
	const FSubtitleAppearance& Appearance)
{
	if (SubtitleText.IsEmptyOrWhitespace()) { return; }

	// Only lines of ADV sequences (the first line may come before the poll has found the player)
	if (!ActivePlayer.IsValid())
	{
		TryFindAdvPlayer();
	}
	UMovieSceneSequencePlayer* Player = ActivePlayer.Get();
	if (!Player) { return; }

	ResolveConfig();

	FString     LineKey;
	USoundBase* Voice = nullptr;
	if (SlotID != 0)
	{
		FindLineInfo(Player, static_cast<uint32>(SlotID), LineKey, Voice);
	}

	// Read history: a line shown for the first time stops fast-forward (unless unread lines are skipped too)
	if (!LineKey.IsEmpty() && MarkLineRead(LineKey) && !GetSkipUnread())
	{
		// Applied in Tick, before the next frame is evaluated
		bFastForwardToggled = false;
		bFastForwardBlocked = bFastForwardHeld;
	}

	if (SlotID != 0)
	{
		// A subtitle section restarted at the same wait (Loop) is the same line
		bool bAlreadyRecorded = false;
		BacklogSlotsAtWait.Add(SlotID, &bAlreadyRecorded);
		if (bAlreadyRecorded) { return; }
	}
	else if (BacklogEntries.Num() > 0
		&& BacklogEntries.Last().Text.EqualTo(SubtitleText)
		&& BacklogEntries.Last().SpeakerName.EqualTo(SpeakerName))
	{
		// ShowMessage shown again
		return;
	}

	FADVBacklogEntry Entry;
	Entry.SpeakerName = SpeakerName;
	Entry.Text        = SubtitleText;
	Entry.Voice       = Voice;
	AddBacklogEntryInternal(Entry);
}

void UADVSubsystem::FindLineInfo(UMovieSceneSequencePlayer* Player, uint32 SlotID, FString& OutLineKey, USoundBase*& OutVoice) const
{
	OutLineKey.Reset();
	OutVoice = nullptr;

	struct FTimedSection
	{
		double                    Start;
		double                    End;
		const UMovieSceneSection* Section;
	};

	// The line's section (a sub-sequence used twice appears twice) and the voices
	TArray<FTimedSection> LineRanges;
	TArray<FTimedSection> Voices;
	ForEachSectionInRootTime(Player->GetSequence(), [&](const UMovieSceneSection* Section, double Start, double End)
	{
		if (Section->GetUniqueID() == SlotID && Section->IsA<UMovieSceneSeqSubtitleSection>())
		{
			LineRanges.Add({ Start, End, Section });
		}
		else if (IsVoiceSection(Section))
		{
			Voices.Add({ Start, End, Section });
		}
	});
	if (LineRanges.Num() == 0) { return; }

	// The instance playing now
	const double Now = Player->GetCurrentTime().AsSeconds();
	const FTimedSection* Line = &LineRanges[0];
	for (const FTimedSection& Range : LineRanges)
	{
		if (Range.Start <= Now + KINDA_SMALL_NUMBER && Now < Range.End)
		{
			Line = &Range;
			break;
		}
	}

	// Same in packaged builds (package path + object names)
	OutLineKey = Line->Section->GetPathName();

	// The voice starting closest to the line (from just before it to its end)
	double BestDistance = TNumericLimits<double>::Max();
	for (const FTimedSection& Voice : Voices)
	{
		if (Voice.Start < Line->Start - 0.25 || Voice.Start >= Line->End) { continue; }

		const double Distance = FMath::Abs(Voice.Start - Line->Start);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			OutVoice = CastChecked<UMovieSceneAudioSection>(Voice.Section)->GetSound();
		}
	}
}

void UADVSubsystem::AddBacklogEntry(const FText& SpeakerName, const FText& Text, USoundBase* Voice)
{
	FADVBacklogEntry Entry;
	Entry.SpeakerName = SpeakerName;
	Entry.Text        = Text;
	Entry.Voice       = Voice;
	AddBacklogEntryInternal(Entry);
}

void UADVSubsystem::AddBacklogEntryInternal(const FADVBacklogEntry& Entry)
{
	ResolveConfig();
	const int32 MaxEntries = FMath::Max(Config ? Config->MaxBacklogEntries : 200, 1);

	BacklogEntries.Add(Entry);
	if (BacklogEntries.Num() > MaxEntries)
	{
		BacklogEntries.RemoveAt(0, BacklogEntries.Num() - MaxEntries);
	}
	OnBacklogEntryAdded.Broadcast(Entry);
}

void UADVSubsystem::ClearBacklog()
{
	StopBacklogVoice();
	BacklogEntries.Reset();
	BacklogSlotsAtWait.Reset();
}

void UADVSubsystem::OpenBacklog()
{
	if (bBacklogOpen) { return; }

	ResolveConfig();
	bBacklogOpen      = true;
	bAdvanceRequested = false;
	OnSkipReleased();

	// The sequence waits while the player reads
	UMovieSceneSequencePlayer* Player = ActivePlayer.Get();
	bResumeAfterBacklog = Player && Player->IsPlaying();
	if (bResumeAfterBacklog)
	{
		Player->Pause();
	}
	UpdateFastForward();

	if (Config && Config->bUseBuiltInBacklogUI)
	{
		ShowBacklogUI();
	}
	OnBacklogOpened.Broadcast();
}

void UADVSubsystem::CloseBacklog()
{
	if (!bBacklogOpen) { return; }

	bBacklogOpen = false;
	StopBacklogVoice();
	HideBacklogUI();
	UpdateFastForward();

	if (bResumeAfterBacklog)
	{
		bResumeAfterBacklog = false;
		if (UMovieSceneSequencePlayer* Player = ActivePlayer.Get())
		{
			if (!bSectionActive)
			{
				Player->Play();
			}
			else if (!IsAtSectionEnd(Player))
			{
				// Pausing cleared the stop at the section end: set it again
				PlayToSectionEnd();
			}
			// At the section end: Tick waits (Stop) or loops (Loop) as before
		}
	}
	OnBacklogClosed.Broadcast();
}

void UADVSubsystem::ToggleBacklog()
{
	if (bBacklogOpen)
	{
		CloseBacklog();
	}
	else
	{
		OpenBacklog();
	}
}

void UADVSubsystem::PlayBacklogVoice(int32 Index)
{
	StopBacklogVoice();
	if (!BacklogEntries.IsValidIndex(Index) || !BacklogEntries[Index].Voice) { return; }

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World) { return; }

	BacklogVoiceComponent = UGameplayStatics::SpawnSound2D(World, BacklogEntries[Index].Voice);
}

void UADVSubsystem::StopBacklogVoice()
{
	if (UAudioComponent* Component = BacklogVoiceComponent.Get())
	{
		Component->Stop();
	}
	BacklogVoiceComponent.Reset();
}

void UADVSubsystem::ShowBacklogUI()
{
	UGameViewportClient* ViewportClient = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr;
	if (!ViewportClient || !Config || BacklogWidget.IsValid()) { return; }

	const int32 FontSize = FMath::Max(Config->BacklogFontSize, 1);
	const FSlateFontInfo Font = Config->BacklogFont
		? FSlateFontInfo(Config->BacklogFont, FontSize)
		: FCoreStyle::GetDefaultFontStyle("Regular", FontSize);

	TWeakObjectPtr<UADVSubsystem> WeakThis(this);
	BacklogWidget = SNew(SADVBacklogWidget)
		.Entries(BacklogEntries)
		.Font(Font)
		.BackgroundColor(Config->BacklogBackgroundColor)
		.TextColor(Config->BacklogTextColor)
		.SpeakerColor(Config->BacklogSpeakerColor)
		.OnPlayVoice_Lambda([WeakThis](int32 Index)
		{
			if (UADVSubsystem* Self = WeakThis.Get())
			{
				Self->PlayBacklogVoice(Index);
			}
		});

	// Below the skip gauge (100)
	ViewportClient->AddViewportWidgetContent(BacklogWidget.ToSharedRef(), 90);

	// Mouse cursor to scroll and to press the voice buttons
	APlayerController* PC = GetLocalController();
	if (Config->bBacklogShowMouseCursor && PC)
	{
		bBacklogChangedCursor = true;
		bSavedShowMouseCursor = PC->bShowMouseCursor;

		PC->SetShowMouseCursor(true);
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
	}
}

void UADVSubsystem::HideBacklogUI()
{
	if (BacklogWidget.IsValid())
	{
		if (UGameViewportClient* ViewportClient = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr)
		{
			ViewportClient->RemoveViewportWidgetContent(BacklogWidget.ToSharedRef());
		}
		BacklogWidget.Reset();
	}

	if (bBacklogChangedCursor)
	{
		bBacklogChangedCursor = false;
		if (APlayerController* PC = GetLocalController())
		{
			PC->SetShowMouseCursor(bSavedShowMouseCursor);

			// A game that hid the cursor is taken to use Game Only input
			if (!bSavedShowMouseCursor)
			{
				PC->SetInputMode(FInputModeGameOnly());
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Fast forward
// ---------------------------------------------------------------------------

void UADVSubsystem::SetFastForwardMode(bool bEnabled)
{
	bFastForwardToggled = bEnabled;
	UpdateFastForward();
}

void UADVSubsystem::ToggleFastForwardMode()
{
	SetFastForwardMode(!bFastForwardToggled);
}

void UADVSubsystem::OnFastForwardPressed()
{
	bFastForwardHeld    = true;
	bFastForwardBlocked = false;
	UpdateFastForward();
}

void UADVSubsystem::OnFastForwardReleased()
{
	bFastForwardHeld    = false;
	bFastForwardBlocked = false;
	UpdateFastForward();
}

void UADVSubsystem::UpdateFastForward()
{
	UMovieSceneSequencePlayer* Player = ActivePlayer.Get();
	const bool bWant = (bFastForwardToggled || (bFastForwardHeld && !bFastForwardBlocked))
		&& Player && !bBacklogOpen && !bFadeInProgress;

	if (bWant == bFastForwardActive && (!bWant || Player == FastForwardPlayer.Get())) { return; }

	const bool bWasActive = bFastForwardActive;

	// Back to normal on the player it was applied to
	if (bFastForwardActive)
	{
		if (UMovieSceneSequencePlayer* Old = FastForwardPlayer.Get())
		{
			Old->SetPlayRate(FastForwardSavedPlayRate);
		}
		SetFastForwardAudio(false);
		FastForwardPlayer.Reset();
		bFastForwardActive = false;
	}

	if (bWant)
	{
		ResolveConfig();
		const float Rate = Config ? FMath::Max(Config->FastForwardRate, 1.0f) : 8.0f;

		FastForwardSavedPlayRate = Player->GetPlayRate();
		Player->SetPlayRate(FastForwardSavedPlayRate * Rate);
		SetFastForwardAudio(true);
		FastForwardPlayer  = Player;
		bFastForwardActive = true;
	}

	if (bFastForwardActive != bWasActive)
	{
		OnFastForwardChanged.Broadcast(bFastForwardActive);
	}
}

void UADVSubsystem::SetFastForwardAudio(bool bMute)
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;

	if (!bMute)
	{
		if (bFastForwardMutedAll)
		{
			bFastForwardMutedAll = false;
			FAudioDeviceHandle Device = World ? World->GetAudioDevice() : (GEngine ? GEngine->GetMainAudioDevice() : FAudioDeviceHandle());
			if (Device.IsValid())
			{
				Device->SetTransientPrimaryVolume(FastForwardSavedVolume);
			}
		}
		if (bFastForwardMixPushed)
		{
			bFastForwardMixPushed = false;
			if (World && FastForwardSoundMix)
			{
				UGameplayStatics::PopSoundMixModifier(World, FastForwardSoundMix);
			}
		}
		return;
	}

	if (!Config || !World) { return; }

	switch (Config->FastForwardAudio)
	{
	case EADVFastForwardAudio::MuteAll:
	{
		FAudioDeviceHandle Device = World->GetAudioDevice();
		if (Device.IsValid())
		{
			FastForwardSavedVolume = Device->GetTransientPrimaryVolume();
			Device->SetTransientPrimaryVolume(0.0f);
			bFastForwardMutedAll = true;
		}
		break;
	}
	case EADVFastForwardAudio::MuteVoiceClasses:
	{
		if (!FastForwardSoundMix)
		{
			FastForwardSoundMix = NewObject<USoundMix>(this);
		}
		for (USoundClass* VoiceClass : Config->VoiceSoundClasses)
		{
			if (VoiceClass)
			{
				UGameplayStatics::SetSoundMixClassOverride(World, FastForwardSoundMix, VoiceClass,
					/*Volume*/ 0.0f, /*Pitch*/ 1.0f, /*FadeInTime*/ 0.0f, /*bApplyToChildren*/ true);
			}
		}
		UGameplayStatics::PushSoundMixModifier(World, FastForwardSoundMix);
		bFastForwardMixPushed = true;
		break;
	}
	case EADVFastForwardAudio::KeepPlaying:
	default:
		break;
	}
}

void UADVSubsystem::SetSkipUnread(bool bSkipUnread)
{
	UADVUserSettings* Settings = GetMutableDefault<UADVUserSettings>();
	Settings->bSkipUnread = bSkipUnread;
	Settings->SaveConfig();
}

bool UADVSubsystem::GetSkipUnread() const
{
	return GetDefault<UADVUserSettings>()->bSkipUnread;
}

// ---------------------------------------------------------------------------
// Voice volume
// ---------------------------------------------------------------------------

void UADVSubsystem::SetVoiceVolume(float Volume)
{
	UADVUserSettings* Settings = GetMutableDefault<UADVUserSettings>();
	Settings->VoiceVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	Settings->SaveConfig();

	ApplyVoiceVolume(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
}

float UADVSubsystem::GetVoiceVolume() const
{
	return FMath::Clamp(GetDefault<UADVUserSettings>()->VoiceVolume, 0.0f, 1.0f);
}

void UADVSubsystem::ApplyVoiceVolume(UWorld* World)
{
	if (!World) { return; }

	// Tried for this world (also when there is nothing to apply)
	VoiceVolumeWorld = World;

	ResolveConfig();
	if (!Config || Config->VoiceSoundClasses.Num() == 0) { return; }

	if (!VoiceVolumeMix)
	{
		VoiceVolumeMix = NewObject<USoundMix>(this);
	}

	const float Volume = GetVoiceVolume();
	for (USoundClass* VoiceClass : Config->VoiceSoundClasses)
	{
		if (VoiceClass)
		{
			UGameplayStatics::SetSoundMixClassOverride(World, VoiceVolumeMix, VoiceClass,
				Volume, /*Pitch*/ 1.0f, /*FadeInTime*/ 0.0f, /*bApplyToChildren*/ true);
		}
	}

	if (!bVoiceVolumePushed)
	{
		UGameplayStatics::PushSoundMixModifier(World, VoiceVolumeMix);
		bVoiceVolumePushed = true;
	}
}

void UADVSubsystem::RemoveVoiceVolume(UWorld* World)
{
	if (!World || World != VoiceVolumeWorld.Get()) { return; }

	if (bVoiceVolumePushed && VoiceVolumeMix)
	{
		UGameplayStatics::PopSoundMixModifier(World, VoiceVolumeMix);
	}
	bVoiceVolumePushed = false;
	VoiceVolumeWorld.Reset();
}

// ---------------------------------------------------------------------------
// Read history (system data)
// ---------------------------------------------------------------------------

FString UADVSubsystem::GetSystemSlotName() const
{
	return Config && !Config->SystemSaveSlotName.IsEmpty() ? Config->SystemSaveSlotName : FString(TEXT("CinematicADV_System"));
}

UADVSystemSaveGame* UADVSubsystem::GetSystemData()
{
	if (!SystemData)
	{
		ResolveConfig();
		const FString SlotName = GetSystemSlotName();
		if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
		{
			SystemData = Cast<UADVSystemSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
		}
		if (!SystemData)
		{
			SystemData = Cast<UADVSystemSaveGame>(UGameplayStatics::CreateSaveGameObject(UADVSystemSaveGame::StaticClass()));
		}
	}
	return SystemData;
}

bool UADVSubsystem::MarkLineRead(const FString& LineKey)
{
	UADVSystemSaveGame* Data = GetSystemData();
	if (!Data) { return false; }

	bool bAlreadyRead = false;
	Data->ReadLines.Add(LineKey, &bAlreadyRead);
	if (!bAlreadyRead)
	{
		bSystemDataDirty = true;
	}
	return !bAlreadyRead;
}

void UADVSubsystem::ClearReadHistory()
{
	if (UADVSystemSaveGame* Data = GetSystemData())
	{
		Data->ReadLines.Reset();
		bSystemDataDirty = true;
		SaveSystemData(/*bAsync*/ false);
	}
}

void UADVSubsystem::SaveSystemData(bool bAsync)
{
	if (!SystemData || !bSystemDataDirty) { return; }

	bSystemDataDirty  = false;
	SystemSaveElapsed = 0.0f;

	const FString SlotName = GetSystemSlotName();
	if (bAsync)
	{
		UGameplayStatics::AsyncSaveGameToSlot(SystemData, SlotName, 0);
	}
	else
	{
		UGameplayStatics::SaveGameToSlot(SystemData, SlotName, 0);
	}
}

// ---------------------------------------------------------------------------
// Input — Skip (hold)
// ---------------------------------------------------------------------------

void UADVSubsystem::OnSkipPressed()
{
	if (!ActivePlayer.IsValid() || bFadeInProgress || bBacklogOpen) { return; }
	bSkipHeld        = true;
	SkipHoldElapsed  = 0.0f;
	ShowSkipGauge();
}

void UADVSubsystem::OnSkipReleased()
{
	if (!bSkipHeld) { return; }
	bSkipHeld       = false;
	SkipHoldElapsed = 0.0f;
	HideSkipGauge();
}

void UADVSubsystem::Skip()
{
	// Blueprint-callable direct skip (no hold required)
	DoSkip();
}

void UADVSubsystem::DoSkip()
{
	if (!ActivePlayer.IsValid() || bFadeInProgress) { return; }

	// Skipping from the backlog (Blueprint): close it without resuming
	bResumeAfterBacklog = false;
	CloseBacklog();

	APlayerController* PC = GetLocalController();
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!PC || !World) { return; }

	ResolveConfig();
	bFadeInProgress = true;
	bSectionActive  = false;

	// Fade to black
	if (PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(
			0.0f, 1.0f,
			FMath::Max(ConfigFadeDuration, KINDA_SMALL_NUMBER),
			FLinearColor::Black,
			/*bShouldFadeAudio*/ false,
			/*bHoldWhenFinished*/ true);
	}

	// Stop sequencer after fade — fires OnStop → external bindings notified
	TWeakObjectPtr<UADVSubsystem> WeakThis(this);
	World->GetTimerManager().SetTimer(
		SkipFadeTimerHandle,
		[WeakThis]()
		{
			UADVSubsystem* Self = WeakThis.Get();
			if (!Self) { return; }

			Self->bFadeInProgress = false;
			if (UMovieSceneSequencePlayer* Player = Self->ActivePlayer.Get())
			{
				Player->Stop(); // OnStop delegate fires here
			}

			// Fade back in (the held black would otherwise stay on screen)
			APlayerController* Controller = Self->GetLocalController();
			if (Self->bConfigFadeInAfterSkip && Controller && Controller->PlayerCameraManager)
			{
				if (Self->ConfigFadeInDuration > 0.0f)
				{
					Controller->PlayerCameraManager->StartCameraFade(
						1.0f, 0.0f, Self->ConfigFadeInDuration, FLinearColor::Black,
						/*bShouldFadeAudio*/ false, /*bHoldWhenFinished*/ false);
				}
				else
				{
					Controller->PlayerCameraManager->StopCameraFade();
				}
			}
		},
		FMath::Max(ConfigFadeDuration, KINDA_SMALL_NUMBER),
		/*bLoop*/ false);
}

// ---------------------------------------------------------------------------
// Gauge widget
// ---------------------------------------------------------------------------

void UADVSubsystem::ShowSkipGauge()
{
	if (!GEngine || !GEngine->GameViewport) { return; }
	if (SkipGaugeContainer.IsValid()) { return; }

	TWeakObjectPtr<UADVSubsystem> WeakSelf(this);

	SAssignNew(SkipGaugeContainer, SOverlay)
	+ SOverlay::Slot()
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Bottom)
	.Padding(FMargin(0.f, 0.f, 50.f, 50.f))
	[
		SAssignNew(SkipGaugeSlate, SSkipGaugeWidget)
		.Progress_Lambda([WeakSelf]() -> float
		{
			if (!WeakSelf.IsValid()) { return 0.0f; }
			return FMath::Clamp(
				WeakSelf->SkipHoldElapsed / WeakSelf->ConfigHoldDuration,
				0.0f, 1.0f);
		})
		.GaugeColor(ConfigGaugeColor)
		.BackgroundColor(ConfigGaugeBgColor)
		.WidgetSize(ConfigGaugeSize)
	];

	GEngine->GameViewport->AddViewportWidgetContent(
		SkipGaugeContainer.ToSharedRef(), 100);
}

void UADVSubsystem::HideSkipGauge()
{
	if (!SkipGaugeContainer.IsValid()) { return; }

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(
			SkipGaugeContainer.ToSharedRef());
	}
	SkipGaugeContainer.Reset();
	SkipGaugeSlate.Reset();
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

void UADVSubsystem::Tick(float DeltaTime)
{
	// Handle hold-to-skip progress
	if (bSkipHeld)
	{
		SkipHoldElapsed += DeltaTime;
		if (SkipHoldElapsed >= ConfigHoldDuration)
		{
			bSkipHeld = false;
			HideSkipGauge();
			DoSkip();
		}
		// Gauge updates via TAttribute lambda — no explicit update needed
	}

	// Pick up ADV sequences before their first wait section
	PollForAdvPlayer(DeltaTime);

	// Record lines for the backlog (normally bound when the world starts; this catches the rest)
	BindSubtitleEvents(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);

	// Fast-forward on / off (also stops at an unread line found during this frame's evaluation)
	UpdateFastForward();

	// Player's voice volume for this world (normally applied when the world starts)
	UWorld* GameWorld = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (GameWorld && GameWorld != VoiceVolumeWorld.Get())
	{
		ApplyVoiceVolume(GameWorld);
	}

	// A loaded game resumes once its level has begun play
	TickPendingLoad();

	// Keep the read history
	if (bSystemDataDirty)
	{
		SystemSaveElapsed += DeltaTime;
		if (SystemSaveElapsed >= 10.0f)
		{
			SaveSystemData(/*bAsync*/ true);
		}
	}

	// The sequence and auto mode wait while the backlog is open
	if (bBacklogOpen) { return; }

	// Process advance (works even when paused)
	if (bAdvanceRequested)
	{
		bAdvanceRequested = false;
		HandleAdvance();
		return;
	}

	if (!bSectionActive) { return; }

	UMovieSceneSequencePlayer* Player = ActivePlayer.Get();
	if (!Player)
	{
		bSectionActive = false;
		return;
	}

	// New section: let the player run up to the section end and pause there by itself
	if (bPendingPlayTo)
	{
		bPendingPlayTo = false;
		PlayToSectionEnd();
		return;
	}

	// Paused at the section end: loop, or keep waiting for Advance()
	if (Player->IsPaused() && IsAtSectionEnd(Player))
	{
		if (ActiveMode == EClickWaitMode::Loop)
		{
			LoopToStart();
		}
		// Stop: stay paused — waiting for Advance()
	}

	// Fast-forward: pass the wait at once
	if (bFastForwardActive)
	{
		if (IsWaitReached(Player))
		{
			AdvancePastWait();
		}
		return;
	}

	// Auto mode: continue by itself after the delay
	if (bAutoMode)
	{
		TickAuto(Player, DeltaTime);
	}
}

bool UADVSubsystem::IsTickable() const
{
	// Ticks while the game runs (the class default object never ticks)
	return !HasAnyFlags(RF_ClassDefaultObject) && GetGameInstance() && GetGameInstance()->GetWorld();
}

TStatId UADVSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UADVSubsystem, STATGROUP_Tickables);
}

// ---------------------------------------------------------------------------
// Playback control
// ---------------------------------------------------------------------------

void UADVSubsystem::PlayToSectionEnd()
{
	UMovieSceneSequencePlayer* Player = ActivePlayer.Get();
	if (!Player) { return; }

	// The player pauses exactly at the section end, before any frame after it is evaluated
	// (checking in Tick would let the next line / audio start for a frame first)
	FMovieSceneSequencePlaybackParams Params;
	Params.Frame        = ActiveSectionEnd;
	Params.PositionType = EMovieScenePositionType::Frame;
	Params.UpdateMethod = EUpdatePositionMethod::Play;

	FMovieSceneSequencePlayToParams PlayToParams;
	PlayToParams.bExclusive = true;

	Player->PlayTo(Params, PlayToParams);
}

bool UADVSubsystem::IsAtSectionEnd(UMovieSceneSequencePlayer* Player) const
{
	const FQualifiedFrameTime Current = Player->GetCurrentTime();
	const FFrameTime CurrentInRate = FFrameRate::TransformTime(Current.Time, Current.Rate, ActiveDisplayRate);

	// Within one frame of the end (an exclusive PlayTo stops just before it)
	return CurrentInRate.AsDecimal() >= ActiveSectionEnd.AsDecimal() - 1.0;
}

void UADVSubsystem::JumpPastSection()
{
	UMovieSceneSequencePlayer* Player = ActivePlayer.Get();
	if (!Player) { return; }

	// Land exactly on the section end: the first frame after the section is not skipped
	FMovieSceneSequencePlaybackParams Params;
	Params.Frame        = ActiveSectionEnd;
	Params.PositionType = EMovieScenePositionType::Frame;
	Params.UpdateMethod = EUpdatePositionMethod::Jump;
	Player->SetPlaybackPosition(Params);
	Player->Play();
}

void UADVSubsystem::LoopToStart()
{
	UMovieSceneSequencePlayer* Player = ActivePlayer.Get();
	if (!Player) { return; }

	FMovieSceneSequencePlaybackParams Params;
	Params.Frame        = ActiveSectionStart;
	Params.PositionType = EMovieScenePositionType::Frame;
	Params.UpdateMethod = EUpdatePositionMethod::Jump;
	Player->SetPlaybackPosition(Params);

	PlayToSectionEnd();
}

void UADVSubsystem::OnPlayerStopped()
{
	// Cancel gauge and any pending fade timer
	if (bSkipHeld)
	{
		bSkipHeld       = false;
		SkipHoldElapsed = 0.0f;
		HideSkipGauge();
	}

	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		World->GetTimerManager().ClearTimer(SkipFadeTimerHandle);
	}
	bFadeInProgress = false;

	// Fast-forward mode ends with the sequence
	bFastForwardToggled = false;

	// Forget the player (a later sequence registers itself) and give the keys back to the game
	ClearActivePlayer();

	SaveSystemData(/*bAsync*/ true);
}
