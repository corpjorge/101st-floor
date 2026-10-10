#include "SFloor101Icon.h"

#include "Floor101Style.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"

void SFloor101Icon::Construct(const FArguments& Args)
{
	Icon = Args._Icon;
	Color = Args._Color;
	Size = Args._Size;
}

EFloor101Icon SFloor101Icon::ForMode(f101::cad::EditMode Mode)
{
	using f101::cad::EditMode;
	switch (Mode)
	{
	case EditMode::Wall: return EFloor101Icon::Wall;
	case EditMode::Door: return EFloor101Icon::Door;
	case EditMode::Window: return EFloor101Icon::Window;
	case EditMode::Column: return EFloor101Icon::Column;
	case EditMode::Services: return EFloor101Icon::Services;
	case EditMode::Furniture: return EFloor101Icon::Furniture;
	case EditMode::Paint: return EFloor101Icon::Paint;
	case EditMode::Dimension: return EFloor101Icon::Dimension;
	default: return EFloor101Icon::Select;
	}
}

int32 SFloor101Icon::OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Elements,
	int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const EFloor101Icon Kind = Icon.Get();
	const bool bEnabled = ShouldBeEnabled(bParentEnabled);
	const ESlateDrawEffect Effect = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FLinearColor Ink = Color.Get().GetColor(Style) * Style.GetColorAndOpacityTint();
	const float Grid = Kind == EFloor101Icon::Mark ? 24.0f : 20.0f;
	const float K = static_cast<float>(Geometry.GetLocalSize().X) / Grid;
	const float Stroke = FMath::Max(1.1f, 1.5f * K);

	auto Lines = [&](TArray<FVector2f> Points, bool bClosed = false)
	{
		if (bClosed && Points.Num() > 0) { const FVector2f First = Points[0]; Points.Add(First); }
		for (FVector2f& Point : Points) { Point *= K; }
		FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), Points, Effect, Ink, true, Stroke);
	};
	auto Arc = [&](float Cx, float Cy, float R, float From, float To)
	{
		const int32 Steps = FMath::Max(6, FMath::CeilToInt(FMath::Abs(To - From) / 12.0f));
		TArray<FVector2f> Points;
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			const float Angle = FMath::DegreesToRadians(FMath::Lerp(From, To, static_cast<float>(Step) / Steps));
			Points.Add(FVector2f(Cx + R * FMath::Cos(Angle), Cy + R * FMath::Sin(Angle)));
		}
		Lines(MoveTemp(Points));
	};
	auto Circle = [&](float Cx, float Cy, float R) { Arc(Cx, Cy, R, 0.0f, 360.0f); };
	auto Rect = [&](float X, float Y, float W, float H) { Lines({{X, Y}, {X + W, Y}, {X + W, Y + H}, {X, Y + H}}, true); };
	auto Curve = [&](FVector2f A, FVector2f Control, FVector2f B)
	{
		TArray<FVector2f> Points;
		for (int32 Step = 0; Step <= 16; ++Step)
		{
			const float T = Step / 16.0f;
			Points.Add(A * (1 - T) * (1 - T) + Control * 2.0f * (1 - T) * T + B * T * T);
		}
		Lines(MoveTemp(Points));
	};
	auto Fill = [&](float X, float Y, float W, float H, const FLinearColor& Tint)
	{
		FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(FVector2f(W, H) * K, FSlateLayoutTransform(FVector2f(X, Y) * K)),
			Floor101::Studio::White(), Effect, Tint * Style.GetColorAndOpacityTint());
	};
	auto Star = [&](bool bFilled)
	{
		TArray<FVector2f> Points;
		for (int32 Index = 0; Index < 10; ++Index)
		{
			const float Angle = -UE_PI / 2.0f + Index * UE_PI / 5.0f;
			const float R = Index % 2 == 0 ? 7.4f : 3.3f;
			Points.Add(FVector2f(10.0f + R * FMath::Cos(Angle), 10.6f + R * FMath::Sin(Angle)));
		}
		if (bFilled && FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
		{
			TArray<FSlateVertex> Vertices;
			TArray<SlateIndex> Indices;
			const FColor Vertex = Ink.ToFColorSRGB();
			Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(), FVector2f(10.0f, 10.6f) * K, FVector2f(0.5f), Vertex));
			for (const FVector2f& Point : Points)
			{
				Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(), Point * K, FVector2f(0.5f), Vertex));
			}
			for (int32 Index = 0; Index < 10; ++Index)
			{
				Indices.Append({0, static_cast<SlateIndex>(1 + Index), static_cast<SlateIndex>(1 + (Index + 1) % 10)});
			}
			const FSlateResourceHandle Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Floor101::Studio::White());
			FSlateDrawElement::MakeCustomVerts(Elements, Layer, Resource, Vertices, Indices, nullptr, 0, 0);
		}
		Lines(MoveTemp(Points), true);
	};

	switch (Kind)
	{
	case EFloor101Icon::Mark:
	{
		// A tower whose top floor is lit: the 101st floor.
		Fill(5.0f, 2.75f, 14.0f, 4.25f, Floor101::Studio::Palette().Brass);
		Rect(5.0f, 2.75f, 14.0f, 18.5f);
		Lines({{5.0f, 11.0f}, {19.0f, 11.0f}});
		Lines({{5.0f, 15.0f}, {19.0f, 15.0f}});
		Lines({{10.5f, 21.25f}, {10.5f, 18.0f}, {13.5f, 18.0f}, {13.5f, 21.25f}});
		break;
	}
	case EFloor101Icon::Select: Lines({{5, 3}, {5, 15.5f}, {8.4f, 12.4f}, {10.6f, 17}, {12.6f, 16.1f}, {10.4f, 11.6f}, {15, 11.4f}}, true); break;
	case EFloor101Icon::Wall:
		Lines({{2, 7}, {18, 7}}); Lines({{2, 13}, {18, 13}});
		Lines({{3.5f, 13}, {9.5f, 7}}); Lines({{8.5f, 13}, {14.5f, 7}}); Lines({{13.5f, 13}, {18, 8.5f}});
		break;
	case EFloor101Icon::Door:
		Lines({{1.5f, 16.5f}, {4, 16.5f}}); Lines({{15, 16.5f}, {18.5f, 16.5f}}); Lines({{4, 16.5f}, {4, 5.5f}});
		Arc(4, 16.5f, 11, -90, 0);
		break;
	case EFloor101Icon::Window:
		Lines({{1.5f, 7}, {5, 7}, {5, 13}, {1.5f, 13}}); Lines({{18.5f, 7}, {15, 7}, {15, 13}, {18.5f, 13}});
		Lines({{5, 7}, {15, 7}}); Lines({{5, 13}, {15, 13}}); Lines({{5, 10}, {15, 10}});
		break;
	case EFloor101Icon::Column: Rect(5, 5, 10, 10); Lines({{5, 5}, {15, 15}}); Lines({{15, 5}, {5, 15}}); break;
	case EFloor101Icon::Services:
		Lines({{2.5f, 15.5f}, {5.8f, 15.5f}}); Lines({{7.5f, 13.8f}, {7.5f, 8.2f}}); Lines({{9.2f, 6.5f}, {17.5f, 6.5f}});
		Circle(7.5f, 15.5f, 1.7f); Circle(7.5f, 6.5f, 1.7f); Lines({{17.5f, 4.5f}, {17.5f, 8.5f}});
		break;
	case EFloor101Icon::Furniture:
	case EFloor101Icon::Model:
		Lines({{4.5f, 9}, {4.5f, 5}, {15.5f, 5}, {15.5f, 9}});
		Lines({{3, 9}, {6, 9}, {6, 12.5f}, {14, 12.5f}, {14, 9}, {17, 9}, {17, 14.5f}, {3, 14.5f}}, true);
		Lines({{4.5f, 14.5f}, {4.5f, 16.5f}}); Lines({{15.5f, 14.5f}, {15.5f, 16.5f}});
		break;
	case EFloor101Icon::Paint:
		Rect(3, 3, 8, 14); Lines({{11, 6.2f}, {15.4f, 7.8f}, {16.3f, 9.7f}, {13.3f, 17}}); Circle(7, 13.5f, 1.2f);
		break;
	case EFloor101Icon::Dimension:
		Lines({{3, 5}, {3, 15}}); Lines({{17, 5}, {17, 15}}); Lines({{3, 10}, {17, 10}});
		Lines({{1.6f, 11.4f}, {4.4f, 8.6f}}); Lines({{15.6f, 11.4f}, {18.4f, 8.6f}});
		break;
	case EFloor101Icon::Undo:
		Lines({{7, 4.5f}, {3.5f, 8}, {7, 11.5f}}); Lines({{3.5f, 8}, {12, 8}}); Arc(12, 12.5f, 4.5f, -90, 90); Lines({{12, 17}, {9, 17}});
		break;
	case EFloor101Icon::Redo:
		Lines({{13, 4.5f}, {16.5f, 8}, {13, 11.5f}}); Lines({{16.5f, 8}, {8, 8}}); Arc(8, 12.5f, 4.5f, 270, 90); Lines({{8, 17}, {11, 17}});
		break;
	case EFloor101Icon::Save:
		Lines({{10, 3}, {10, 12}}); Lines({{6.5f, 8.5f}, {10, 12}, {13.5f, 8.5f}}); Lines({{3.5f, 13}, {3.5f, 17}, {16.5f, 17}, {16.5f, 13}});
		break;
	case EFloor101Icon::Options:
		Lines({{3, 6}, {11, 6}}); Lines({{15, 6}, {17, 6}}); Lines({{3, 14}, {5, 14}}); Lines({{9, 14}, {17, 14}});
		Circle(13, 6, 2); Circle(7, 14, 2);
		break;
	case EFloor101Icon::Exit:
		Lines({{8, 3.5f}, {4, 3.5f}, {4, 16.5f}, {8, 16.5f}}); Lines({{12, 6.5f}, {15.5f, 10}, {12, 13.5f}}); Lines({{15.5f, 10}, {8, 10}});
		break;
	case EFloor101Icon::Design: Lines({{4, 16.5f}, {4, 3.5f}, {17, 16.5f}}, true); Lines({{7, 13.5f}, {7, 9.9f}, {10.6f, 13.5f}}, true); break;
	case EFloor101Icon::Walk:
		Circle(11.5f, 3.6f, 1.6f); Lines({{10.6f, 6.6f}, {8.6f, 12}, {11.6f, 14.2f}, {12.5f, 17.8f}});
		Lines({{9.4f, 9}, {6.2f, 10.4f}}); Lines({{10.6f, 6.6f}, {13.5f, 9.8f}, {16.1f, 10.4f}}); Lines({{8.6f, 12}, {6.6f, 17}});
		break;
	case EFloor101Icon::Photo:
		Rect(2.5f, 6, 15, 10.5f); Lines({{7, 6}, {8.2f, 4}, {11.8f, 4}, {13, 6}}); Circle(10, 11.2f, 2.9f);
		break;
	case EFloor101Icon::Frame:
		Lines({{3, 7}, {3, 4}, {6, 4}}); Lines({{14, 4}, {17, 4}, {17, 7}}); Lines({{17, 13}, {17, 16}, {14, 16}}); Lines({{6, 16}, {3, 16}, {3, 13}});
		Rect(7, 7.5f, 6, 5);
		break;
	case EFloor101Icon::Top:
		Lines({{10, 2.5f}, {10, 9}}); Lines({{7.2f, 6.2f}, {10, 9}, {12.8f, 6.2f}}); Lines({{2.5f, 13.5f}, {10, 11}, {17.5f, 13.5f}, {10, 16}}, true);
		break;
	case EFloor101Icon::Plan:
		Rect(2.5f, 3.5f, 15, 13); Lines({{10, 3.5f}, {10, 8.5f}}); Lines({{10, 12}, {10, 16.5f}}); Lines({{2.5f, 10}, {7, 10}});
		break;
	case EFloor101Icon::Xray:
	{
		const FVector2f Corners[] = {{10, 3}, {17.5f, 6.6f}, {10, 10.2f}, {2.5f, 6.6f}};
		for (int32 Side = 0; Side < 4; ++Side)
		{
			const FVector2f A = Corners[Side];
			const FVector2f B = Corners[(Side + 1) % 4];
			const float Length = (B - A).Size();
			for (float At = 0.0f; At < Length; At += 4.2f)
			{
				Lines({A + (B - A) * (At / Length), A + (B - A) * (FMath::Min(Length, At + 2.2f) / Length)});
			}
		}
		Lines({{2.5f, 10.6f}, {10, 14.2f}, {17.5f, 10.6f}}); Lines({{2.5f, 14}, {10, 17.6f}, {17.5f, 14}});
		break;
	}
	case EFloor101Icon::Levels:
		Lines({{10, 3}, {17.5f, 6.6f}, {10, 10.2f}, {2.5f, 6.6f}}, true);
		Lines({{2.5f, 10.6f}, {10, 14.2f}, {17.5f, 10.6f}}); Lines({{2.5f, 14}, {10, 17.6f}, {17.5f, 14}});
		break;
	case EFloor101Icon::Eye: Curve({2, 10}, {10, 0}, {18, 10}); Curve({2, 10}, {10, 20}, {18, 10}); Circle(10, 10, 2.4f); break;
	case EFloor101Icon::ChevronDown:
	case EFloor101Icon::Down: Lines({{6, 8}, {10, 12}, {14, 8}}); break;
	case EFloor101Icon::ChevronRight: Lines({{8, 6}, {12, 10}, {8, 14}}); break;
	case EFloor101Icon::Up: Lines({{6, 12}, {10, 8}, {14, 12}}); break;
	case EFloor101Icon::Plus: Lines({{10, 4.5f}, {10, 15.5f}}); Lines({{4.5f, 10}, {15.5f, 10}}); break;
	case EFloor101Icon::Search: Circle(8.5f, 8.5f, 5); Lines({{12.2f, 12.2f}, {16.5f, 16.5f}}); break;
	case EFloor101Icon::Star: Star(false); break;
	case EFloor101Icon::StarFilled: Star(true); break;
	case EFloor101Icon::Close: Lines({{5.5f, 5.5f}, {14.5f, 14.5f}}); Lines({{14.5f, 5.5f}, {5.5f, 14.5f}}); break;
	case EFloor101Icon::Check: Lines({{4.5f, 10.5f}, {8.1f, 14.1f}, {15.5f, 6.5f}}); break;
	case EFloor101Icon::Warn: Lines({{10, 3}, {18, 17}, {2, 17}}, true); Lines({{10, 8.2f}, {10, 12}}); Lines({{10, 14.4f}, {10, 14.9f}}); break;
	case EFloor101Icon::Info: Circle(10, 10, 7.5f); Lines({{10, 9}, {10, 14}}); Lines({{10, 6.1f}, {10, 6.6f}}); break;
	case EFloor101Icon::Trash:
		Lines({{3.5f, 5.5f}, {16.5f, 5.5f}}); Lines({{8, 5.5f}, {8, 3.5f}, {12, 3.5f}, {12, 5.5f}}); Lines({{5.5f, 5.5f}, {6.3f, 16.5f}, {13.7f, 16.5f}, {14.5f, 5.5f}});
		break;
	case EFloor101Icon::Doc: Lines({{5, 2.5f}, {11.5f, 2.5f}, {15, 6}, {15, 17.5f}, {5, 17.5f}}, true); Lines({{11.5f, 2.5f}, {11.5f, 6}, {15, 6}}); break;
	case EFloor101Icon::Folder: Lines({{2.5f, 4}, {7.5f, 4}, {9.5f, 6}, {17.5f, 6}, {17.5f, 16}, {2.5f, 16}}, true); break;
	case EFloor101Icon::Export:
		Lines({{10, 12}, {10, 3}}); Lines({{6.5f, 6.5f}, {10, 3}, {13.5f, 6.5f}}); Lines({{4, 11}, {4, 16.5f}, {16, 16.5f}, {16, 11}});
		break;
	case EFloor101Icon::Grid:
		Rect(3, 3, 14, 14); Lines({{3, 7.7f}, {17, 7.7f}}); Lines({{3, 12.3f}, {17, 12.3f}}); Lines({{7.7f, 3}, {7.7f, 17}}); Lines({{12.3f, 3}, {12.3f, 17}});
		break;
	case EFloor101Icon::Flip:
		Lines({{4, 7}, {15, 7}}); Lines({{12, 4}, {15, 7}, {12, 10}}); Lines({{16, 13}, {5, 13}}); Lines({{8, 10}, {5, 13}, {8, 16}});
		break;
	case EFloor101Icon::Monitor: Rect(2.5f, 3.5f, 15, 10); Lines({{7, 17}, {13, 17}}); Lines({{10, 13.5f}, {10, 17}}); break;
	case EFloor101Icon::Sliders:
		Lines({{5, 3}, {5, 10.2f}}); Lines({{5, 13.8f}, {5, 17}}); Lines({{10, 3}, {10, 5.2f}}); Lines({{10, 8.8f}, {10, 17}});
		Lines({{15, 3}, {15, 11.2f}}); Lines({{15, 14.8f}, {15, 17}}); Circle(5, 12, 1.8f); Circle(10, 7, 1.8f); Circle(15, 13, 1.8f);
		break;
	case EFloor101Icon::Keys:
		Rect(2, 5, 16, 10); Lines({{5, 8}, {6, 8}}); Lines({{8, 8}, {9, 8}}); Lines({{11, 8}, {12, 8}}); Lines({{14, 8}, {15, 8}}); Lines({{6, 12}, {14, 12}});
		break;
	case EFloor101Icon::Pulse: Lines({{2, 10}, {5.5f, 10}, {7.5f, 5.5f}, {10.5f, 14.5f}, {12.5f, 10}, {18, 10}}); break;
	case EFloor101Icon::Rotate: Arc(10, 10.5f, 6, -180, 90); Lines({{12.5f, 14}, {10, 16.5f}, {12.5f, 19}}); break;
	default: break;
	}
	return Layer;
}
