#pragma once

#include "Floor101Style.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

// A local star avoids depending on symbol-font fallback on Windows.
class SFloor101FavoriteIcon : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SFloor101FavoriteIcon) {} SLATE_ATTRIBUTE(bool, Active) SLATE_END_ARGS()
	void Construct(const FArguments& Args) { Active = Args._Active; }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(18.0, 18.0); }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out,
		int32 Layer, const FWidgetStyle&, bool) const override
	{
		TArray<FVector2f> Points;
		for (int32 Index = 0; Index <= 10; ++Index)
		{
			const float Angle = -UE_PI / 2.0f + static_cast<float>(Index) * UE_PI / 5.0f;
			const float Radius = Index % 2 == 0 ? 7.5f : 3.4f;
			Points.Add(FVector2f(9.0f + Radius * FMath::Cos(Angle), 9.0f + Radius * FMath::Sin(Angle)));
		}
		FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None,
			Floor101::Studio::Color(Active.Get(false) ? TEXT("D6AD78") : TEXT("B3BAB9")), true, Active.Get(false) ? 2.0f : 1.0f);
		return Layer;
	}
private:
	TAttribute<bool> Active;
};
