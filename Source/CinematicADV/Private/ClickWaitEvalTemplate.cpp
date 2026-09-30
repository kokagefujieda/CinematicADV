// Copyright 2026 kokage. All Rights Reserved.

#include "ClickWaitEvalTemplate.h"
#include "ClickWaitSection.h"
#include "ADVSubsystem.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneExecutionToken.h"
#include "MovieSceneSequencePlayer.h"
#include "MovieScene.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

/** Execution token: tells UADVSubsystem (game thread) that the player is inside a wait section. */
struct FClickWaitExecutionToken : IMovieSceneExecutionToken
{
	EClickWaitMode Mode;
	uint32         SectionKey;
	/** Seconds in the evaluated (sub)sequence's own time. */
	double         LocalNow;
	double         LocalStart;
	double         LocalEnd;

	FClickWaitExecutionToken(EClickWaitMode InMode, uint32 InSectionKey, double InNow, double InStart, double InEnd)
		: Mode(InMode), SectionKey(InSectionKey), LocalNow(InNow), LocalStart(InStart), LocalEnd(InEnd)
	{
	}

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand,
		FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		// Only sequence players (game / PIE) can wait; the Sequencer editor preview has none
		UMovieSceneSequencePlayer* SequencePlayer = Cast<UMovieSceneSequencePlayer>(Player.AsUObject());
		if (!SequencePlayer) { return; }

		UObject* PlaybackContext = Player.GetPlaybackContext();
		UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;
		UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		UADVSubsystem* Subsystem = GI ? GI->GetSubsystem<UADVSubsystem>() : nullptr;
		if (!Subsystem) { return; }

		// The player that evaluates this section is the one to control (not "the first playing one")
		Subsystem->OnSectionEvaluated(SequencePlayer, SectionKey, Mode, LocalNow, LocalStart, LocalEnd);
	}
};

FClickWaitEvalTemplate::FClickWaitEvalTemplate(const UClickWaitSection& InSection)
{
	Mode = InSection.Mode;

	if (const UMovieScene* MovieScene = InSection.GetTypedOuter<UMovieScene>())
	{
		TickResolution = MovieScene->GetTickResolution();
	}

	const TRange<FFrameNumber> Range = InSection.GetRange();
	SectionStart = Range.HasLowerBound() ? Range.GetLowerBoundValue() : FFrameNumber(0);
	SectionEnd   = Range.HasUpperBound() ? Range.GetUpperBoundValue() : FFrameNumber(0);

	SectionKey = InSection.GetUniqueID();
}

void FClickWaitEvalTemplate::Evaluate(
	const FMovieSceneEvaluationOperand& Operand,
	const FMovieSceneContext& Context,
	const FPersistentEvaluationData& PersistentData,
	FMovieSceneExecutionTokens& ExecutionTokens) const
{
	const double Now   = TickResolution.AsSeconds(Context.GetTime());
	const double Start = TickResolution.AsSeconds(FFrameTime(SectionStart));
	const double End   = TickResolution.AsSeconds(FFrameTime(SectionEnd));

	ExecutionTokens.Add(FClickWaitExecutionToken(Mode, SectionKey, Now, Start, End));
}
