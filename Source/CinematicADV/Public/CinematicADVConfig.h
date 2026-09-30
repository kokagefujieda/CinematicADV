// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CinematicADVConfig.generated.h"

class UInputAction;
class UInputMappingContext;
class USoundClass;

/** What a click does while the sequence is playing outside a Click Wait section. */
UENUM(BlueprintType)
enum class EADVClickOutsideWait : uint8
{
	/** Jump to the next Click Wait (Stop: its end, waiting; Loop: its start). */
	JumpToNextWait UMETA(DisplayName = "Jump to Next Wait"),
	/** Ignore the click. */
	DoNothing      UMETA(DisplayName = "Do Nothing"),
};

/**
 * CinematicADV configuration DataAsset.
 *
 * How to use:
 *   1. Create this DataAsset anywhere in your Content folder
 *      (right-click → Miscellaneous → Data Asset → CinematicADVConfig).
 *   2. Set InputMappingContext to the IMC that maps your advance key (e.g. Left Click, Enter).
 *   3. Set AdvanceAction to the Input Action bound in that IMC.
 *   4. Set it in Project Settings → Plugins → CinematicADV → Config Asset.
 *      (Without this the asset is found automatically in the editor, but may not be packaged.)
 */
UCLASS(BlueprintType)
class CINEMATICADV_API UCinematicADVConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Input Mapping Context that contains the AdvanceAction mapping.
	 * The plugin adds it (priority 90) only while a sequence with Click Wait sections is playing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputMappingContext> InputMappingContext;

	/**
	 * Input Action that advances past the current wait section.
	 * Must be mapped to a key inside InputMappingContext.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> AdvanceAction;

	/**
	 * Input Action that skips the entire sequence (fade to black → Stop).
	 * Must be mapped to a key inside InputMappingContext.
	 * Leave empty to disable skip.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> SkipAction;

	/**
	 * Click while the sequence plays outside a Click Wait section.
	 * (Inside a Stop section, the first click shows the whole text if a typewriter is still revealing it,
	 * the next click advances.)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Advance")
	EADVClickOutsideWait ClickOutsideWait = EADVClickOutsideWait::JumpToNextWait;

	/**
	 * Input Action that turns auto mode on / off. Must be mapped to a key inside InputMappingContext.
	 * Leave empty to control auto mode from Blueprint only (Set Auto Mode / Toggle Auto Mode).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Auto")
	TObjectPtr<UInputAction> AutoAction;

	/**
	 * Auto mode without a voice: seconds to wait once the wait is reached and the text is fully shown.
	 * Total = AutoBaseDelay + AutoDelayPerChar × characters of the line.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Auto", meta=(ClampMin="0.0", UIMin="0.0"))
	float AutoBaseDelay = 1.0f;

	/** Auto mode without a voice: extra seconds per character of the line (Sequencer Subtitles text). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Auto", meta=(ClampMin="0.0", UIMin="0.0"))
	float AutoDelayPerChar = 0.05f;

	/** Auto mode with a voice: seconds to wait after the voice has finished. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Auto", meta=(ClampMin="0.0", UIMin="0.0"))
	float AutoDelayAfterVoice = 0.5f;

	/**
	 * Audio sections whose sound uses one of these Sound Classes are treated as voices.
	 * (An audio section counts as a voice if it matches this OR VoiceAssetKeywords.)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Auto")
	TArray<TObjectPtr<USoundClass>> VoiceSoundClasses;

	/**
	 * Audio sections whose sound asset path contains one of these words (case-insensitive) are treated as voices.
	 * e.g. "Voice" matches /Game/Voice/Ch01/VO_001. Clear the list to use VoiceSoundClasses only.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Auto")
	TArray<FString> VoiceAssetKeywords = { TEXT("Voice"), TEXT("VO_") };

	/** Seconds the player must hold SkipAction before the skip triggers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skip", meta=(ClampMin="0.1", UIMin="0.1"))
	float HoldDuration = 1.0f;

	/** Duration of the fade-to-black after hold completes (seconds). 0 = instant cut. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skip", meta=(ClampMin="0.0", UIMin="0.0"))
	float FadeOutDuration = 0.5f;

	/** Fade back in from black after the skipped sequence has stopped. Turn off if your game handles the fade itself. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skip")
	bool bFadeInAfterSkip = true;

	/** Duration of the fade back in after a skip (seconds). 0 = instant. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skip", meta=(EditCondition="bFadeInAfterSkip", ClampMin="0.0", UIMin="0.0"))
	float FadeInDuration = 0.5f;

	/** Size of the circular gauge widget in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skip|Gauge", meta=(ClampMin="32", UIMin="32"))
	float GaugeSize = 80.0f;

	/** Fill color of the progress arc. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skip|Gauge")
	FLinearColor GaugeColor = FLinearColor(1.f, 0.8f, 0.f, 1.f);

	/** Background ring color. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skip|Gauge")
	FLinearColor GaugeBackgroundColor = FLinearColor(0.f, 0.f, 0.f, 0.55f);
};
