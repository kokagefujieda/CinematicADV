// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Misc/FrameTime.h"
#include "Misc/FrameRate.h"
#include "TimerManager.h"
#include "ClickWaitSection.h"
#include "CinematicADVConfig.h"
#include "SubtitleSettings.h"
#include "ADVTypes.h"
#include "ADVSubsystem.generated.h"

class ULevelSequencePlayer;
class UMovieSceneSequence;
class UMovieSceneSequencePlayer;
class APlayerController;
class ULocalPlayer;
class UCinematicADVConfig;
class SSkipGaugeWidget;
class UMovieSceneSection;
class USubtitleSubsystem;
class USoundBase;
class UAudioComponent;
class USoundMix;
class UADVSystemSaveGame;
class UADVSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnADVAutoModeChanged, bool, bAutoMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnADVBacklogEntryAdded, const FADVBacklogEntry&, Entry);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnADVBacklogEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnADVFastForwardChanged, bool, bFastForwarding);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnADVSaveSlotEvent, int32, SlotIndex);

/** One Click Wait section in the player's root time (seconds). */
struct FADVWaitPoint
{
	uint32         Key   = 0;
	EClickWaitMode Mode  = EClickWaitMode::Stop;
	double         Start = 0.0;
	double         End   = 0.0;
};

/**
 * Central subsystem for CinematicADV.
 *
 * Zero-Blueprint setup:
 *   1. Create a UCinematicADVConfig DataAsset in Content.
 *   2. Set InputMappingContext, AdvanceAction, and optionally SkipAction / AutoAction.
 *   3. Set it in Project Settings → Plugins → CinematicADV → Config Asset.
 *   4. Add a Click Wait Track to your Level Sequence and place sections.
 *   The player that plays a Click Wait section is picked up automatically, and the input
 *   mapping is active only while that sequence plays.
 *
 * Manual override: Call RegisterSequencePlayer() from Blueprint if needed.
 */
UCLASS()
class CINEMATICADV_API UADVSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Manual override (optional) ---

	/** Manually register a sequence player (enables input for it). Called automatically if not used. */
	UFUNCTION(BlueprintCallable, Category="CinematicADV")
	void RegisterSequencePlayer(ULevelSequencePlayer* Player);

	// --- Input ---

	/**
	 * Advance (normally called via the Enhanced Input binding):
	 * - Stop section while a typewriter is still revealing text: jump to the wait position (whole text shown)
	 * - Stop / Loop section otherwise: continue after the section
	 * - Outside a wait section: jump to the next wait (Config: ClickOutsideWait)
	 */
	UFUNCTION(BlueprintCallable, Category="CinematicADV")
	void Advance();

	/**
	 * Immediately fade to black and stop the sequence.
	 * Fires OnStop so all external bindings are notified.
	 * Can be called from Blueprint for a direct skip without hold.
	 */
	UFUNCTION(BlueprintCallable, Category="CinematicADV")
	void Skip();

	// --- State ---

	UFUNCTION(BlueprintPure, Category="CinematicADV")
	bool IsWaiting() const { return bSectionActive; }

	// --- Auto mode ---

	/**
	 * Auto mode: each wait continues by itself once the text is fully shown and a delay has passed
	 * (voice: until the voice ends + AutoDelayAfterVoice; no voice: AutoBaseDelay + AutoDelayPerChar × characters).
	 * Clicking still works while auto mode is on. It stays on until turned off (also across sequences).
	 */
	UFUNCTION(BlueprintCallable, Category="CinematicADV|Auto")
	void SetAutoMode(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category="CinematicADV|Auto")
	void ToggleAutoMode();

	UFUNCTION(BlueprintPure, Category="CinematicADV|Auto")
	bool IsAutoMode() const { return bAutoMode; }

	/** Fires when auto mode is turned on or off (to show an "AUTO" indicator, etc.). */
	UPROPERTY(BlueprintAssignable, Category="CinematicADV|Auto")
	FOnADVAutoModeChanged OnAutoModeChanged;

	// --- Backlog ---

	/**
	 * Lines shown so far, oldest first. Lines of Sequencer Subtitles are recorded while a sequence with
	 * Click Wait sections plays; add other lines with AddBacklogEntry. Kept while the game runs.
	 */
	UFUNCTION(BlueprintPure, Category="CinematicADV|Backlog")
	TArray<FADVBacklogEntry> GetBacklogEntries() const { return BacklogEntries; }

	/** Add a line yourself (e.g. text shown by your own UI). */
	UFUNCTION(BlueprintCallable, Category="CinematicADV|Backlog")
	void AddBacklogEntry(const FText& SpeakerName, const FText& Text, USoundBase* Voice = nullptr);

	UFUNCTION(BlueprintCallable, Category="CinematicADV|Backlog")
	void ClearBacklog();

	/** Open the backlog: the sequence (and auto mode) pauses until it is closed. */
	UFUNCTION(BlueprintCallable, Category="CinematicADV|Backlog")
	void OpenBacklog();

	UFUNCTION(BlueprintCallable, Category="CinematicADV|Backlog")
	void CloseBacklog();

	UFUNCTION(BlueprintCallable, Category="CinematicADV|Backlog")
	void ToggleBacklog();

	UFUNCTION(BlueprintPure, Category="CinematicADV|Backlog")
	bool IsBacklogOpen() const { return bBacklogOpen; }

	/** Play the voice of a backlog line (index into Get Backlog Entries). Stops the voice played before. */
	UFUNCTION(BlueprintCallable, Category="CinematicADV|Backlog")
	void PlayBacklogVoice(int32 Index);

	UFUNCTION(BlueprintCallable, Category="CinematicADV|Backlog")
	void StopBacklogVoice();

	UPROPERTY(BlueprintAssignable, Category="CinematicADV|Backlog")
	FOnADVBacklogEntryAdded OnBacklogEntryAdded;

	UPROPERTY(BlueprintAssignable, Category="CinematicADV|Backlog")
	FOnADVBacklogEvent OnBacklogOpened;

	UPROPERTY(BlueprintAssignable, Category="CinematicADV|Backlog")
	FOnADVBacklogEvent OnBacklogClosed;

	// --- Fast forward / read history ---

	/**
	 * Fast-forward mode (the toggle; FastForwardAction also fast-forwards while held).
	 * Turns itself off at a line not read yet (unless Skip Unread) and when the sequence ends.
	 */
	UFUNCTION(BlueprintCallable, Category="CinematicADV|FastForward")
	void SetFastForwardMode(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="CinematicADV|FastForward")
	bool IsFastForwardMode() const { return bFastForwardToggled; }

	/** true while the sequence is actually being fast-forwarded (toggle or key held). */
	UFUNCTION(BlueprintPure, Category="CinematicADV|FastForward")
	bool IsFastForwarding() const { return bFastForwardActive; }

	/** Fires when fast-forwarding starts or stops (to show a "SKIP" indicator, etc.). */
	UPROPERTY(BlueprintAssignable, Category="CinematicADV|FastForward")
	FOnADVFastForwardChanged OnFastForwardChanged;

	/** Player option: fast-forward also skips lines not read yet. Saved at once (GameUserSettings.ini). */
	UFUNCTION(BlueprintCallable, Category="CinematicADV|FastForward")
	void SetSkipUnread(bool bSkipUnread);

	UFUNCTION(BlueprintPure, Category="CinematicADV|FastForward")
	bool GetSkipUnread() const;

	/** Forget which lines have been read (all save slots share it). */
	UFUNCTION(BlueprintCallable, Category="CinematicADV|FastForward")
	void ClearReadHistory();

	// --- Voice volume ---

	/** Player option: volume of the voice Sound Classes (Config: VoiceSoundClasses), 0 - 1. Saved at once. */
	UFUNCTION(BlueprintCallable, Category="CinematicADV|Voice")
	void SetVoiceVolume(float Volume);

	UFUNCTION(BlueprintPure, Category="CinematicADV|Voice")
	float GetVoiceVolume() const;

	// --- Save / Load ---

	/**
	 * Save to a slot: the level, the ADV sequence and its position (at a wait: that wait), the backlog and the variables.
	 * Call Capture Save Thumbnail before opening your save screen to add a picture.
	 */
	UFUNCTION(BlueprintCallable, Category="CinematicADV|Save")
	bool SaveGameToSlot(int32 SlotIndex);

	/**
	 * Load a slot: the variables and the backlog come back at once, then the saved level is opened and the sequence
	 * resumes at the saved wait (On Game Loaded fires then). Event Track events before that point are not run again.
	 */
	UFUNCTION(BlueprintCallable, Category="CinematicADV|Save")
	bool LoadGameFromSlot(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category="CinematicADV|Save")
	bool DoesSaveSlotExist(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category="CinematicADV|Save")
	bool DeleteSaveSlot(int32 SlotIndex);

	/** Date, last line, level and thumbnail of a slot. false if the slot is empty. */
	UFUNCTION(BlueprintCallable, Category="CinematicADV|Save")
	bool GetSaveSlotInfo(int32 SlotIndex, FADVSaveSlotInfo& OutInfo);

	/**
	 * Take the picture for the next saves (the game screen without UI, ready on the next frame).
	 * Call it before your save screen opens so the screen itself is not in the picture.
	 */
	UFUNCTION(BlueprintCallable, Category="CinematicADV|Save")
	void CaptureSaveThumbnail();

	/** true from Load Game From Slot until the sequence has resumed. Check it before starting your own sequence in BeginPlay. */
	UFUNCTION(BlueprintPure, Category="CinematicADV|Save")
	bool IsLoadingGame() const { return PendingLoad != nullptr; }

	UPROPERTY(BlueprintAssignable, Category="CinematicADV|Save")
	FOnADVSaveSlotEvent OnGameSaved;

	/** Fires after a loaded game has resumed (level opened, sequence at the saved position). */
	UPROPERTY(BlueprintAssignable, Category="CinematicADV|Save")
	FOnADVSaveSlotEvent OnGameLoaded;

	// --- Variables (saved with the game) ---

	UFUNCTION(BlueprintCallable, Category="CinematicADV|Variables")
	void SetStringVariable(FName Name, const FString& Value);

	UFUNCTION(BlueprintPure, Category="CinematicADV|Variables")
	FString GetStringVariable(FName Name, const FString& DefaultValue) const;

	UFUNCTION(BlueprintCallable, Category="CinematicADV|Variables")
	void SetNumberVariable(FName Name, double Value);

	UFUNCTION(BlueprintPure, Category="CinematicADV|Variables")
	double GetNumberVariable(FName Name, double DefaultValue = 0.0) const;

	UFUNCTION(BlueprintCallable, Category="CinematicADV|Variables")
	void SetFlag(FName Name, bool bValue);

	/** false if the flag was never set. */
	UFUNCTION(BlueprintPure, Category="CinematicADV|Variables")
	bool GetFlag(FName Name) const;

	/** All variables (e.g. for a debug display). */
	UFUNCTION(BlueprintPure, Category="CinematicADV|Variables")
	FADVVariables GetAllVariables() const { return Variables; }

	/** Forget all variables (e.g. for a new game). */
	UFUNCTION(BlueprintCallable, Category="CinematicADV|Variables")
	void ClearVariables();

	// --- Called internally by FClickWaitEvalTemplate ---

	/**
	 * The player is inside a wait section. Times are seconds in the evaluated (sub)sequence's own time;
	 * they are converted to the player's time so waits inside sub-sequences work.
	 * (Sub-sequences played at a rate other than 1 are not supported.)
	 */
	void OnSectionEvaluated(UMovieSceneSequencePlayer* Player, uint32 SectionKey, EClickWaitMode Mode,
		double LocalNow, double LocalStart, double LocalEnd);

	// --- FTickableGameObject ---

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

private:
	// Player
	void SetActivePlayer(UMovieSceneSequencePlayer* Player);
	void ClearActivePlayer();

	// Config / input
	UCinematicADVConfig* ResolveConfig();
	void EnsureInputBound();
	void AddInputContext();
	void RemoveInputContext();
	APlayerController* GetLocalController() const;

	/**
	 * Calls Visit for every active section of a sequence (master and object-binding tracks) and of its
	 * sub-sequences / shots (one level), with its range in the root sequence's time (seconds, clipped to the sub-section).
	 */
	static void ForEachSectionInRootTime(UMovieSceneSequence* Sequence,
		TFunctionRef<void(const UMovieSceneSection* Section, double Start, double End)> Visit);

	/** Click Wait sections of a sequence: its own tracks and those of its sub-sequences / shots (one level). */
	static void CollectWaitPoints(UMovieSceneSequence* Sequence, TArray<FADVWaitPoint>& OutWaits);

	/** Find a playing sequence that contains Click Wait sections, so input works from its first frame. */
	void PollForAdvPlayer(float DeltaTime);
	void TryFindAdvPlayer();

	// Advance handling
	void HandleAdvance();
	bool IsTextRevealing() const;
	USubtitleSubsystem* GetSubtitleSubsystem() const;
	void JumpToWaitPosition();
	void JumpToNextWait();

	/** Wait on a wait section as if it had been reached by playing (Stop: at its end, Loop: from its start). */
	void EnterWait(const FADVWaitPoint& Wait, FFrameRate DisplayRate);

	/** Leave the current wait and play on from its end (click or auto). */
	void AdvancePastWait();

	/** Stop: paused at the section end. Loop: inside the section. */
	bool IsWaitReached(UMovieSceneSequencePlayer* Player) const;

	// Auto mode
	void TickAuto(UMovieSceneSequencePlayer* Player, float DeltaTime);
	void ResetAutoTimer();

	/** Seconds to wait before auto-advancing the current wait (counted from when it is reached and the text is shown). */
	float ComputeAutoDelay(UMovieSceneSequencePlayer* Player) const;

	/** Whether an audio section plays a voice (Config: VoiceSoundClasses / VoiceAssetKeywords). */
	bool IsVoiceSection(const UMovieSceneSection* Section) const;

	// Backlog
	void BindSubtitleEvents(UWorld* World);

	UFUNCTION()
	void HandleSubtitleSlotStarted(int32 SlotID, const FText& SubtitleText, const FText& SpeakerName, const FSubtitleAppearance& Appearance);

	/**
	 * A subtitle section (SlotID = its UniqueID): its key for the read history (path name) and its voice
	 * (the voice starting closest to the line).
	 */
	void FindLineInfo(UMovieSceneSequencePlayer* Player, uint32 SlotID, FString& OutLineKey, USoundBase*& OutVoice) const;

	void AddBacklogEntryInternal(const FADVBacklogEntry& Entry);

	// Fast forward
	void OnFastForwardPressed();
	void OnFastForwardReleased();
	void ToggleFastForwardMode();
	/** Apply the wanted fast-forward state to the player and the sound. */
	void UpdateFastForward();
	void SetFastForwardAudio(bool bMute);

	// Read history (system data)
	UADVSystemSaveGame* GetSystemData();
	FString GetSystemSlotName() const;
	/** Marks a line read; returns true if it had not been read before. */
	bool MarkLineRead(const FString& LineKey);
	void SaveSystemData(bool bAsync);

	// Voice volume
	void ApplyVoiceVolume(UWorld* World);
	void RemoveVoiceVolume(UWorld* World);

	// Save / Load
	FString GetSaveSlotName(int32 SlotIndex);
	static FString GetWorldLevelPath(const UWorld* World);
	void TickPendingLoad();
	void RestoreFromSave(UWorld* World, UADVSaveGame* Save);
	void HandleScreenshotCaptured(int32 Width, int32 Height, const TArray<FColor>& Colors);
	void ShowBacklogUI();
	void HideBacklogUI();

	// Playback control
	void PlayToSectionEnd();
	bool IsAtSectionEnd(UMovieSceneSequencePlayer* Player) const;
	void JumpPastSection();
	void LoopToStart();

	/** Input callbacks for hold-to-skip. */
	void OnSkipPressed();
	void OnSkipReleased();

	/** Performs the actual fade-to-black → Player->Stop() sequence (then fades back in if configured). */
	void DoSkip();

	void ShowSkipGauge();
	void HideSkipGauge();

	/** Bound to OnStop and OnFinished of the active player. */
	UFUNCTION()
	void OnPlayerStopped();

	/** A world is going away (level travel): forget its player and give the keys back. */
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	FDelegateHandle WorldCleanupHandle;
	FDelegateHandle WorldActorsInitializedHandle;

	UPROPERTY()
	TWeakObjectPtr<UMovieSceneSequencePlayer> ActivePlayer;

	/** Resolved config (kept referenced while the game instance lives). */
	UPROPERTY()
	TObjectPtr<UCinematicADVConfig> Config;

	/** Controller the input actions are bound to (re-bound when it changes, e.g. after level travel). */
	TWeakObjectPtr<APlayerController> BoundController;

	/** Local player the input mapping context was added to. */
	TWeakObjectPtr<ULocalPlayer> ContextLocalPlayer;
	bool           bContextAdded     = false;

	/** Sequences already checked for Click Wait sections. */
	TMap<TWeakObjectPtr<UMovieSceneSequence>, bool> AdvSequenceCache;
	float          PollElapsed       = 0.0f;

	bool           bSectionActive    = false;
	bool           bAutoMode         = false;
	bool           bAdvanceRequested = false;
	bool           bPendingPlayTo    = false;
	bool           bSkipHeld         = false;
	bool           bFadeInProgress   = false;
	EClickWaitMode ActiveMode        = EClickWaitMode::Loop;
	uint32         ActiveSectionKey  = 0;

	/** Section range in the player's display rate. */
	FFrameTime     ActiveSectionStart;
	FFrameTime     ActiveSectionEnd;
	FFrameRate     ActiveDisplayRate;

	/** Auto mode: time counted at the current wait, and its delay (computed once per wait; < 0 = not yet). */
	float          AutoElapsed = 0.0f;
	float          AutoDelay   = -1.0f;

	/** Where playback last left a wait (player's time, seconds). Voices starting after it belong to the next wait. */
	double         LastWaitEndSeconds = TNumericLimits<double>::Lowest();

	// Config cache (populated in ResolveConfig)
	EADVClickOutsideWait ConfigClickOutsideWait = EADVClickOutsideWait::JumpToNextWait;
	float        ConfigFadeDuration     = 0.5f;
	bool         bConfigFadeInAfterSkip = true;
	float        ConfigFadeInDuration   = 0.5f;
	float        ConfigHoldDuration     = 1.0f;
	float        ConfigGaugeSize        = 80.0f;
	float        ConfigAutoBaseDelay       = 1.0f;
	float        ConfigAutoDelayPerChar    = 0.05f;
	float        ConfigAutoDelayAfterVoice = 0.5f;
	FLinearColor ConfigGaugeColor       = FLinearColor(1.f, 0.8f, 0.f, 1.f);
	FLinearColor ConfigGaugeBgColor     = FLinearColor(0.f, 0.f, 0.f, 0.55f);

	// Backlog
	UPROPERTY()
	TArray<FADVBacklogEntry> BacklogEntries;

	/** Subtitle sections already recorded at the current wait (a Loop restarting them adds nothing). */
	TSet<int32>  BacklogSlotsAtWait;

	TWeakObjectPtr<USubtitleSubsystem> BoundSubtitles;
	TWeakObjectPtr<UAudioComponent>    BacklogVoiceComponent;
	TSharedPtr<SWidget>                BacklogWidget;

	bool         bBacklogOpen          = false;
	bool         bResumeAfterBacklog   = false;
	bool         bBacklogChangedCursor = false;
	bool         bSavedShowMouseCursor = false;

	// Fast forward
	bool         bFastForwardToggled = false;
	bool         bFastForwardHeld    = false;
	/** Stopped at an unread line while the key is held: waits for the key to be released. */
	bool         bFastForwardBlocked = false;
	bool         bFastForwardActive  = false;
	float        FastForwardSavedPlayRate = 1.0f;
	TWeakObjectPtr<UMovieSceneSequencePlayer> FastForwardPlayer;

	bool         bFastForwardMutedAll   = false;
	float        FastForwardSavedVolume = 1.0f;
	bool         bFastForwardMixPushed  = false;

	UPROPERTY()
	TObjectPtr<USoundMix> FastForwardSoundMix;

	// Voice volume
	UPROPERTY()
	TObjectPtr<USoundMix> VoiceVolumeMix;
	/** World the voice volume was last applied for. */
	TWeakObjectPtr<UWorld> VoiceVolumeWorld;
	bool         bVoiceVolumePushed = false;

	// Save / Load
	UPROPERTY()
	FADVVariables Variables;

	/** Loaded save waiting for its level to begin play. */
	UPROPERTY()
	TObjectPtr<UADVSaveGame> PendingLoad;
	int32        PendingLoadSlot = -1;
	TWeakObjectPtr<UWorld> PendingLoadFromWorld;

	/** PNG taken by CaptureSaveThumbnail. */
	TArray<uint8>   PendingThumbnail;
	FDelegateHandle ScreenshotHandle;

	// Read history
	UPROPERTY()
	TObjectPtr<UADVSystemSaveGame> SystemData;
	bool         bSystemDataDirty  = false;
	float        SystemSaveElapsed = 0.0f;

	float        SkipHoldElapsed = 0.0f;
	FTimerHandle SkipFadeTimerHandle;

	TSharedPtr<SWidget>          SkipGaugeContainer;
	TSharedPtr<SSkipGaugeWidget> SkipGaugeSlate;
};
