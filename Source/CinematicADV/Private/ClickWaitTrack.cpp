// Copyright 2026 kokage. All Rights Reserved.

#include "ClickWaitTrack.h"
#include "ClickWaitSection.h"
#include "ClickWaitEvalTemplate.h"

bool UClickWaitTrack::IsEmpty() const
{
	return Sections.IsEmpty();
}

bool UClickWaitTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UClickWaitSection::StaticClass();
}

UMovieSceneSection* UClickWaitTrack::CreateNewSection()
{
	return NewObject<UClickWaitSection>(this, NAME_None, RF_Transactional);
}

const TArray<UMovieSceneSection*>& UClickWaitTrack::GetAllSections() const
{
	return reinterpret_cast<const TArray<UMovieSceneSection*>&>(Sections);
}

bool UClickWaitTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}

void UClickWaitTrack::AddSection(UMovieSceneSection& Section)
{
	Sections.Add(&Section);
}

void UClickWaitTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}

void UClickWaitTrack::RemoveSectionAt(int32 SectionIndex)
{
	Sections.RemoveAt(SectionIndex);
}

void UClickWaitTrack::RemoveAllAnimationData()
{
	Sections.Empty();
}

FMovieSceneEvalTemplatePtr UClickWaitTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	const UClickWaitSection* WaitSection = Cast<UClickWaitSection>(&InSection);
	if (WaitSection)
	{
		return FClickWaitEvalTemplate(*WaitSection);
	}
	return FMovieSceneEvalTemplatePtr();
}

#if WITH_EDITORONLY_DATA
FText UClickWaitTrack::GetDefaultDisplayName() const
{
	return NSLOCTEXT("CinematicADV", "ClickWaitTrackName", "Click Wait");
}
#endif
