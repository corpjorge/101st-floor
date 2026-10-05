#pragma once

#include "F101/Cad/EditController.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

// Small original drafting symbols, rendered locally by Slate with the button's foreground.
class SFloor101ToolIcon : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SFloor101ToolIcon) {} SLATE_ARGUMENT(f101::cad::EditMode, Mode) SLATE_END_ARGS()
	void Construct(const FArguments& Args) { Mode = Args._Mode; }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(18.0, 18.0); }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Elements,
		int32 Layer, const FWidgetStyle& Style, bool) const override
	{
		using f101::cad::EditMode;
		auto Line = [&](std::initializer_list<FVector2f> Points)
		{
			TArray<FVector2f> Path;
			for (const auto& Point : Points) { Path.Add(Point); }
			FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), Path, ESlateDrawEffect::None,
				Style.GetForegroundColor(), true, 1.3f);
		};
		switch (Mode)
		{
		case EditMode::Dimension:
			Line({{3,3},{3,16}}); Line({{15,3},{15,16}}); Line({{1,10},{17,10}}); Line({{1,12},{5,8}}); Line({{13,12},{17,8}}); break;
		case EditMode::Wall:
			Line({{2,3},{16,3},{16,15},{2,15},{2,3}}); Line({{2,7},{16,7}}); Line({{2,11},{16,11}}); Line({{7,3},{7,7}}); Line({{11,7},{11,11}}); Line({{7,11},{7,15}}); break;
		case EditMode::Door:
			Line({{3,16},{3,2},{14,2},{14,16}}); Line({{5,16},{5,4},{12,7},{12,16},{5,16}}); Line({{10,11},{10,12}}); break;
		case EditMode::Window:
			Line({{2,2},{16,2},{16,16},{2,16},{2,2}}); Line({{9,2},{9,16}}); Line({{2,9},{16,9}}); break;
		case EditMode::Column:
			Line({{2,2},{16,2}}); Line({{2,16},{16,16}}); Line({{4,2},{4,16}}); Line({{9,2},{9,16}}); Line({{14,2},{14,16}}); break;
		case EditMode::Services:
			Line({{2,14},{7,14},{7,5},{14,5},{14,2}}); Line({{1,12},{3,12},{3,16},{1,16},{1,12}}); Line({{12,1},{16,1}}); break;
		case EditMode::Furniture:
			Line({{2,8},{2,14},{16,14},{16,8},{13,8},{13,11},{5,11},{5,8},{2,8}}); Line({{5,8},{5,4},{13,4},{13,8}}); Line({{4,14},{4,17}}); Line({{14,14},{14,17}}); break;
		case EditMode::Paint:
			Line({{2,5},{9,1},{16,5},{9,9},{2,5}}); Line({{2,9},{9,13},{16,9}}); Line({{2,13},{9,17},{16,13}}); break;
		default:
			Line({{3,2},{3,15},{7,11},{10,16},{12,15},{9,10},{15,10},{3,2}}); break;
		}
		return Layer;
	}
private:
	f101::cad::EditMode Mode = f101::cad::EditMode::Select;
};
