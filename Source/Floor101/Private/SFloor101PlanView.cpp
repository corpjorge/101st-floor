#include "SFloor101PlanView.h"

#include "F101/Drawing/PlanDrawing.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace
{
	using f101::drawing::Ink;

	constexpr double DashMm = 120.0; // same pattern as the SVG
	constexpr double GapMm = 80.0;
	constexpr double FontSizePerCapHeight = 1.0 / 0.96; // Slate size N draws capitals about 0.96 N units tall
	constexpr float MinFontSize = 4.0f;                  // smaller labels are left out

	FColor InkColor(Ink Value)
	{
		const f101::drawing::Rgb C = f101::drawing::inkColor(Value);
		return FColor(C.r, C.g, C.b, 255);
	}

	// The dashes of a polyline, in local units.
	TArray<TArray<FVector2D>> Dashes(const TArray<FVector2D>& Points, double Dash, double Gap)
	{
		TArray<TArray<FVector2D>> Result;
		double Phase = 0.0; // along the current dash (< Dash) or gap (>= Dash)
		for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
		{
			const FVector2D A = Points[Index];
			const FVector2D Segment = Points[Index + 1] - A;
			const double Length = Segment.Size();
			double Done = 0.0;
			while (Done < Length && Length > 0.0)
			{
				const bool bDrawing = Phase < Dash;
				const double Step = FMath::Min(Length - Done, bDrawing ? Dash - Phase : Dash + Gap - Phase);
				if (bDrawing)
				{
					Result.Add({A + Segment * (Done / Length), A + Segment * ((Done + Step) / Length)});
				}
				Done += Step;
				Phase = FMath::Fmod(Phase + Step, Dash + Gap);
			}
		}
		return Result;
	}
}

void SFloor101PlanView::Construct(const FArguments& InArgs)
{
	Drawing = InArgs._Drawing;
	Mapping = InArgs._Mapping;
	MarginMm = InArgs._MarginMm;
}

FFloor101PlanMapping SFloor101PlanView::Fit(const f101::drawing::PlanDrawing& Plan, const FVector2D& Size, double Margin)
{
	const double Width = Plan.max.x_mm - Plan.min.x_mm + 2.0 * Margin;
	const double Height = Plan.max.y_mm - Plan.min.y_mm + 2.0 * Margin;
	FFloor101PlanMapping Result;
	Result.Scale = FMath::Max(FMath::Min(Size.X / FMath::Max(Width, 1.0), Size.Y / FMath::Max(Height, 1.0)), 1e-6);
	Result.Origin.X = (Size.X - Width * Result.Scale) / 2.0 + (Margin - Plan.min.x_mm) * Result.Scale;
	Result.Origin.Y = (Size.Y - Height * Result.Scale) / 2.0 + (Margin + Plan.max.y_mm) * Result.Scale;
	return Result;
}

int32 SFloor101PlanView::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), White, ESlateDrawEffect::None, FLinearColor::White);
	const f101::drawing::PlanDrawing* Plan = Drawing ? Drawing() : nullptr;
	if (!Plan || Plan->empty())
	{
		return LayerId;
	}
	const TOptional<FFloor101PlanMapping> Mapped = Mapping ? Mapping(AllottedGeometry) : TOptional<FFloor101PlanMapping>();
	const FFloor101PlanMapping Map = Mapped.IsSet() ? Mapped.GetValue() : Fit(*Plan, AllottedGeometry.GetLocalSize(), MarginMm);
	auto ToLocal = [&Map](const f101::cad::PlanPoint& P)
	{
		return FVector2D(Map.Origin.X + P.x_mm * Map.Scale, Map.Origin.Y - P.y_mm * Map.Scale);
	};

	// Fills: their triangles as custom vertices over the white brush.
	const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*White);
	const FSlateRenderTransform& Transform = AllottedGeometry.GetAccumulatedRenderTransform();
	for (const f101::drawing::Fill& Fill : Plan->fills)
	{
		const FColor Color = InkColor(Fill.ink);
		TArray<FSlateVertex> Vertices;
		Vertices.Reserve(static_cast<int32>(Fill.outline.size()));
		for (const f101::cad::PlanPoint& Point : Fill.outline)
		{
			Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, FVector2f(ToLocal(Point)), FVector2f(0.5f, 0.5f), Color));
		}
		TArray<SlateIndex> Indices;
		Indices.Reserve(static_cast<int32>(Fill.triangles.size()));
		for (const std::uint32_t Index : Fill.triangles)
		{
			Indices.Add(static_cast<SlateIndex>(Index));
		}
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId + 1, Handle, Vertices, Indices, nullptr, 0, 0);
	}

	// Strokes: at least a pixel wide; hidden ones dashed.
	for (const f101::drawing::Stroke& Stroke : Plan->strokes)
	{
		TArray<FVector2D> Points;
		for (const f101::cad::PlanPoint& Point : Stroke.points)
		{
			Points.Add(ToLocal(Point));
		}
		if (Stroke.closed && !Points.IsEmpty())
		{
			const FVector2D First = Points[0]; // a copy: adding an element of the array to itself is not allowed
			Points.Add(First);
		}
		const FLinearColor Color(InkColor(Stroke.ink));
		const float Thickness = FMath::Max(1.0f, static_cast<float>(Stroke.widthMm * Map.Scale));
		if (!Stroke.dashed)
		{
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None,
				Color, true, Thickness);
			continue;
		}
		for (const TArray<FVector2D>& Dash : Dashes(Points, DashMm * Map.Scale, GapMm * Map.Scale))
		{
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(), Dash, ESlateDrawEffect::None,
				Color, true, Thickness);
		}
	}

	// Labels, centred and turned; too small to read ones are left out.
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	for (const f101::drawing::Label& Label : Plan->labels)
	{
		float FontSize = static_cast<float>(Label.heightMm * Map.Scale * FontSizePerCapHeight);
		if (Mapped.IsSet() && (Label.ink == Ink::Dimension || Label.ink == Ink::DimensionSelected))
			FontSize = FMath::Max(12.0f, FontSize); // legible on screen; exports retain their physical text size
		if (FontSize < MinFontSize)
		{
			continue;
		}
		const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(Label.ink == Ink::Label ? "Bold" : "Regular", FontSize);
		const FString Text = UTF8_TO_TCHAR(Label.text.c_str());
		const FVector2D Size = Measure->Measure(Text, Font);
		const FSlateRenderTransform Turn(FQuat2D(FMath::DegreesToRadians(static_cast<float>(-Label.angleDegrees))));
		FSlateDrawElement::MakeText(OutDrawElements, LayerId + 3,
			AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(ToLocal(Label.at) - Size / 2.0), Turn, FVector2D(0.5, 0.5)), Text, Font,
			ESlateDrawEffect::None, FLinearColor(InkColor(Label.ink)));
	}
	return LayerId + 3;
}
