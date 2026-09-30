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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnADVAutoModeChanged, bool, bAutoMode);

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

	// Advance handling
	void HandleAdvance();
	bool IsTextRevealing() const;
	const USubtitleSubsystem* GetSubtitleSubsystem() const;
	void JumpToWaitPosition();
	void JumpToNextWait();

	/** Leave the current wait and play on from its end (click or auto). */
	void AdvancePastWait();

	// Auto mode
	void TickAuto(UMovieSceneSequencePlayer* Player, float DeltaTime);
	void ResetAutoTimer();

	/** Seconds to wait before auto-advancing the current wait (counted from when it is reached and the text is shown). */
	float ComputeAutoDelay(UMovieSceneSequencePlayer* Player) const;

	/** Whether an audio section plays a voice (Config: VoiceSoundClasses / VoiceAssetKeywords). */
	bool IsVoiceSection(const UMovieSceneSection* Section) const;

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

	float        SkipHoldElapsed = 0.0f;
	FTimerHandle SkipFadeTimerHandle;

	TSharedPtr<SWidget>          SkipGaugeContainer;
	TSharedPtr<SSkipGaugeWidget> SkipGaugeSlate;
};
