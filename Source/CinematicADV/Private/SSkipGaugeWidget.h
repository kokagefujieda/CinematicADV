// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

/**
 * Circular hold-to-skip gauge drawn entirely in Slate (no assets required).
 * Shows a ring that fills clockwise as the player holds the skip button.
 */
class SSkipGaugeWidget : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSkipGaugeWidget)
		: _Progress(0.0f)
		, _GaugeColor(FLinearColor(1.f, 0.8f, 0.f, 1.f))
		, _BackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.55f))
		, _WidgetSize(80.0f)
	{}
	SLATE_ATTRIBUTE(float,        Progress)
	SLATE_ATTRIBUTE(FLinearColor, GaugeColor)
	SLATE_ATTRIBUTE(FLinearColor, BackgroundColor)
	SLATE_ATTRIBUTE(float,        WidgetSize)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	/** Draw a circular arc from StartT to EndT (0.0–1.0 = full circle, top = 0). */
	void DrawArc(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& AllottedGeometry,
		const FVector2D& Center,
		float Radius,
		float StartT,
		float EndT,
		float Thickness,
		const FLinearColor& Color) const;

	TAttribute<float>        Progress;
	TAttribute<FLinearColor> GaugeColor;
	TAttribute<FLinearColor> BackgroundColor;
	TAttribute<float>        WidgetSize;
};
