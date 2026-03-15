// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Misc/FrameTime.h"
#include "TimerManager.h"
#include "ClickWaitSection.h"
#include "ADVSubsystem.generated.h"

class ULevelSequencePlayer;
class SSkipGaugeWidget;

/**
 * Central subsystem for CinematicADV.
 *
 * Zero-Blueprint setup:
 *   1. Create a UCinematicADVConfig DataAsset in Content.
 *   2. Set InputMappingContext, AdvanceAction, and optionally SkipAction.
 *   3. Add a Click Wait Track to your Level Sequence and place sections.
 *   That's it — player registration and input binding happen automatically.
 *
 * Manual override: Call RegisterSequencePlayer() from Blueprint if needed.
 */
UCLASS()
class CINEMATICADV_API UADVSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// --- Manual override (optional) ---

	/** Manually register a sequence player. Called automatically if not used. */
	UFUNCTION(BlueprintCallable, Category="CinematicADV")
	void RegisterSequencePlayer(ULevelSequencePlayer* Player);

	// --- Input ---

	/**
	 * Advance past the current wait section.
	 * Normally called automatically via Enhanced Input binding.
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

	// --- Called internally by FClickWaitEvalTemplate ---

	void OnSectionEntered(EClickWaitMode Mode, FFrameTime Start, FFrameTime End, FFrameRate Rate);

	// --- FTickableGameObject ---

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

private:
	void TryAutoRegisterPlayer();
	void TryBindInput();
	void JumpPastSection();
	void LoopToStart();

	/** Input callbacks for hold-to-skip. */
	void OnSkipPressed();
	void OnSkipReleased();

	/** Performs the actual fade-to-black → Player->Stop() sequence. */
	void DoSkip();

	void ShowSkipGauge();
	void HideSkipGauge();

	UFUNCTION()
	void OnPlayerStopped();

	UPROPERTY()
	TWeakObjectPtr<ULevelSequencePlayer> ActivePlayer;

	bool           bSectionActive    = false;
	bool           bAdvanceRequested = false;
	bool           bInputBound       = false;
	bool           bSkipHeld         = false;
	bool           bFadeInProgress   = false;
	EClickWaitMode ActiveMode        = EClickWaitMode::Loop;
	FFrameTime     ActiveSectionStart;
	FFrameTime     ActiveSectionEnd;
	FFrameRate     ActiveDisplayRate;

	// Config cache (populated in TryBindInput)
	float        ConfigFadeDuration = 0.5f;
	float        ConfigHoldDuration = 1.0f;
	float        ConfigGaugeSize    = 80.0f;
	FLinearColor ConfigGaugeColor   = FLinearColor(1.f, 0.8f, 0.f, 1.f);
	FLinearColor ConfigGaugeBgColor = FLinearColor(0.f, 0.f, 0.f, 0.55f);

	float        SkipHoldElapsed = 0.0f;
	FTimerHandle SkipFadeTimerHandle;

	TSharedPtr<SWidget>          SkipGaugeContainer;
	TSharedPtr<SSkipGaugeWidget> SkipGaugeSlate;
};
