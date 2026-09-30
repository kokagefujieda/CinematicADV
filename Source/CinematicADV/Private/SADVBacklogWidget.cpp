// Copyright 2026 kokage. All Rights Reserved.

#include "SADVBacklogWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CinematicADV"

SADVBacklogWidget::SADVBacklogWidget()
	: BackgroundBrush(FLinearColor::White)
{
}

void SADVBacklogWidget::Construct(const FArguments& InArgs)
{
	OnPlayVoice = InArgs._OnPlayVoice;

	const FSlateFontInfo& TextFont = InArgs._Font;
	FSlateFontInfo SpeakerFont = TextFont;
	SpeakerFont.Size = FMath::Max(1.0f, FMath::RoundToFloat(TextFont.Size * 0.8f));

	// Voice buttons are kept in their own column so the text lines up
	const float ButtonSize = FMath::Max(24.0f, TextFont.Size * 1.6f);

	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	for (int32 Index = 0; Index < InArgs._Entries.Num(); ++Index)
	{
		const FADVBacklogEntry& Entry = InArgs._Entries[Index];

		TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
		if (!Entry.SpeakerName.IsEmptyOrWhitespace())
		{
			Lines->AddSlot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 4.f)
			[
				SNew(STextBlock)
				.Text(Entry.SpeakerName)
				.Font(SpeakerFont)
				.ColorAndOpacity(FSlateColor(InArgs._SpeakerColor))
			];
		}
		Lines->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(Entry.Text)
			.Font(TextFont)
			.ColorAndOpacity(FSlateColor(InArgs._TextColor))
			.AutoWrapText(true)
		];

		TSharedRef<SWidget> VoiceButton = SNullWidget::NullWidget;
		if (Entry.Voice)
		{
			VoiceButton =
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ToolTipText(LOCTEXT("BacklogPlayVoiceTip", "Play voice"))
				.OnClicked(FOnClicked::CreateSP(this, &SADVBacklogWidget::HandlePlayClicked, Index))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BacklogPlayVoice", "▶"))
					.Font(SpeakerFont)
				];
		}

		List->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 28.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Top)
			.Padding(0.f, 0.f, 16.f, 0.f)
			[
				SNew(SBox)
				.WidthOverride(ButtonSize)
				.HeightOverride(ButtonSize)
				[
					VoiceButton
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				Lines
			]
		];
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(&BackgroundBrush)
		.BorderBackgroundColor(InArgs._BackgroundColor)
		.Padding(FMargin(0.f, 60.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			+ SHorizontalBox::Slot()
			.FillWidth(6.f)
			[
				SAssignNew(ScrollBox, SScrollBox)
				+ SScrollBox::Slot()
				[
					List
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
		]
	];

	// Open at the latest line
	ScrollBox->ScrollToEnd();
}

FReply SADVBacklogWidget::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled();
}

FReply SADVBacklogWidget::HandlePlayClicked(int32 EntryIndex)
{
	OnPlayVoice.ExecuteIfBound(EntryIndex);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
