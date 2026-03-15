// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneSection.h"
#include "ClickWaitSection.generated.h"

UENUM(BlueprintType)
enum class EClickWaitMode : uint8
{
	/** Playback pauses when the playhead reaches the section end. Click → jump past section. */
	Stop  UMETA(DisplayName = "Stop"),

	/** Playback loops between section start and end until clicked. Click → jump past section. */
	Loop  UMETA(DisplayName = "Loop"),
};

/**
 * A section that controls Sequencer playback flow.
 * Stop: pause at section end, click to continue.
 * Loop: loop the section range until clicked, then continue.
 */
UCLASS(meta=(DisplayName="Click Wait Section"))
class CINEMATICADV_API UClickWaitSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	UClickWaitSection();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ClickWait")
	EClickWaitMode Mode = EClickWaitMode::Loop;
};
