// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Brushes/SlateColorBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "ADVSubsystem.h"

class SScrollBox;

/**
 * Built-in backlog screen (Slate, no assets required).
 * Lines oldest first, opened scrolled to the latest. A button replays the voice of a line.
 */
class SADVBacklogWidget : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnPlayVoice, int32 /*EntryIndex*/);

	SLATE_BEGIN_ARGS(SADVBacklogWidget)
		: _BackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.85f))
		, _TextColor(FLinearColor::White)
		, _SpeakerColor(FLinearColor(1.f, 0.85f, 0.5f, 1.f))
	{}
		SLATE_ARGUMENT(TArray<FADVBacklogEntry>, Entries)
		SLATE_ARGUMENT(FSlateFontInfo, Font)
		SLATE_ARGUMENT(FLinearColor, BackgroundColor)
		SLATE_ARGUMENT(FLinearColor, TextColor)
		SLATE_ARGUMENT(FLinearColor, SpeakerColor)
		SLATE_EVENT(FOnPlayVoice, OnPlayVoice)
	SLATE_END_ARGS()

	SADVBacklogWidget();

	void Construct(const FArguments& InArgs);

	/** The wheel never reaches the game while the backlog is open (a wheel key mapped to BacklogAction would close it). */
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	FReply HandlePlayClicked(int32 EntryIndex);

	FOnPlayVoice           OnPlayVoice;
	FSlateColorBrush       BackgroundBrush;
	TSharedPtr<SScrollBox> ScrollBox;
};
