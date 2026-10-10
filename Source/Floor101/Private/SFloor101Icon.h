#pragma once

#include "CoreMinimal.h"
#include "F101/Cad/EditController.h"
#include "Widgets/SLeafWidget.h"

// Original line icons on a 20-unit grid, drawn by Slate with 1.5-unit strokes (no font or image files). The tools use
// the conventions of a floor plan: a door with its swing, a window's glazing, a hatched wall, a dimension with 45° ticks.
enum class EFloor101Icon : uint8
{
	Mark, Select, Wall, Door, Window, Column, Services, Furniture, Paint, Dimension,
	Undo, Redo, Save, Options, Exit, Design, Walk, Photo, Frame, Top, Plan, Xray, Eye, Levels,
	ChevronDown, ChevronRight, Up, Down, Plus, Search, Star, StarFilled, Close, Check, Warn, Info, Trash, Doc, Folder, Export,
	Grid, Flip, Monitor, Sliders, Keys, Pulse, Rotate, Model
};

class SFloor101Icon : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SFloor101Icon) : _Icon(EFloor101Icon::Select), _Size(20.0f), _Color(FSlateColor::UseForeground()) {}
		SLATE_ATTRIBUTE(EFloor101Icon, Icon)
		SLATE_ARGUMENT(float, Size)
		SLATE_ATTRIBUTE(FSlateColor, Color)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& Elements,
		int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

	static EFloor101Icon ForMode(f101::cad::EditMode Mode);

private:
	TAttribute<EFloor101Icon> Icon;
	TAttribute<FSlateColor> Color;
	float Size = 20.0f;
};
