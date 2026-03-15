// Copyright 2026 kokage. All Rights Reserved.

#include "ClickWaitEvalTemplate.h"
#include "ClickWaitSection.h"
#include "ADVSubsystem.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneExecutionToken.h"
#include "MovieScene.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

/** Execution token: notifies UADVSubsystem on the game thread when a section is being evaluated. */
struct FClickWaitExecutionToken : IMovieSceneExecutionToken
{
	EClickWaitMode Mode;
	FFrameTime     SectionStart;
	FFrameTime     SectionEnd;
	FFrameRate     DisplayRate;

	FClickWaitExecutionToken(EClickWaitMode InMode, FFrameTime InStart, FFrameTime InEnd, FFrameRate InRate)
		: Mode(InMode), SectionStart(InStart), SectionEnd(InEnd), DisplayRate(InRate)
	{
	}

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand,
		FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		UObject* PlaybackContext = Player.GetPlaybackContext();
		if (!PlaybackContext) { return; }

		UWorld* World = PlaybackContext->GetWorld();
		if (!World) { return; }

		UGameInstance* GI = World->GetGameInstance();
		if (!GI) { return; }

		UADVSubsystem* Subsystem = GI->GetSubsystem<UADVSubsystem>();
		if (!Subsystem) { return; }

		Subsystem->OnSectionEntered(Mode, SectionStart, SectionEnd, DisplayRate);
	}
};

FClickWaitEvalTemplate::FClickWaitEvalTemplate(const UClickWaitSection& InSection)
{
	Mode = InSection.Mode;

	// Get tick and display rates from the owning MovieScene
	const UMovieScene* MovieScene = InSection.GetTypedOuter<UMovieScene>();
	const FFrameRate TickRate    = MovieScene ? MovieScene->GetTickResolution() : FFrameRate(24000, 1);
	DisplayRate                  = MovieScene ? MovieScene->GetDisplayRate()    : FFrameRate(30, 1);

	// Convert section range from tick resolution to display rate
	const TRange<FFrameNumber> Range = InSection.GetRange();

	const FFrameNumber StartTick = Range.HasLowerBound() ? Range.GetLowerBoundValue() : FFrameNumber(0);
	const FFrameNumber EndTick   = Range.HasUpperBound() ? Range.GetUpperBoundValue() : FFrameNumber(0);

	SectionStart = FFrameRate::TransformTime(FFrameTime(StartTick), TickRate, DisplayRate);
	SectionEnd   = FFrameRate::TransformTime(FFrameTime(EndTick),   TickRate, DisplayRate);
}

void FClickWaitEvalTemplate::Evaluate(
	const FMovieSceneEvaluationOperand& Operand,
	const FMovieSceneContext& Context,
	const FPersistentEvaluationData& PersistentData,
	FMovieSceneExecutionTokens& ExecutionTokens) const
{
	ExecutionTokens.Add(FClickWaitExecutionToken(Mode, SectionStart, SectionEnd, DisplayRate));
}
