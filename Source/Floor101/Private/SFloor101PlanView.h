#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

namespace f101::drawing
{
	struct PlanDrawing;
}

// Plan millimetres to the widget's local units: (Origin.X + x * Scale, Origin.Y - y * Scale), north up.
struct FFloor101PlanMapping
{
	FVector2D Origin = FVector2D::ZeroVector;
	double Scale = 0.1; // local units per mm
};

// Paints a plan drawing (ADR-0015) on white paper: fills, strokes (hidden ones dashed) and labels, in the drawing's
// inks. The 2D view maps it through the camera; without a mapping (exports) the drawing is fitted with a margin.
class SFloor101PlanView : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SFloor101PlanView) : _MarginMm(1000.0) {}
		// The drawing to paint, read at every paint; null paints blank paper.
		SLATE_ARGUMENT(TFunction<const f101::drawing::PlanDrawing*()>, Drawing)
		// Where the drawing goes; unset (or returning unset) fits it.
		SLATE_ARGUMENT(TFunction<TOptional<FFloor101PlanMapping>(const FGeometry&)>, Mapping)
		SLATE_ARGUMENT(double, MarginMm)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// The drawing's bounds plus a margin, centred in a widget of that size.
	static FFloor101PlanMapping Fit(const f101::drawing::PlanDrawing& Drawing, const FVector2D& Size, double MarginMm);

private:
	TFunction<const f101::drawing::PlanDrawing*()> Drawing;
	TFunction<TOptional<FFloor101PlanMapping>(const FGeometry&)> Mapping;
	double MarginMm = 1000.0;
};
