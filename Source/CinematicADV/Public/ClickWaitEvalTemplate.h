// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Misc/FrameRate.h"
#include "ClickWaitSection.h"
#include "ClickWaitEvalTemplate.generated.h"

/**
 * Evaluation template for UClickWaitTrack.
 * Tells UADVSubsystem which player is inside which wait section, in that sequence's own time
 * (the subsystem converts it to the player's time, so waits inside sub-sequences work too).
 */
USTRUCT()
struct CINEMATICADV_API FClickWaitEvalTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	FClickWaitEvalTemplate() = default;
	explicit FClickWaitEvalTemplate(const UClickWaitSection& InSection);

	UPROPERTY()
	EClickWaitMode Mode = EClickWaitMode::Loop;

	/** Section range in the tick resolution of the owning MovieScene. */
	UPROPERTY()
	FFrameNumber SectionStart;

	UPROPERTY()
	FFrameNumber SectionEnd;

	UPROPERTY()
	FFrameRate TickResolution = FFrameRate(24000, 1);

	/** Identifies the section while it is being waited on. */
	UPROPERTY()
	uint32 SectionKey = 0;

private:
	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context,
		const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
};
