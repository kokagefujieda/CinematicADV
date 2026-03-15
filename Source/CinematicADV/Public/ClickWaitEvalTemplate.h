// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Misc/FrameTime.h"
#include "ClickWaitSection.h"
#include "ClickWaitEvalTemplate.generated.h"

/**
 * Evaluation template for UClickWaitTrack.
 * Notifies UADVSubsystem to start monitoring this section for loop/stop behavior.
 */
USTRUCT()
struct CINEMATICADV_API FClickWaitEvalTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	FClickWaitEvalTemplate() = default;
	explicit FClickWaitEvalTemplate(const UClickWaitSection& InSection);

	UPROPERTY()
	EClickWaitMode Mode = EClickWaitMode::Loop;

	/** Section start in display rate frames. Used to jump back on loop. */
	UPROPERTY()
	FFrameTime SectionStart;

	/** Section end in display rate frames. Trigger point for loop/stop. */
	UPROPERTY()
	FFrameTime SectionEnd;

	/** Display rate of the owning MovieScene. Stored for subsystem use. */
	UPROPERTY()
	FFrameRate DisplayRate;

private:
	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context,
		const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
};
