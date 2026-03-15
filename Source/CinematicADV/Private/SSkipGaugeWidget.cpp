// Copyright 2026 kokage. All Rights Reserved.

#include "SSkipGaugeWidget.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void SSkipGaugeWidget::Construct(const FArguments& InArgs)
{
	Progress        = InArgs._Progress;
	GaugeColor      = InArgs._GaugeColor;
	BackgroundColor = InArgs._BackgroundColor;
	WidgetSize      = InArgs._WidgetSize;
}

int32 SSkipGaugeWidget::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const float     S       = AllottedGeometry.GetLocalSize().X;
	const FVector2D Center  = FVector2D(S * 0.5f, S * 0.5f);
	const float     Radius  = S * 0.40f;
	const float     Track   = S * 0.09f;
	const float     Prog    = FMath::Clamp(Progress.Get(), 0.0f, 1.0f);

	// Background ring (full circle)
	DrawArc(OutDrawElements, LayerId,
		AllottedGeometry, Center, Radius,
		0.0f, 1.0f, Track, BackgroundColor.Get());

	// Progress arc (clockwise from top)
	if (Prog > 0.005f)
	{
		DrawArc(OutDrawElements, LayerId + 1,
			AllottedGeometry, Center, Radius,
			0.0f, Prog, Track, GaugeColor.Get());
	}

	// Center label "SKIP"
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(S * 0.16f));
	const FText Label = FText::FromString(TEXT("SKIP"));

	const TSharedRef<FSlateFontMeasure> FontMeasure =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D TextSize = FontMeasure->Measure(Label, Font);
	const FVector2D TextPos  = Center - TextSize * 0.5f;

	FSlateDrawElement::MakeText(
		OutDrawElements, LayerId + 2,
		AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(TextPos)),
		Label, Font,
		ESlateDrawEffect::None,
		FLinearColor::White);

	return LayerId + 2;
}

FVector2D SSkipGaugeWidget::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	const float S = WidgetSize.Get();
	return FVector2D(S, S);
}

void SSkipGaugeWidget::DrawArc(
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FGeometry& AllottedGeometry,
	const FVector2D& Center,
	float Radius,
	float StartT,
	float EndT,
	float Thickness,
	const FLinearColor& Color) const
{
	const int32 TotalSegments = 64;
	const int32 DrawSegments  = FMath::Max(2, FMath::RoundToInt(TotalSegments * (EndT - StartT)));

	TArray<FVector2D> Points;
	Points.Reserve(DrawSegments + 1);

	for (int32 i = 0; i <= DrawSegments; i++)
	{
		const float T     = FMath::Lerp(StartT, EndT, (float)i / DrawSegments);
		const float Angle = -PI * 0.5f + T * 2.0f * PI;
		Points.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
	}

	FSlateDrawElement::MakeLines(
		OutDrawElements, LayerId,
		AllottedGeometry.ToPaintGeometry(),
		Points,
		ESlateDrawEffect::None,
		Color,
		/*bAntialias*/ true,
		Thickness);
}
