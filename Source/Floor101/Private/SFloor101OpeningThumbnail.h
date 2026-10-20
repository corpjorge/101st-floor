#pragma once

#include "F101/Cad/OpeningCatalogue.h"
#include "F101/Geometry/CarpentryBuilder.h"
#include "F101/Geometry/WallJoins.h"
#include "Floor101Style.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/SLeafWidget.h"

// Draw the real family mesh, closed, from the front. No generated bitmap assets or duplicate design definitions.
class SFloor101OpeningThumbnail : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SFloor101OpeningThumbnail) {}
		SLATE_ARGUMENT(std::string, Family)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args)
	{
		const auto* Family = f101::cad::findOpeningFamily(Args._Family);
		if (!Family) { return; }
		f101::cad::Opening Opening;
		Opening.kind = Family->kind;
		Opening.widthMm = Family->widthMm;
		Opening.heightMm = Family->heightMm;
		(void)f101::cad::applyOpeningFamily(Opening, Family->id);
		f101::cad::Wall Wall;
		Wall.end = {Family->widthMm, 0};
		Wall.thicknessMm = 150;
		Wall.heightMm = Family->heightMm + 100;
		const auto Mesh = f101::geometry::buildOpeningMesh(Wall, f101::geometry::plainFootprint(Wall), Opening);
		const auto& P = Floor101::Studio::Palette();
		for (const auto& Section : Mesh.sections)
		{
			const FLinearColor Base = Section.slot == "glass" ? P.Info : Section.slot == "hardware" ? P.Text3 : P.OnPaper;
			for (std::size_t i = 0; i + 2 < Section.indices.size(); i += 3)
			{
				FFace Face;
				Face.Depth = 0.0;
				const auto& Normal = Mesh.normals[Section.indices[i]];
				if (-0.4 * Normal.x + Normal.y + 0.15 * Normal.z >= 0.0) { continue; }
				const float Shade = Normal.z > 0.5 ? 1.0f : Normal.y < -0.5 ? 0.9f : 0.7f;
				Face.Color = (Base * Shade).ToFColorSRGB();
				for (std::size_t j = 0; j < 3; ++j)
				{
					const auto& V = Mesh.positions[Section.indices[i + j]];
					const FVector2D Point(V.x + 0.4 * V.y, -V.z + 0.15 * V.y);
					Face.Points.Add(Point);
					Bounds += Point;
					Face.Depth += V.y - 0.4 * V.x + 0.15 * V.z;
				}
				Faces.Add(MoveTemp(Face));
			}
		}
		Faces.Sort([](const FFace& A, const FFace& B) { return A.Depth > B.Depth; });
	}
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Floor101::Studio::Size::OpeningPicture); }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out,
		int32 Layer, const FWidgetStyle&, bool) const override
	{
		if (!Bounds.bIsValid) { return Layer; }
		const FVector2D Size = Geometry.GetLocalSize();
		const FVector2D Extent = Bounds.GetSize();
		const double Margin = Floor101::Studio::Size::Gutter;
		const double Scale = FMath::Max(0.0, FMath::Min((Size.X - Margin) / FMath::Max(1.0, Extent.X), (Size.Y - Margin) / FMath::Max(1.0, Extent.Y)));
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
			for (int32 Index : {0, 1, 2}) { Indices.Add(static_cast<SlateIndex>(Start + Index)); }
		}
		const auto Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Floor101::Studio::White());
		FSlateDrawElement::MakeCustomVerts(Out, Layer, Resource, Vertices, Indices, nullptr, 0, 0);
		return Layer;
	}
private:
	struct FFace { TArray<FVector2D> Points; double Depth; FColor Color; };
	TArray<FFace> Faces;
	FBox2D Bounds{ForceInit};
};
