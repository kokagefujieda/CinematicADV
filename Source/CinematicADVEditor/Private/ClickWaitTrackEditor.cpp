// Copyright 2026 kokage. All Rights Reserved.

#include "ClickWaitTrackEditor.h"
#include "ClickWaitTrack.h"
#include "ClickWaitSection.h"

#include "ISequencer.h"
#include "SequencerSectionPainter.h"
#include "MovieScene.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#if ENGINE_MINOR_VERSION >= 7
#include "MVVM/Views/ViewUtilities.h"
#else
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#endif

#define LOCTEXT_NAMESPACE "ClickWaitTrackEditor"

// ---------------------------------------------------------------------------
// FClickWaitSectionUI
// ---------------------------------------------------------------------------

FClickWaitSectionUI::FClickWaitSectionUI(UMovieSceneSection& InSection)
	: FSequencerSection(InSection)
{
}

int32 FClickWaitSectionUI::OnPaintSection(FSequencerSectionPainter& Painter) const
{
	const UClickWaitSection* Section = Cast<UClickWaitSection>(
		const_cast<FClickWaitSectionUI*>(this)->GetSectionObject());

	// Loop = blue-ish, Stop = orange-ish
	const FLinearColor LoopColor(0.1f, 0.4f, 0.9f, 1.0f);
	const FLinearColor StopColor(0.9f, 0.45f, 0.1f, 1.0f);
	const FLinearColor BarColor = (Section && Section->Mode == EClickWaitMode::Stop) ? StopColor : LoopColor;

	return Painter.PaintSectionBackground(BarColor);
}

FText FClickWaitSectionUI::GetSectionTitle() const
{
	const UClickWaitSection* Section = Cast<UClickWaitSection>(
		const_cast<FClickWaitSectionUI*>(this)->GetSectionObject());
	if (!Section)
	{
		return LOCTEXT("NoSection", "Click Wait");
	}
	return Section->Mode == EClickWaitMode::Loop
		? LOCTEXT("LoopMode", "Loop")
		: LOCTEXT("StopMode", "Stop");
}

#if ENGINE_MINOR_VERSION >= 7
float FClickWaitSectionUI::GetSectionHeight(const UE::Sequencer::FViewDensityInfo& ViewDensity) const
#else
float FClickWaitSectionUI::GetSectionHeight() const
#endif
{
	return 24.0f;
}

// ---------------------------------------------------------------------------
// FClickWaitTrackEditor
// ---------------------------------------------------------------------------

TSharedRef<ISequencerTrackEditor> FClickWaitTrackEditor::CreateTrackEditor(
	TSharedRef<ISequencer> OwningSequencer)
{
	return MakeShareable(new FClickWaitTrackEditor(OwningSequencer));
}

FClickWaitTrackEditor::FClickWaitTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer)
{
}

FText FClickWaitTrackEditor::GetDisplayName() const
{
	return LOCTEXT("TrackDisplayName", "Click Wait Track");
}

bool FClickWaitTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const
{
	return TrackClass == UClickWaitTrack::StaticClass();
}

TSharedRef<ISequencerSection> FClickWaitTrackEditor::MakeSectionInterface(
	UMovieSceneSection& SectionObject,
	UMovieSceneTrack& Track,
	FGuid ObjectBinding)
{
	return MakeShared<FClickWaitSectionUI>(SectionObject);
}

void FClickWaitTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddClickWaitTrack", "Click Wait Track"),
		LOCTEXT("AddClickWaitTrackTooltip",
			"Add a Click Wait track. Sections define Loop or Stop regions that pause until the player clicks."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.CameraActor"),
		FUIAction(FExecuteAction::CreateSP(this, &FClickWaitTrackEditor::HandleAddClickWaitTrack))
	);
}

TSharedPtr<SWidget> FClickWaitTrackEditor::BuildOutlinerEditWidget(
	const FGuid& ObjectBinding,
	UMovieSceneTrack* Track,
	const FBuildEditWidgetParams& Params)
{
	if (!Cast<UClickWaitTrack>(Track))
	{
		return TSharedPtr<SWidget>();
	}

	TWeakObjectPtr<UMovieSceneTrack>       WeakTrack  = Track;
	TWeakPtr<FClickWaitTrackEditor> WeakEditor =
		StaticCastSharedRef<FClickWaitTrackEditor>(AsShared());

#if ENGINE_MINOR_VERSION >= 7
	return UE::Sequencer::MakeAddButton(
		LOCTEXT("AddWaitSection", "Wait"),
		FOnClicked::CreateLambda([WeakEditor, WeakTrack]() -> FReply
		{
			if (TSharedPtr<FClickWaitTrackEditor> Ed = WeakEditor.Pin())
			{
				if (UMovieSceneTrack* T = WeakTrack.Get()) { Ed->AddNewSectionToTrack(T); }
			}
			return FReply::Handled();
		}),
		Params.ViewModel);
#else
	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "FlatButton")
		.ContentPadding(FMargin(2, 0))
		.OnClicked_Lambda([WeakEditor, WeakTrack]() -> FReply
		{
			if (TSharedPtr<FClickWaitTrackEditor> Ed = WeakEditor.Pin())
			{
				if (UMovieSceneTrack* T = WeakTrack.Get()) { Ed->AddNewSectionToTrack(T); }
			}
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.TextStyle(FAppStyle::Get(), "NormalText.Important")
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			.Text(LOCTEXT("AddWaitSection", "Wait"))
		];
#endif
}

void FClickWaitTrackEditor::AddNewSectionToTrack(UMovieSceneTrack* Track)
{
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (!SequencerPtr.IsValid()) { return; }

	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	if (!FocusedMovieScene || FocusedMovieScene->IsReadOnly()) { return; }

	const FScopedTransaction Transaction(LOCTEXT("AddClickWaitSection_Transaction", "Add Click Wait Section"));
	FocusedMovieScene->Modify();
	Track->Modify();

	UMovieSceneSection* NewSection = Track->CreateNewSection();
	if (!NewSection) { return; }

	const FFrameNumber CurrentTime = SequencerPtr->GetLocalTime().Time.GetFrame();
	const FFrameRate   TickRes     = FocusedMovieScene->GetTickResolution();
	const FFrameNumber Duration    = (3.0 * TickRes).FloorToFrame();
	NewSection->SetRange(TRange<FFrameNumber>(CurrentTime, CurrentTime + Duration));
	NewSection->SetOverlapPriority(0);

	Track->AddSection(*NewSection);

	SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	SequencerPtr->EmptySelection();
	SequencerPtr->SelectSection(NewSection);
	SequencerPtr->ThrobSectionSelection();
}

void FClickWaitTrackEditor::HandleAddClickWaitTrack()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	if (!FocusedMovieScene || FocusedMovieScene->IsReadOnly()) { return; }

	const FScopedTransaction Transaction(LOCTEXT("AddClickWaitTrack_Transaction", "Add Click Wait Track"));
	FocusedMovieScene->Modify();

	UClickWaitTrack* NewTrack = FocusedMovieScene->AddTrack<UClickWaitTrack>();
	if (NewTrack)
	{
		UMovieSceneSection* NewSection = NewTrack->CreateNewSection();
		if (NewSection)
		{
			NewTrack->AddSection(*NewSection);
			if (FocusedMovieScene->GetPlaybackRange().HasLowerBound())
			{
				const FFrameNumber Start   = FocusedMovieScene->GetPlaybackRange().GetLowerBoundValue();
				const FFrameRate   TickRes = FocusedMovieScene->GetTickResolution();
				const FFrameNumber Dur     = (3.0 * TickRes).FloorToFrame();
				NewSection->SetRange(TRange<FFrameNumber>(Start, Start + Dur));
			}
		}
		if (TSharedPtr<ISequencer> Seq = GetSequencer())
		{
			Seq->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
		}
	}
}

#undef LOCTEXT_NAMESPACE
