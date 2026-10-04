#pragma once

#include "F101/Geometry/FurnitureBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

// Isometric thumbnails derived from the same parametric boxes used by the model.
class SFloor101FurnitureThumbnail : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SFloor101FurnitureThumbnail) {}
		SLATE_ARGUMENT(std::string, Type)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args)
	{
		const auto Furniture = f101::cad::makeFurniture(Args._Type, {});
		const auto Shape = f101::geometry::furnitureShape(Furniture);
		auto Project = [](double X, double Y, double Z) { return FVector2D((X + Y) * 0.7, (X - Y) * 0.35 - Z); };
		for (const auto& Box : Shape.boxes)
		{
			const auto* Finish = f101::cad::findFinish(f101::geometry::furnitureSlotFinish(Furniture, Box.slot));
			const FColor Base = Finish ? FColor(Finish->color.r, Finish->color.g, Finish->color.b) : FColor(180, 180, 175);
			auto Add = [&](TArray<FVector2D> Points, double Depth, float Shade)
			{
				for (const auto& Point : Points) { Bounds += Point; }
				Faces.Add({MoveTemp(Points), Depth, (FLinearColor::FromSRGBColor(Base) * Shade).ToFColorSRGB()});
			};
			const double Depth = (Box.x0 + Box.x1 - Box.y0 - Box.y1 + Box.z0 + Box.z1) * 0.5;
			Add({Project(Box.x1, Box.y0, Box.z0), Project(Box.x1, Box.y1, Box.z0), Project(Box.x1, Box.y1, Box.z1), Project(Box.x1, Box.y0, Box.z1)}, Depth, 0.65f);
			Add({Project(Box.x0, Box.y0, Box.z0), Project(Box.x1, Box.y0, Box.z0), Project(Box.x1, Box.y0, Box.z1), Project(Box.x0, Box.y0, Box.z1)}, Depth, 0.85f);
			Add({Project(Box.x0, Box.y0, Box.z1), Project(Box.x1, Box.y0, Box.z1), Project(Box.x1, Box.y1, Box.z1), Project(Box.x0, Box.y1, Box.z1)}, Depth, 1.0f);
		}
		Faces.Sort([](const FFace& A, const FFace& B) { return A.Depth < B.Depth; });
	}
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(84.0); }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out,
		int32 Layer, const FWidgetStyle&, bool) const override
	{
		if (!Bounds.bIsValid) { return Layer; }
		const FVector2D Size = Geometry.GetLocalSize();
		const FVector2D Extent = Bounds.GetSize();
		const double Scale = FMath::Min((Size.X - 12.0) / FMath::Max(1.0, Extent.X), (Size.Y - 12.0) / FMath::Max(1.0, Extent.Y));
		TArray<FSlateVertex> Vertices;
		TArray<SlateIndex> Indices;
		for (const auto& Face : Faces)
		{
			const SlateIndex Start = static_cast<SlateIndex>(Vertices.Num());
			for (const auto& Point : Face.Points)
			{
				const FVector2D Local = (Point - Bounds.GetCenter()) * Scale + Size * 0.5;
				Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(), FVector2f(Local), FVector2f(0.5f), Face.Color));
			}
			for (int32 Index : {0, 1, 2, 0, 2, 3}) { Indices.Add(static_cast<SlateIndex>(Start + Index)); }
		}
		const auto Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		FSlateDrawElement::MakeCustomVerts(Out, Layer, Resource, Vertices, Indices, nullptr, 0, 0);
		return Layer;
	}
private:
	struct FFace { TArray<FVector2D> Points; double Depth; FColor Color; };
	TArray<FFace> Faces;
	FBox2D Bounds{ForceInit};
};
