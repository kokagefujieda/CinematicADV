// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CinematicADVConfig.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * CinematicADV configuration DataAsset.
 *
 * How to use:
 *   1. Create this DataAsset anywhere in your Content folder
 *      (right-click → Miscellaneous → Data Asset → CinematicADVConfig).
 *   2. Set InputMappingContext to the IMC that maps your advance key (e.g. Left Click, Enter).
 *   3. Set AdvanceAction to the Input Action bound in that IMC.
 *   Done. No Blueprint or Project Settings required.
 */
UCLASS(BlueprintType)
class CINEMATICADV_API UCinematicADVConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Input Mapping Context that contains the AdvanceAction mapping.
	 * The plugin will add this IMC automatically at priority 90.
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

	/** Seconds the player must hold SkipAction before the skip triggers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skip", meta=(ClampMin="0.1", UIMin="0.1"))
	float HoldDuration = 1.0f;

	/** Duration of the fade-to-black after hold completes (seconds). 0 = instant cut. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skip", meta=(ClampMin="0.0", UIMin="0.0"))
	float FadeOutDuration = 0.5f;

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
